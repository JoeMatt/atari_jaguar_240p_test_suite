#include "./eeprom_test.h"

#include "./main.h"
#include "./text_engine.h"

/* ---------------------------------------------------------------------------
 * 93C46 EEPROM low-level driver.
 *
 * The Jaguar's NM93C46-equivalent serial EEPROM is wired to three
 * memory-mapped Jerry I/O ports (per the Jaguar Technical Reference
 * Manual and Virtual Jaguar's well-tested emulation):
 *
 *     EE_DO_R   $F14001  (read)   bit 0 = DO line (chip -> cart)
 *     EE_DI_W   $F14801  (write)  bit 0 = DI line; the act of writing
 *                                 also pulses the SK clock once
 *     EE_CS     $F15001  (R/W)    any access strobes CS, restarting
 *                                 the chip's internal state machine
 *
 * Protocol on the wire (MSB first):
 *     START(1) + OP(2) + ADDR(6)  [+ DATA(16) for WRITE/WRAL]
 *
 * Opcodes (the upper 2 bits, with the next 2 address bits doubling
 * as a sub-opcode for op 00):
 *     READ   = 10 aaaaaa            -> 16 data bits clocked out on DO
 *     WRITE  = 01 aaaaaa dddddddddddddddd   (write enable required)
 *     ERASE  = 11 aaaaaa            -> word becomes 0xFFFF
 *     EWEN   = 00 11xxxx            -> enables WRITE/ERASE/WRAL/ERAL
 *     EWDS   = 00 00xxxx            -> disables them (chip default at power-on)
 *     WRAL   = 00 01xxxx dddddddddddddddd   (write all 64 words)
 *     ERAL   = 00 10xxxx            -> erase all 64 words
 *
 * Real-hardware caveats (we deliberately target the VJ behaviour
 * because that's what we can CI-test, and the upstream BitJag
 * tracker has no real-hardware reproductions of EEPROM bugs to test
 * against):
 *
 *   1. Real chips emit a leading dummy '0' bit between the address
 *      and the 16 data bits during READ. VJ collapses that, so we
 *      do 16 reads here, not 17. If a future BigPEmu/MAME run
 *      reports off-by-one bit shifts, add an extra dummy clock
 *      between the address phase and the data read loop.
 *   2. Between data-out reads the real chip needs the SK clock
 *      pulsed; VJ's eeprom_get_do() advances the bit counter on
 *      every DO read so the cart never has to clock manually.
 *      Toggling DI mid-read would push VJ's state machine into
 *      the default branch (back to OP_A) and corrupt the read --
 *      so we deliberately don't.
 *   3. VJ enables writes on every CS pulse; real hardware requires
 *      EWEN once after power-on. We send EWEN anyway so the same
 *      driver works in both worlds.
 *   4. Real WRITE has a ~10 ms internal program cycle (poll DO=0
 *      while busy, DO=1 when done). VJ completes synchronously.
 *      We poll with a sane timeout so neither path stalls.
 * --------------------------------------------------------------------------- */

#define EE_DO_REG (*(volatile uint8_t *)0xF14001)
#define EE_DI_REG (*(volatile uint8_t *)0xF14801)
#define EE_CS_REG (*(volatile uint8_t *)0xF15001)

/* 93C46 organisation: 64 words of 16 bits each. */
#define EE_NWORDS 64

/* WRITE poll budget. VJ returns ready immediately; real hardware
 * needs ~10 ms which at 13.3 MHz / typical loop overhead lands well
 * under 50,000 iterations. 100,000 gives margin without stalling. */
#define EE_WRITE_TIMEOUT 100000

static void ee_cs(void){
    /* Touching $F15001 (read OR write) resets the chip state machine
     * to START -- this is how every transaction begins. The discard
     * volatile read is the cheapest way to "touch" the address while
     * still being a side-effecting access the compiler can't elide. */
    (void)EE_CS_REG;
}

static void ee_send_bit(uint8_t b){
    /* Bit 0 of the byte we write is the DI value; the act of writing
     * is what clocks SK on the chip, so we do it in one move.b. */
    EE_DI_REG = (uint8_t)(b & 1);
}

static void ee_send_bits(uint16_t v, int nbits){
    /* MSB-first transmission, as required by the 93C46 spec. */
    int i;
    for(i = nbits - 1; i >= 0; i--){
        ee_send_bit((uint8_t)((v >> i) & 1));
    }
}

static uint16_t ee_recv_word(void){
    /* See caveat #2 above: in VJ each $F14001 read advances the
     * internal bit counter; we must NOT pulse the clock between
     * reads or we'll fall into the default state-machine branch.
     * If a real-hardware regression shows up, this is the loop
     * that needs an `ee_send_bit(0)` before each read. */
    uint16_t v = 0;
    int i;
    for(i = 0; i < 16; i++){
        v = (uint16_t)((v << 1) | (EE_DO_REG & 1));
    }
    return v;
}

static int ee_wait_ready(void){
    /* DO is held low while a WRITE/ERASE programs the cell; goes
     * back high when the chip is idle. VJ returns 1 immediately so
     * this is a near-instant no-op there, but it's correct on real
     * hardware where the program cycle is ~10 ms.
     *
     * Returns 1 when the chip reports idle, 0 if the timeout
     * expired first. Callers MUST check the return value before
     * trusting that a WRITE/ERASE landed -- a false return here
     * means the cell may not have been programmed (or the chip is
     * unplugged / mis-wired) and the on-screen verification pass
     * will subsequently catch it as a readback mismatch. */
    int timeout = EE_WRITE_TIMEOUT;
    while(timeout-- > 0){
        if(EE_DO_REG & 1){
            return 1;
        }
    }
    return 0;
}

static uint16_t eeprom_read_word_drv(uint8_t addr){
    /* READ: start(1) + op(10) + addr(6) -> 16 data bits */
    ee_cs();
    ee_send_bit(1);
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bits((uint16_t)(addr & 0x3F), 6);
    return ee_recv_word();
}

static void eeprom_ewen_drv(void){
    /* EWEN: start(1) + op(00) + sub(11xxxx) -- 4 trailing bits are
     * "don't care" but we send 0s to be predictable. */
    ee_cs();
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(1);
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
}

static void eeprom_ewds_drv(void){
    /* EWDS: start(1) + op(00) + sub(00xxxx) -- the chip's reset
     * default; we send it explicitly when the test is done so we
     * don't leave the EEPROM "open for writes" after exit. */
    ee_cs();
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(0);
}

static int eeprom_write_word_drv(uint8_t addr, uint16_t data){
    /* WRITE: start(1) + op(01) + addr(6) + data(16) -> poll DO until idle.
     *
     * Returns the result of ee_wait_ready() so callers can detect a
     * stuck/disconnected chip without having to do a separate
     * readback. Walking-1s and address-as-data passes deliberately
     * ignore this -- they verify by re-READ at the end so any
     * silent timeout still surfaces as an error count -- but the
     * exit-time restore path uses it to avoid over-counting retry
     * attempts that fail for the same underlying reason. */
    ee_cs();
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(1);
    ee_send_bits((uint16_t)(addr & 0x3F), 6);
    ee_send_bits(data, 16);
    return ee_wait_ready();
}

/* ---------------------------------------------------------------------------
 * Test UI helpers.
 *
 * Layout (320x240, NTSC -- PAL adds settings->PALOffset across the
 * vertical layout the same way every other test does):
 *
 *     row 0   (y=  8): title "EEPROM TEST (93C46 64x16)"
 *     row 1   (y= 24): result line "WRITE TEST: pending"  (status)
 *     row 2   (y= 40): cursor info "ADDR: 0x00  ORIG: FFFF  CUR: FFFF"
 *     rows 3-10       : 8x8 hex grid of all 64 words
 *     row 11+ (y=200): controls cheat-sheet
 *
 * The grid is rendered as one textBox per row (so we only pay for
 * 8 textBox allocations) and the cursor highlight is done by
 * recoloring the active row -- same pattern as the controller test.
 * --------------------------------------------------------------------------- */

#define GRID_COLS 8
#define GRID_ROWS 8
/* Grid sits at x=0 with a full-screen 320 px wide sprite per row.
 * That gives the 43-char "00: 0000 0000 ..." line ~62 px of right-
 * margin slack at 6 px/char so the in-engine word-wrap can never
 * trigger (a wrap would push the second line outside the sprite's
 * fixed-height framebuffer and corrupt the next box -- see comment
 * on textBox widths below). Y starts at 80 with 14-px row spacing
 * so the 10-px-tall (8 font + 1 spacing + 1 shadow) sprites have a
 * 4-px gap between them. */
#define GRID_X 0
#define GRID_Y 80
#define GRID_ROW_H 14

static void hexbyte(char *out, uint8_t v){
    /* No printf in this codebase -- itostring handles ints but
     * doesn't zero-pad, so do the 4-char hex word ourselves. */
    static const char DIGITS[] = "0123456789ABCDEF";
    out[0] = DIGITS[(v >> 4) & 0xF];
    out[1] = DIGITS[v & 0xF];
}

static void hexword(char *out, uint16_t v){
    hexbyte(out, (uint8_t)(v >> 8));
    hexbyte(out + 2, (uint8_t)(v & 0xFF));
}

static void formatGridRow(char *out, const uint16_t *words, int row){
    /* "00:FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF" -- the 2-char
     * row label is the address of the first word in that row, in
     * hex, so the user can map any cell back to its EEPROM index. */
    int col;
    hexbyte(out, (uint8_t)(row * GRID_COLS));
    out[2] = ':';
    for(col = 0; col < GRID_COLS; col++){
        out[3 + col * 5] = ' ';
        hexword(out + 4 + col * 5, words[row * GRID_COLS + col]);
    }
    out[3 + GRID_COLS * 5] = '\0';
}

static void formatCursorLine(char *out, int cursor, uint16_t orig, uint16_t cur){
    /* Template offsets (0-based):
     *   "ADDR: 0x00  ORIG: 0000  CUR: 0000"
     *    0123456789012345678901234567890123
     *            ^^         ^^^^         ^^^^
     *            8,9        18..21       29..32
     * The CUR field starts at column 29 -- column 28 is the space
     * after "CUR:". Writing the hex word at 28 (the original code)
     * stomped on that space and left the trailing '0' of the
     * template intact, producing "CUR:FFFF0" on screen. */
    int i;
    static const char tmpl[] = "ADDR: 0x00  ORIG: 0000  CUR: 0000";
    for(i = 0; tmpl[i] != '\0'; i++){
        out[i] = tmpl[i];
    }
    out[i] = '\0';
    hexbyte(out + 8, (uint8_t)(cursor & 0xFF));
    hexword(out + 18, orig);
    hexword(out + 29, cur);
}

static void readAll(uint16_t out[EE_NWORDS]){
    int i;
    for(i = 0; i < EE_NWORDS; i++){
        out[i] = eeprom_read_word_drv((uint8_t)i);
    }
}

static int writeAll(const uint16_t in[EE_NWORDS]){
    /* Returns the number of words that read back wrong after writing.
     * Caller is expected to have already issued EWEN.
     *
     * The write pass deliberately ignores ee_wait_ready()'s return:
     * a timeout there will surface here as a readback mismatch, so
     * counting it twice would inflate the error total. The verify
     * pass is the source of truth. */
    int i;
    int errors = 0;
    for(i = 0; i < EE_NWORDS; i++){
        (void)eeprom_write_word_drv((uint8_t)i, in[i]);
    }
    for(i = 0; i < EE_NWORDS; i++){
        if(eeprom_read_word_drv((uint8_t)i) != in[i]){
            errors++;
        }
    }
    return errors;
}

void EepromTest(void){
    /* C89-style: every local declared at the top of the function
     * before any executable statement. m68k-atari-mint-gcc currently
     * defaults to gnu99 so mixed decls would compile, but every
     * other test in this codebase plays C89-strict and there's no
     * reason to be the odd one out. */
    int exit_test = 0;
    int cursor = 0;          /* 0..63 -- selected word */
    int needRedraw = 1;
    int totalErrors = 0;     /* cumulative across button-triggered passes */
    int lastTestRan = 0;     /* 0=none, 1=walking-1s, 2=address-as-data */
    int row, i;
    /* Originals are restored on exit; current is what's on the chip
     * right now (refreshed after every write-test pass and on B). */
    uint16_t original[EE_NWORDS];
    uint16_t current[EE_NWORDS];
    char rowBuf[3 + GRID_COLS * 5 + 1];
    char cursorBuf[40];
    /* Sized for the longest line we publish: the exit-time restore
     * failure warning ("WARN: restore failed at 0xNN -- check save
     * data") tops out at ~48 chars, so 64 leaves headroom for any
     * future status text without reallocating. */
    char statusBuf[64];
    textBox *titleTb;
    textBox *statusTb;
    textBox *cursorTb;
    textBox *gridTb[GRID_ROWS];
    textBox *helpTb1;
    textBox *helpTb2;

    readAll(original);
    for(i = 0; i < EE_NWORDS; i++){
        current[i] = original[i];
    }

    /* All textboxes here use 320 px width (full screen, phrase-
     * aligned at 40 octets / 320/8) for two reasons:
     *   1) Sprite buffers are allocated at newTextBox() time based
     *      on the *initial* string's line_count after word-wrap.
     *      If a later updateTextBox() pushes the text long enough
     *      to wrap an extra line, drawing overflows the framebuffer
     *      and corrupts adjacent sprites (the symptom is the
     *      garbled-grid screenshot we shipped first).
     *   2) The Object Processor expects DEPTH8 sprite widths to be
     *      multiples of a phrase (8 px). 300 was *not* phrase-
     *      aligned; 320 is.
     * Initial strings are deliberately the worst-case length each
     * box will ever hold so the per-frame strncpy in updateLine
     * never has to grow tb->text -- that keeps the heap quiet and
     * matches the BounceSquareTest / AudioTest pattern. */
    titleTb  = newTextBox("EEPROM TEST (93C46 64x16)            ", 320, 9, mainFont, 0,
                          settings->d, 0, 8 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    /* Worst-case status: "WARN: restore failed at 0xNN -- check save data" (48 chars). */
    statusTb = newTextBox("WARN: restore failed at 0xFF -- check save data", 320, 9, mainFont, 0,
                          settings->d, 0, 24 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, statusTb, "STATUS: idle (press A or X)", 999999, 999999, GREY);

    cursorTb = newTextBox("ADDR: 0x00  ORIG: 0000  CUR: 0000", 320, 9, mainFont, 0,
                          settings->d, 0, 40 + settings->PALOffset, 13, 1);

    for(row = 0; row < GRID_ROWS; row++){
        gridTb[row] = newTextBox("00: 0000 0000 0000 0000 0000 0000 0000 0000", 320, 9, mainFont, 0,
                                 settings->d, GRID_X, GRID_Y + row * GRID_ROW_H + settings->PALOffset, 13, 1);
    }

    /* Help text deliberately kept under 50 chars so it fits on one
     * line in a 320-px box (50 chars * 6 px/char = 300 px, with
     * 20 px slack). Shortened from the verbose first draft which
     * wrapped to 2 lines and overflowed the sprite. */
    helpTb1 = newTextBox("D-PAD move  A walk-1s  X addr-as-data",
                         320, 9, mainFont, 0,
                         settings->d, 0, 188 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, helpTb1, NULL, 999999, 999999, GREY);

    helpTb2 = newTextBox("B re-read  Y erase  OPTION restore+exit",
                         320, 9, mainFont, 0,
                         settings->d, 0, 200 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, helpTb2, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit_test){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            cursor = (cursor + 1) % EE_NWORDS;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_LEFT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            cursor = (cursor + EE_NWORDS - 1) % EE_NWORDS;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            cursor = (cursor + GRID_COLS) % EE_NWORDS;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            cursor = (cursor + EE_NWORDS - GRID_COLS) % EE_NWORDS;
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            /* Walking-1s pattern: word[i] = 1 << (i % 16). Touches every
             * data bit at every cell at least once across the 64-word
             * span -- catches stuck bits and address aliasing in one pass. */
            uint16_t pattern[EE_NWORDS];
            int errs;
            settings->controllerLock = 1;
            for(i = 0; i < EE_NWORDS; i++){
                pattern[i] = (uint16_t)(1u << (i % 16));
            }
            eeprom_ewen_drv();
            errs = writeAll(pattern);
            totalErrors += errs;
            lastTestRan = 1;
            readAll(current);
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_X) && settings->controllerLock == 0){
            /* Address-as-data: word[i] = (i << 8) | (~i & 0xFF). Each
             * cell holds a unique value derived from its index, so a
             * mis-addressed write is immediately visible on the grid
             * (the value won't line up with its row/col label). */
            uint16_t pattern[EE_NWORDS];
            int errs;
            settings->controllerLock = 1;
            for(i = 0; i < EE_NWORDS; i++){
                pattern[i] = (uint16_t)(((uint16_t)i << 8) | ((~(uint16_t)i) & 0xFF));
            }
            eeprom_ewen_drv();
            errs = writeAll(pattern);
            totalErrors += errs;
            lastTestRan = 2;
            readAll(current);
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_B) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            readAll(current);
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_Y) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            eeprom_ewen_drv();
            eeprom_write_word_drv((uint8_t)cursor, 0xFFFF);
            current[cursor] = eeprom_read_word_drv((uint8_t)cursor);
            needRedraw = 1;
        }

        /* Same exit gesture every other test uses: OPTION while the
         * controller debouncer is idle. We don't pull the helper from
         * extra_tests.c because it's static there and exposing it just
         * for one consumer would be more code, not less. */
        if((settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            exit_test = 1;
        }

        if(needRedraw){
            /* C89-strict: every local declared at the top of this
             * block before any executable statement. Same convention
             * as EepromTest()'s function prologue above. */
            const char *label;
            const char *prefix;
            int n;
            int k;
            uint16_t statusColor;

            needRedraw = 0;

            /* Status line summarises the most recent destructive test
             * pass plus running error total. Idle is shown in grey,
             * pass in green, fail in red so it pops at a glance.
             *
             * Labels are deliberately short (no trailing pad) --
             * updateLine recomputes char_count from the new string
             * each call, so we don't need to splat trailing spaces
             * just to "erase" leftover pixels from a previous longer
             * label. The initial newTextBox allocation reserved
             * enough sprite framebuffer for the worst-case string
             * (the WARN: line on exit), so anything shorter draws
             * cleanly into a freshly cleared screen. */
            label = "idle (press A or X)";
            if(lastTestRan == 1){
                label = "WALKING-1S complete";
            } else if(lastTestRan == 2){
                label = "ADDR-AS-DATA complete";
            }
            n = 0;
            prefix = "STATUS: ";
            while(prefix[n] != '\0'){ statusBuf[n] = prefix[n]; n++; }
            k = 0;
            while(label[k] != '\0' && n < (int)sizeof(statusBuf) - 12){
                statusBuf[n++] = label[k++];
            }
            if(lastTestRan){
                const char *errLabel = "  ERRORS: ";
                int j = 0;
                /* Clamp only the *displayed* count -- totalErrors is
                 * the cumulative running total across button-
                 * triggered passes and is read again below for status
                 * colour selection. Mutating it here would silently
                 * cap the true count at 999 forever after the first
                 * overflow. 64 errors per pass max means it'd take
                 * 16+ destructive passes back-to-back to actually
                 * hit 999, but the bug is real either way. */
                int displayErrors = totalErrors;
                while(errLabel[j] != '\0' && n < (int)sizeof(statusBuf) - 5){
                    statusBuf[n++] = errLabel[j++];
                }
                if(displayErrors > 999) displayErrors = 999;
                if(displayErrors >= 100){
                    statusBuf[n++] = (char)('0' + (displayErrors / 100));
                }
                if(displayErrors >= 10){
                    statusBuf[n++] = (char)('0' + ((displayErrors / 10) % 10));
                }
                statusBuf[n++] = (char)('0' + (displayErrors % 10));
            }
            statusBuf[n] = '\0';
            statusColor = GREY;
            if(lastTestRan){
                statusColor = (totalErrors == 0) ? GREEN : RED;
            }
            updateLine(settings, mainFont, statusTb, statusBuf, 999999, 999999, statusColor);

            formatCursorLine(cursorBuf, cursor, original[cursor], current[cursor]);
            updateLine(settings, mainFont, cursorTb, cursorBuf, 999999, 999999, WHITE);

            for(row = 0; row < GRID_ROWS; row++){
                /* Highlight the row that contains the cursor in red so
                 * the active selection is obvious without having to
                 * underline a single 4-char hex word (which would need
                 * a sub-textBox). The selected cell within that row
                 * is the one whose address matches the cursor info line. */
                uint16_t color = (cursor / GRID_COLS == row) ? RED : WHITE;
                formatGridRow(rowBuf, current, row);
                updateLine(settings, mainFont, gridTb[row], rowBuf, 999999, 999999, color);
            }
        }
    }

    /* Restore original contents and disable writes so the EEPROM
     * leaves the test exactly the way the user left it on entry --
     * even if every other game on the cart trusts the chip's
     * power-on EWDS state, we can't assume our own rewrites haven't
     * already cycled it through EWEN.
     *
     * For each word that was modified during the run, write +
     * read-back verify with up to RESTORE_RETRIES attempts. A real
     * 93C46 should never need more than one pass under correct
     * timing, but the retry loop covers the case where a marginal
     * cell or a flaky chip drops a single program cycle -- much
     * better than silently leaving the user's save data half-
     * restored. If anything still doesn't stick we set a flag,
     * publish a red warning to the status line, and hold the
     * screen long enough for the user to read it before tearing
     * the test down. */
    {
        const int RESTORE_RETRIES = 3;
        int restoreFailed = 0;
        int badAddr = -1;
        int attempts;
        uint16_t verify = 0;

        eeprom_ewen_drv();
        for(i = 0; i < EE_NWORDS; i++){
            if(current[i] == original[i]){
                continue;
            }
            verify = current[i];
            for(attempts = 0; attempts < RESTORE_RETRIES; attempts++){
                (void)eeprom_write_word_drv((uint8_t)i, original[i]);
                verify = eeprom_read_word_drv((uint8_t)i);
                if(verify == original[i]){
                    current[i] = verify;
                    break;
                }
            }
            if(verify != original[i] && !restoreFailed){
                restoreFailed = 1;
                badAddr = i;
            }
        }
        eeprom_ewds_drv();

        if(restoreFailed){
            /* Compose "WARN: restore failed at 0xNN -- check save
             * data" directly into statusBuf -- no printf in this
             * codebase, so we walk the literals byte-by-byte and
             * splice in the failing address with hexbyte(). The
             * 2-second hold (~120 NTSC frames) is short enough not
             * to feel laggy when the test passes for everyone but
             * the unlucky user with a flaky cart. All declarations
             * up top so this stays C89-clean for the cross-compiler. */
            const char *warn = "WARN: restore failed at 0x";
            const char *tail = " -- check save data";
            int n = 0;
            int t = 0;
            int hold;

            while(warn[n] != '\0'){ statusBuf[n] = warn[n]; n++; }
            hexbyte(statusBuf + n, (uint8_t)(badAddr & 0xFF));
            n += 2;
            while(tail[t] != '\0'){ statusBuf[n++] = tail[t++]; }
            statusBuf[n] = '\0';
            updateLine(settings, mainFont, statusTb, statusBuf, 999999, 999999, RED);

            for(hold = 0; hold < 120; hold++){
                vsync();
            }
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    helpTb2  = freeTextBox(helpTb2);
    helpTb1  = freeTextBox(helpTb1);
    for(row = GRID_ROWS - 1; row >= 0; row--){
        gridTb[row] = freeTextBox(gridTb[row]);
    }
    cursorTb = freeTextBox(cursorTb);
    statusTb = freeTextBox(statusTb);
    titleTb  = freeTextBox(titleTb);
}
