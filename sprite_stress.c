#include "./sprite_stress.h"

#include "./main.h"
#include "./text_engine.h"

/* ---------------------------------------------------------------------------
 * Sprite Stress Test internals
 *
 * The Jaguar OP processes a per-line display list whose total cost
 * is dominated by two things:
 *
 *   1. Per-object header fetch & parse (constant per object)
 *   2. Per-object pixel-data fetch (bytes = sprite-width-in-bytes,
 *      i.e. width-in-px for DEPTH8)
 *
 * For DEPTH8 sprites at PWIDTH4 the total per-scanline data-fetch
 * budget is ~8 KiB (varies slightly with PIT/clut access pressure
 * and BG/border colour fills). MAX_SPRITES is chosen so that the
 * worst-case 32x32 single-line configuration sits right on top of
 * that ceiling: 256 * 32 = 8192 bytes/line of sprite pixel data,
 * which is enough to drive the OP into visible drop-out on real
 * hardware without overflowing rmvlib's display-list buffers. Smaller
 * sprite sizes give a larger headroom -- 256 * 8 = 2048 bytes/line
 * for 8x8 -- which is useful for separating "header-fetch" cost
 * from "pixel-fetch" cost when comparing emulators against hardware.
 * --------------------------------------------------------------------------- */

/* OP layer for the stress sprites. HardwareMenu hides every layer
 * (0..15) before launching a test, then we re-show layers 3..15 once
 * our textboxes are attached so the test owns its own layer band.
 * Layer 12 keeps the stress sprites well clear of layer 13
 * (textboxes) so the OP walks them as a contiguous block. */
#define STRESS_LAYER     12

/* Hard cap on the number of sprite objects in our pool. 256 was
 * picked to comfortably exceed OP saturation in every (size, mode)
 * combination this test exposes -- enough headroom to characterise
 * the breakdown without risking display-list buffer overflow inside
 * rmvlib. Don't raise it without re-verifying on real hardware. */
#define MAX_SPRITES      256

/* Three sprite-size variants. Each width is one phrase (8 px) or
 * a small multiple thereof, which is what DEPTH8 OP objects require.
 * Width-in-bytes equals width-in-px because depth is 1 byte/px. */
#define SIZE_COUNT       3
static const int kSpriteSizes[SIZE_COUNT] = {8, 16, 32};

/* CLUT index used to fill the stress sprite pixel buffer. The Jaguar
 * menu loads sonicPal into clut1 at boot; index 0x06 is "highlight
 * pink" in that palette. We override it to white on entry and
 * restore the original on exit so the menu paints normally when we
 * return. */
#define STRESS_CLUT_IDX  0x06

/* Vertical band the stress sprites live in. Title / status / mode
 * lines occupy y=8..56; help lines occupy y=200..220. Anything in
 * between is fair game; in TILED mode we cap the lowest sprite at
 * y=184 so the bottom row never bleeds into the help text. */
#define STRESS_Y0        80
#define STRESS_Y_MAX     184

/* Helper: append the decimal representation of |value| to out[*pos].
 * No printf in this codebase, and we want the C89-strict no-mid-
 * declaration discipline, so this is the cheapest readable path. */
static int appendDec(char *out, int pos, int value){
    char digits[8];
    int dlen = 0;
    int v = value;
    if(v < 0){
        out[pos++] = '-';
        v = -v;
    }
    if(v == 0){
        digits[dlen++] = '0';
    } else {
        while(v > 0 && dlen < (int)sizeof(digits)){
            digits[dlen++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while(dlen-- > 0){
        out[pos++] = digits[dlen];
    }
    return pos;
}

static int appendStr(char *out, int pos, const char *s){
    int j = 0;
    while(s[j] != '\0'){
        out[pos++] = s[j++];
    }
    return pos;
}

void SpriteStressTest(void){
    /* C89 strict: every local declared at the top of the function
     * before any executable statement. Same convention as
     * EepromTest() / every other test in this codebase. */
    int exit_test = 0;
    int needRedraw = 1;
    int needRebuild = 1;
    int spriteCount = 1;
    int sizeIdx = 0;            /* index into kSpriteSizes */
    int activeSize = 8;         /* px; mirrors kSpriteSizes[sizeIdx] */
    int singleLineMode = 1;     /* 1 = stack on one Y, 0 = tiled */
    int i;
    uint16_t savedClut;
    uint8_t *spriteData;
    sprite *pool[MAX_SPRITES];
    char status1Buf[64];
    char status2Buf[64];
    textBox *titleTb;
    textBox *statusTb;
    textBox *modeTb;
    textBox *helpTb1;
    textBox *helpTb2;

    for(i = 0; i < MAX_SPRITES; i++){
        pool[i] = NULL;
    }

    /* Repurpose CLUT[STRESS_CLUT_IDX] for the duration of the test
     * -- we have to set it to *something* visible because the menu's
     * sonicPal entry there is opaque-pink, which would defeat the
     * "is the OP drawing this sprite or not?" eyeball check.
     * Restored verbatim on exit. */
    savedClut = TOMREGS->clut1[STRESS_CLUT_IDX];
    TOMREGS->clut1[STRESS_CLUT_IDX] = (uint16_t)((31u << 11) | (31u << 6) | 63u);

    /* One shared DEPTH8 byte buffer, sized for the largest variant
     * (32x32 = 1024 bytes). 8x8 and 16x16 sprites read sub-regions
     * of the same buffer per their declared (iwidth, height) -- the
     * fill is uniform so it doesn't matter where they start. malloc
     * on m68k aligns to >= 4 bytes; rmvlib's DEPTH8 OP path doesn't
     * require strict 8-byte (phrase) alignment of the data pointer
     * itself, only that the row stride is a multiple of one phrase. */
    spriteData = malloc(sizeof(uint8_t) * 32 * 32);
    memset(spriteData, STRESS_CLUT_IDX, 32 * 32);

    /* All textboxes use the full-screen 320 px width (40 phrases,
     * phrase-aligned for DEPTH8 as required by the OP) and are
     * initialised with their worst-case string so the underlying
     * sprite framebuffer is sized once at newTextBox() time. Any
     * subsequent updateLine() that writes a longer string would
     * overflow the framebuffer and corrupt the next textBox's
     * sprite -- see the EEPROM Test render-corruption fix in PR #10
     * for the cautionary tale. */
    titleTb = newTextBox("SPRITE STRESS TEST (OP SATURATION)",
                         320, 9, mainFont, 0, settings->d,
                         0, 8 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    /* Worst case: "SPRITES: 256 / 256   BYTES/LINE: 8192" (38 chars). */
    statusTb = newTextBox("SPRITES: 256 / 256   BYTES/LINE: 8192",
                          320, 9, mainFont, 0, settings->d,
                          0, 24 + settings->PALOffset, 13, 1);

    /* Worst case: "MODE: SINGLE LINE   SIZE: 32x32 (1024 b/spr)" (44 chars). */
    modeTb = newTextBox("MODE: SINGLE LINE   SIZE: 32x32 (1024 b/spr)",
                        320, 9, mainFont, 0, settings->d,
                        0, 40 + settings->PALOffset, 13, 1);

    helpTb1 = newTextBox("L/R: -/+1  U/D: +/-8  A: SIZE  B: MODE",
                         320, 9, mainFont, 0, settings->d,
                         0, 200 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, helpTb1, NULL, 999999, 999999, GREY);

    helpTb2 = newTextBox("C: RESET to 1  OPTION: EXIT",
                         320, 9, mainFont, 0, settings->d,
                         0, 212 + settings->PALOffset, 13, 1);
    updateLine(settings, mainFont, helpTb2, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit_test){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        /* DOWN + OPTION = in-test help. Checked first so it consumes
         * the controllerLock before the standalone DOWN handler can
         * fire its -8 decrement on the same frame. Same gesture as
         * the patterns / video tests use throughout this codebase. */
        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION))
                && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_SPRITE_STRESS);
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(spriteCount < MAX_SPRITES){
                spriteCount++;
                needRebuild = 1;
                needRedraw = 1;
            }
        }
        if((settings->joy1 & JOYPAD_LEFT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(spriteCount > 0){
                spriteCount--;
                needRebuild = 1;
                needRedraw = 1;
            }
        }
        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            spriteCount += 8;
            if(spriteCount > MAX_SPRITES) spriteCount = MAX_SPRITES;
            needRebuild = 1;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            spriteCount -= 8;
            if(spriteCount < 0) spriteCount = 0;
            needRebuild = 1;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            sizeIdx = (sizeIdx + 1) % SIZE_COUNT;
            activeSize = kSpriteSizes[sizeIdx];
            needRebuild = 1;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_B) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            singleLineMode = !singleLineMode;
            needRebuild = 1;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_C) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            spriteCount = 1;
            needRebuild = 1;
            needRedraw = 1;
        }
        if((settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            exit_test = 1;
        }

        if(needRebuild){
            /* Free + recreate the entire sprite pool whenever the
             * count, size, or mode changes. Sprite dimensions are
             * baked into sprite_header.iwidth / dwidth / height by
             * new_sprite() and rmvlib offers no public API to mutate
             * them on a live sprite, so a rebuild is the only way to
             * change size. The cost is paid once per button press,
             * not per frame, so heap churn stays bounded. */
            int s;
            int cols;
            int sx;
            int sy;

            needRebuild = 0;

            for(s = 0; s < MAX_SPRITES; s++){
                if(pool[s] != NULL){
                    pool[s]->invisible = 1;
                    detach_sprite_from_display(pool[s]);
                    free(pool[s]);
                    pool[s] = NULL;
                }
            }

            cols = 320 / activeSize;
            if(cols < 1) cols = 1;

            for(s = 0; s < spriteCount; s++){
                if(singleLineMode){
                    /* All sprites pinned to one Y -- this is the
                     * worst-case OP per-scanline cost: every header
                     * is examined and every sprite contributes
                     * activeSize bytes of pixel-fetch to the same
                     * scanline. X wraps at the screen edge so the
                     * count is still visually obvious as the row
                     * fills up; once it overflows you're stacking
                     * sprites on top of each other but the OP is
                     * still doing all the work. */
                    sx = (s % cols) * activeSize;
                    sy = STRESS_Y0;
                } else {
                    /* Spread across the active band. Per-scanline
                     * cost is now bounded by `cols` (number of
                     * sprites that fit horizontally), so the OP only
                     * walks `min(spriteCount, cols)` headers per
                     * line. Useful for diffing per-header overhead
                     * vs per-pixel-fetch overhead between hardware
                     * and emulators. */
                    sx = (s % cols) * activeSize;
                    sy = STRESS_Y0 + (s / cols) * activeSize;
                    if(sy > STRESS_Y_MAX - activeSize){
                        sy = STRESS_Y_MAX - activeSize;
                    }
                }
                pool[s] = new_sprite(activeSize, activeSize,
                                     sx, sy + settings->PALOffset,
                                     DEPTH8, (phrase*)spriteData);
                /* trans=0 to match every other DEPTH8 stress
                 * sprite in this project (controller_test, LED
                 * test). With trans=1 the OP would punch holes
                 * through any pixels that happen to equal the magic
                 * transparent index, which would invalidate the
                 * pixel-fetch budget estimate. */
                pool[s]->trans = 0;
                attach_sprite_to_display_at_layer(pool[s], settings->d, STRESS_LAYER);
            }
        }

        if(needRedraw){
            /* C89-strict: every local declared at the top of this
             * block before any executable statement. */
            int bytesPerLine;
            int cols;
            int n;
            const char *modeLabel;
            const char *sizeLabel;

            needRedraw = 0;

            cols = 320 / activeSize;
            if(cols < 1) cols = 1;

            /* Pessimistic per-scanline data-fetch estimate:
             *   - SINGLE-LINE: every sprite touches the same row,
             *     so bytes/line = spriteCount * activeSize.
             *   - TILED: at most `cols` sprites touch a given row,
             *     so bytes/line = min(spriteCount, cols) * activeSize.
             * This is *just* the pixel-data fetch -- it doesn't
             * include OP header overhead (~8 bytes/object that the
             * GPU walks regardless of size). The number is meant as
             * a rough "are we near the ~8 KiB/line ceiling?" gauge,
             * not a precise cycle count. */
            if(singleLineMode){
                bytesPerLine = spriteCount * activeSize;
            } else {
                int active = (spriteCount < cols) ? spriteCount : cols;
                bytesPerLine = active * activeSize;
            }

            n = 0;
            n = appendStr(status1Buf, n, "SPRITES: ");
            n = appendDec(status1Buf, n, spriteCount);
            n = appendStr(status1Buf, n, " / 256   BYTES/LINE: ");
            n = appendDec(status1Buf, n, bytesPerLine);
            status1Buf[n] = '\0';
            updateLine(settings, mainFont, statusTb, status1Buf, 999999, 999999, WHITE);

            modeLabel = singleLineMode ? "SINGLE LINE" : "TILED";
            switch(sizeIdx){
                case 0:  sizeLabel = "8x8 (64 b/spr)";    break;
                case 1:  sizeLabel = "16x16 (256 b/spr)"; break;
                default: sizeLabel = "32x32 (1024 b/spr)"; break;
            }
            n = 0;
            n = appendStr(status2Buf, n, "MODE: ");
            n = appendStr(status2Buf, n, modeLabel);
            n = appendStr(status2Buf, n, "   SIZE: ");
            n = appendStr(status2Buf, n, sizeLabel);
            status2Buf[n] = '\0';
            updateLine(settings, mainFont, modeTb, status2Buf, 999999, 999999, GREY);
        }
    }

    /* Hide layers FIRST so the OP stops walking the stress display
     * list before we mutate it. patterns.c:38-43 and eeprom_test.c:621-629
     * both follow the same hide-then-detach order -- doing it the other
     * way around lets the OP race against detach_sprite_from_display()
     * and can spray garbage on the way out. */
    hide_or_show_display_layer_range(settings->d, 0, 3, 15);

    for(i = 0; i < MAX_SPRITES; i++){
        if(pool[i] != NULL){
            pool[i]->invisible = 1;
            detach_sprite_from_display(pool[i]);
            free(pool[i]);
            pool[i] = NULL;
        }
    }
    free(spriteData);

    TOMREGS->clut1[STRESS_CLUT_IDX] = savedClut;

    helpTb2  = freeTextBox(helpTb2);
    helpTb1  = freeTextBox(helpTb1);
    modeTb   = freeTextBox(modeTb);
    statusTb = freeTextBox(statusTb);
    titleTb  = freeTextBox(titleTb);
}
