#include "./extra_tests.h"
#include "./tests.h" /* for the C1..C9 frequency constants used by audio */

/* ---------------------------------------------------------------------------
 * Helpers
 *
 * Jaguar TOM RGB16 video mode + DEPTH16 framebuffers pack pixels as
 * R5 B5 G6 in a single 16-bit word: (red<<11) | (blue<<6) | green
 * with red/blue 0..31 and green 0..63. This matches the existing patterns
 * (Draw100IRE, DrawWhiteScreen, DropShadowTest, etc).
 *
 * NOTE: this macro is named PACK_RGB16 (not RGB16) because the SDK already
 * uses the bare identifier RGB16 as a TOM vmode bitflag (e.g. main.c does
 * `TOMREGS->vmode = RGB16 | CSYNC | BGEN | PWIDTH4 | VIDEN`). Reusing the
 * name for a pixel-pack macro would mask the SDK constant inside this TU
 * and trigger -Wmacro-redefined.
 * --------------------------------------------------------------------------- */

#define PACK_RGB16(r, b, g) (uint16_t)( ((uint16_t)((r) & 0x1F) << 11) | ((uint16_t)((b) & 0x1F) << 6) | (uint16_t)((g) & 0x3F) )

#define COLOR_BLACK   PACK_RGB16(0,  0,  0)
#define COLOR_WHITE   PACK_RGB16(31, 31, 63)
#define COLOR_GRAY50  PACK_RGB16(16, 16, 32)
#define COLOR_GRAY25  PACK_RGB16(8,  8,  16)
#define COLOR_RED     PACK_RGB16(31, 0,  0)
#define COLOR_GREEN   PACK_RGB16(0,  0,  63)
#define COLOR_BLUE    PACK_RGB16(0,  31, 0)
#define COLOR_YELLOW  PACK_RGB16(31, 0,  63)
#define COLOR_CYAN    PACK_RGB16(0,  31, 63)
#define COLOR_MAGENTA PACK_RGB16(31, 31, 0)

/* Fill a DEPTH16 RGB16 framebuffer of (w*h) pixels with a constant color. */
static void fillPACK_RGB16(uint16_t *buf, int count, uint16_t color){
    int i;
    for(i = 0; i != count; i++){
        buf[i] = color;
    }
}

/* Draw an axis-aligned filled rectangle into a DEPTH16 framebuffer.
 * No clipping -- caller must keep the rect inside the buffer. */
static void rectPACK_RGB16(uint16_t *buf, int stride, int x, int y, int w, int h, uint16_t color){
    int row, col;
    for(row = 0; row < h; row++){
        uint16_t *line = buf + (y + row) * stride + x;
        for(col = 0; col < w; col++){
            line[col] = color;
        }
    }
}

/* Standard "press OPTION to exit" loop body shared by every static pattern. */
static int extraExitPressed(void){
    return ((settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0);
}

/* Standard sprite teardown for procedural full-screen patterns. */
static void teardownFullscreenSprite(sprite *s, void *data){
    if(s != NULL){
        s->invisible = 1;
        detach_sprite_from_display(s);
        free(s);
    }
    if(data != NULL){
        free(data);
    }
}

/* ---------------------------------------------------------------------------
 * Test pattern: Color Bars with Gray Scale
 *
 * Inspired by Digital Video Essentials / SMPTE color bars overlay used in the
 * Genesis / SNES versions: the top 2/3 of the screen shows the seven 75% IRE
 * primary bars (gray, yellow, cyan, green, magenta, red, blue) on top of an
 * 11-step gray scale strip in the bottom 1/3, so you can compare each color
 * channel against neutral gray with color filters.
 * --------------------------------------------------------------------------- */
void DrawColorBarsGray(void){
    const int W = 320, H = 240;
    static const uint16_t bars75[7] = {
        PACK_RGB16(24, 24, 48), /* 75% gray   */
        PACK_RGB16(24, 0,  48), /* 75% yellow */
        PACK_RGB16(0,  24, 48), /* 75% cyan   */
        PACK_RGB16(0,  0,  48), /* 75% green  */
        PACK_RGB16(24, 24, 0),  /* 75% magenta*/
        PACK_RGB16(24, 0,  0),  /* 75% red    */
        PACK_RGB16(0,  24, 0)   /* 75% blue   */
    };

    int exit = 0;
    int i, x;
    int barW = W / 7;
    int barsH = (H * 2) / 3;
    int rampH = H - barsH;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_BLACK);

    /* 75% color bars in the top 2/3 */
    for(i = 0; i < 7; i++){
        int x0 = i * barW;
        int w  = (i == 6) ? (W - x0) : barW;
        rectPACK_RGB16(buf, W, x0, 0, w, barsH, bars75[i]);
    }

    /* 11-step gray ramp across the bottom 1/3 */
    int stepW = W / 11;
    for(i = 0; i < 11; i++){
        x = i * stepW;
        int w = (i == 10) ? (W - x) : stepW;
        int g6 = (i * 63) / 10;     /* 0..63   */
        int rb5 = (i * 31) / 10;    /* 0..31   */
        uint16_t c = PACK_RGB16(rb5, rb5, g6);
        rectPACK_RGB16(buf, W, x, barsH, w, rampH, c);
    }

    sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_BARS);
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Test pattern: Linearity
 *
 * Concentric circles + a uniform grid of equal-area squares, the canonical
 * tool for diagnosing CRT deflection linearity (the squares should look
 * square in every region of the tube, and the circles round). Distinct from
 * the existing Grid pattern, which is a simple line grid for overscan.
 * --------------------------------------------------------------------------- */
void DrawLinearity(void){
    const int W = 320, H = 240;
    const int STEP = 16;     /* 20x15 grid of 16-pixel squares */
    const int CX = W/2, CY = H/2;
    int exit = 0;
    int x, y, r, dx, dy;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_BLACK);

    /* Vertical rules */
    for(x = 0; x <= W; x += STEP){
        int xc = (x == W) ? W - 1 : x;
        for(y = 0; y < H; y++){
            buf[y * W + xc] = COLOR_WHITE;
        }
    }
    /* Horizontal rules */
    for(y = 0; y <= H; y += STEP){
        int yc = (y == H) ? H - 1 : y;
        for(x = 0; x < W; x++){
            buf[yc * W + x] = COLOR_WHITE;
        }
    }

    /* Concentric circles for linearity / aspect-ratio reference */
    for(r = STEP; r <= (H/2 - STEP/2); r += STEP){
        /* Bresenham-ish midpoint algorithm */
        int cx = 0, cy = r;
        int d = 1 - r;
        while(cx <= cy){
            const int pts[8][2] = {
                { CX+cx, CY+cy }, { CX-cx, CY+cy },
                { CX+cx, CY-cy }, { CX-cx, CY-cy },
                { CX+cy, CY+cx }, { CX-cy, CY+cx },
                { CX+cy, CY-cx }, { CX-cy, CY-cx }
            };
            int p;
            for(p = 0; p < 8; p++){
                dx = pts[p][0]; dy = pts[p][1];
                if(dx >= 0 && dx < W && dy >= 0 && dy < H){
                    buf[dy * W + dx] = COLOR_RED;
                }
            }
            if(d < 0){
                d += 2*cx + 3;
            } else {
                d += 2*(cx - cy) + 5;
                cy--;
            }
            cx++;
        }
    }

    /* Center crosshair, 16 pixels */
    for(x = -8; x <= 8; x++){
        if(CX + x >= 0 && CX + x < W) buf[CY * W + (CX + x)] = COLOR_GREEN;
    }
    for(y = -8; y <= 8; y++){
        if(CY + y >= 0 && CY + y < H) buf[(CY + y) * W + CX] = COLOR_GREEN;
    }

    sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_GRID);
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Test pattern: Phase
 *
 * Eight color blocks at fixed hues (yellow, magenta, cyan, red, green, blue,
 * white-on-gray, black-on-gray) used to evaluate NTSC chroma phase via a
 * vectorscope or via the eyeball-test: each block has a small cross in
 * its complementary color so any chroma shift becomes visible at the seam.
 * --------------------------------------------------------------------------- */
void DrawPhase(void){
    const int W = 320, H = 240;
    static const uint16_t blocks[8] = {
        COLOR_YELLOW, COLOR_MAGENTA, COLOR_CYAN, COLOR_RED,
        COLOR_GREEN,  COLOR_BLUE,    COLOR_WHITE, COLOR_BLACK
    };
    static const uint16_t complements[8] = {
        COLOR_BLUE,   COLOR_GREEN,   COLOR_RED,  COLOR_CYAN,
        COLOR_MAGENTA,COLOR_YELLOW,  COLOR_BLACK,COLOR_WHITE
    };

    int exit = 0;
    int row, col, i;
    int cellW = W / 4;    /* 4 columns */
    int cellH = H / 2;    /* 2 rows    */

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_GRAY50);

    for(i = 0; i < 8; i++){
        row = i / 4;
        col = i % 4;
        int x = col * cellW;
        int y = row * cellH;
        int w = (col == 3) ? (W - x) : cellW;
        int h = (row == 1) ? (H - y) : cellH;
        rectPACK_RGB16(buf, W, x, y, w, h, blocks[i]);
        /* small complementary-color cross in the middle of each block */
        int ccx = x + w/2, ccy = y + h/2;
        int len = 8;
        int j;
        for(j = -len; j <= len; j++){
            if(ccx + j >= 0 && ccx + j < W) buf[ccy * W + (ccx + j)] = complements[i];
            if(ccy + j >= 0 && ccy + j < H) buf[(ccy + j) * W + ccx] = complements[i];
        }
    }

    sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_BARS);
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Test pattern: Brightness
 *
 * 4 black bars at 0, 2, 4 and 7.5 IRE on a black background -- you raise
 * the display's brightness control until the highest two are just visible
 * but the lowest two stay buried. Distinct from the PLUGE which combines
 * brightness + contrast cues.
 * --------------------------------------------------------------------------- */
void DrawBrightness(void){
    const int W = 320, H = 240;
    /* 4 levels of "just-above-black" gray, 6-bit green / 5-bit red+blue. */
    static const uint16_t levels[4] = {
        PACK_RGB16(1, 1, 2),   /* ~ 2 IRE  */
        PACK_RGB16(2, 2, 4),   /* ~ 4 IRE  */
        PACK_RGB16(3, 3, 6),   /* ~ 7.5 IRE */
        PACK_RGB16(4, 4, 8)    /* ~10 IRE  */
    };

    int exit = 0;
    int i;
    int barW = 40, barH = 160;
    int gap = 16;
    int totalW = 4 * barW + 3 * gap;
    int x0 = (W - totalW) / 2;
    int y0 = (H - barH) / 2;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_BLACK);

    for(i = 0; i < 4; i++){
        int x = x0 + i * (barW + gap);
        rectPACK_RGB16(buf, W, x, y0, barW, barH, levels[i]);
    }

    sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_PLUGE);
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Test pattern: Contrast
 *
 * 4 white-ish bars at 90, 95, 100 and 109 IRE on a 50% gray background --
 * raise the display's contrast until the top two are distinguishable but
 * the bottom two start to merge, then back off one notch.
 * --------------------------------------------------------------------------- */
void DrawContrast(void){
    const int W = 320, H = 240;
    static const uint16_t levels[4] = {
        PACK_RGB16(28, 28, 56),  /* ~ 90 IRE */
        PACK_RGB16(29, 29, 59),  /* ~ 95 IRE */
        PACK_RGB16(31, 31, 62),  /* ~100 IRE */
        PACK_RGB16(31, 31, 63)   /* peak     */
    };

    int exit = 0;
    int i;
    int barW = 40, barH = 160;
    int gap = 16;
    int totalW = 4 * barW + 3 * gap;
    int x0 = (W - totalW) / 2;
    int y0 = (H - barH) / 2;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_GRAY50);

    for(i = 0; i < 4; i++){
        int x = x0 + i * (barW + gap);
        rectPACK_RGB16(buf, W, x, y0, barW, barH, levels[i]);
    }

    sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_PLUGE);
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Video test: Manual Lag Test
 *
 * A 16x16 white square moves once per frame across the bottom of the screen
 * synchronised with a row of frame-number digits along the top. Two displays
 * placed side-by-side will land on different digits per frame of lag, just
 * like the SNES/Genesis "Manual Lag Test" mode. A pauses/resumes the
 * auto-scroll; OPTION exits.
 * --------------------------------------------------------------------------- */
void ManualLagTest(void){
    const int W = 320, H = 240;
    int exit = 0;
    uint8_t frameDigit = 0;
    int x = 0, dir = 1;

    settings->fadeToColor = 0x0000;

    /* Background: a 16-cell horizontal ramp 0..F so the user always knows
     * which "frame slice" the square is over. */
    uint16_t *bg = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(bg, W * H, COLOR_BLACK);
    int i;
    for(i = 0; i < 16; i++){
        int g6 = (i * 63) / 15;
        int rb5 = (i * 31) / 15;
        uint16_t c = PACK_RGB16(rb5, rb5, g6);
        rectPACK_RGB16(bg, W, i * (W/16), 0, W/16, 32, c);
    }
    sprite *bgS = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, bg);
    bgS->trans = 0;
    attach_sprite_to_display_at_layer(bgS, settings->d, 12);

    /* The moving white square */
    uint16_t *sq = malloc(sizeof(uint16_t) * 16 * 16);
    for(i = 0; i < 16*16; i++) sq[i] = COLOR_WHITE;
    sprite *sqS = new_sprite(16, 16, 0, H - 32 + settings->PALOffset, DEPTH16, sq);
    sqS->trans = 0;
    attach_sprite_to_display_at_layer(sqS, settings->d, 13);

    /* Frame counter readout */
    textBox *fnTb = newTextBox("FRAME 0  ", 80, 9, mainFont, 0, settings->d, 8, 40, 13, 1);
    updateLine(settings, mainFont, fnTb, NULL, 999999, 999999, GREEN);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        /* Auto-bounce, one pixel per frame */
        x += dir;
        if(x >= W - 16){ x = W - 16; dir = -1; }
        if(x <= 0){     x = 0;       dir =  1; }
        sqS->x = x;

        frameDigit = (frameDigit + 1) & 0x0F;
        char digit = frameDigit < 10 ? ('0' + frameDigit) : ('A' + (frameDigit - 10));
        fnTb->text[6] = digit;
        updateTextBox(fnTb);

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_MANUALLAG);
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            /* Pause/resume by pinning direction to zero */
            settings->controllerLock = 1;
            if(dir == 0){ dir = 1; } else { dir = 0; }
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    fnTb = freeTextBox(fnTb);
    teardownFullscreenSprite(sqS, sq);
    teardownFullscreenSprite(bgS, bg);
}

/* ---------------------------------------------------------------------------
 * Video test: Alternate 240p / 480i
 *
 * The Jaguar always outputs progressive 240p (or 288p in PAL) from this cart.
 * What this test actually verifies is whether the *display* is treating the
 * signal as true 240p or as a deinterlaced 480i input. We render a
 * full-screen horizontal-stripe pattern (1-pixel-on / 1-pixel-off) and let
 * the user toggle between three modes:
 *
 *   Static:  the stripes never change.  On a real 240p path you see crisp
 *            alternating lines; an aggressive 480i deinterlacer will smear
 *            them into uniform gray.
 *   Toggle:  every frame we invert the pattern (white <-> black).  On true
 *            240p this looks like solid gray @ 60 Hz flicker.  A display
 *            that thinks it's 480i will lock onto one field and show
 *            stable stripes (or visible flicker artifacts).
 *   Vertical: same as Static but vertical stripes -- useful as a sanity
 *            check that the display geometry isn't smearing horizontally.
 *
 * A cycles modes, OPTION exits.
 * --------------------------------------------------------------------------- */
void Alternate240p480iTest(void){
    const int W = 320, H = 240;
    enum { MODE_STATIC = 0, MODE_TOGGLE = 1, MODE_VERTICAL = 2, MODE_COUNT = 3 };
    int mode = MODE_STATIC;
    int field = 0;
    int exit = 0;
    int redrawLabel = 1;
    int x, y;

    settings->fadeToColor = 0x0000;

    /* Two pre-baked framebuffers we ping-pong between for the Toggle mode.
     * `fbA` is "white-on-even-rows", `fbB` is the inverse. */
    uint16_t *fbA = malloc(sizeof(uint16_t) * W * H);
    uint16_t *fbB = malloc(sizeof(uint16_t) * W * H);
    uint16_t *fbV = malloc(sizeof(uint16_t) * W * H);

    for(y = 0; y < H; y++){
        uint16_t cA = (y & 1) ? COLOR_BLACK : COLOR_WHITE;
        uint16_t cB = (y & 1) ? COLOR_WHITE : COLOR_BLACK;
        for(x = 0; x < W; x++){
            fbA[y * W + x] = cA;
            fbB[y * W + x] = cB;
        }
    }
    for(y = 0; y < H; y++){
        for(x = 0; x < W; x++){
            fbV[y * W + x] = (x & 1) ? COLOR_BLACK : COLOR_WHITE;
        }
    }

    sprite *fbS = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, fbA);
    fbS->trans = 0;
    attach_sprite_to_display_at_layer(fbS, settings->d, 12);

    /* Mode label sits inside the safe area but stays small so it doesn't
     * obscure the centre of the test pattern. */
    textBox *modeTb = newTextBox("MODE: STATIC 240p ", 192, 9, mainFont, 0, settings->d, 8, 8, 13, 1);
    updateLine(settings, mainFont, modeTb, NULL, 999999, 999999, GREEN);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(mode == MODE_TOGGLE){
            field ^= 1;
            fbS->data = (phrase*)(field ? fbB : fbA);
        }

        if(redrawLabel){
            const char *label = "STATIC 240p ";
            switch(mode){
                case MODE_STATIC:   label = "STATIC 240p "; fbS->data = (phrase*)fbA; break;
                case MODE_TOGGLE:   label = "TOGGLE FIELD"; field = 0; fbS->data = (phrase*)fbA; break;
                case MODE_VERTICAL: label = "VERT 240p   "; fbS->data = (phrase*)fbV; break;
            }
            int j;
            for(j = 0; j < 12; j++){ modeTb->text[6 + j] = label[j]; }
            updateLine(settings, mainFont, modeTb, NULL, 999999, 999999, GREEN);
            redrawLabel = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_GENERAL);
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            mode = (mode + 1) % MODE_COUNT;
            redrawLabel = 1;
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    modeTb = freeTextBox(modeTb);
    /* Detach + free sprite, then free both ping-pong buffers. The sprite's
     * data pointer may currently point at fbA, fbB or fbV; teardown only
     * frees what we pass it explicitly. */
    if(fbS != NULL){
        fbS->invisible = 1;
        detach_sprite_from_display(fbS);
        free(fbS);
    }
    free(fbA);
    free(fbB);
    free(fbV);
}

/* ---------------------------------------------------------------------------
 * Audio test: L/R Balance + 1 kHz reference tone
 *
 * Cycles through Center, Left, Right and Mute at the press of A; the on-
 * screen indicator shows which channel is active. B toggles a continuous
 * 1 kHz reference tone (using the DSP rom_sine wavetable) at the chosen
 * panning, played at settings->masterVolume so the Options knob applies.
 * Useful for verifying RCA/SCART wiring and balance trim.
 * --------------------------------------------------------------------------- */
void AudioBalanceTest(void){
    int exit = 0;
    int redraw = 1;
    int state = 1;            /* 0 = mute, 1 = center, 2 = left, 3 = right */
    int playing = 0;
    int ii, i;

    /* 75 cycles of the 128-sample DSP sine wavetable, played at C7 ~= 1 kHz */
    int sampleRepeat = 75;
    int sampleSize = 128 * sampleRepeat;
    int16_t DSPSample[sampleSize];
    for(ii = 0; ii < sampleRepeat; ii++){
        for(i = 0; i < 128; i++){
            DSPSample[(ii*128) + i] = (int16_t)JERRYREGS->rom_sine[i];
        }
    }

    settings->fadeToColor = 0x0000;

    textBox *titleTb = newTextBox("AUDIO L/R BALANCE", 192, 9, mainFont, 0, settings->d, 80, 64, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *stateTb = newTextBox("CHANNEL: CENTER ", 192, 9, mainFont, 0, settings->d, 80, 96, 13, 1);
    updateLine(settings, mainFont, stateTb, NULL, 999999, 999999, WHITE);

    textBox *toneTb  = newTextBox("TONE: OFF       ", 192, 9, mainFont, 0, settings->d, 80, 112, 13, 1);
    updateLine(settings, mainFont, toneTb, NULL, 999999, 999999, WHITE);

    textBox *helpTb = newTextBox("A: change channel  B: tone  OPTION: exit", 256, 9, mainFont, 0, settings->d, 24, 184, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(redraw){
            const char *label = "CENTER ";
            int pan = 8;
            switch(state){
                case 0: label = "MUTE   "; break;
                case 1: label = "CENTER "; pan = 8;  break;
                case 2: label = "LEFT   "; pan = 0;  break;
                case 3: label = "RIGHT  "; pan = 16; break;
            }
            for(i = 0; i < 7; i++){ stateTb->text[9 + i] = label[i]; }
            updateLine(settings, mainFont, stateTb, NULL, 999999, 999999, WHITE);

            for(i = 0; i < 4; i++){ toneTb->text[6 + i] = playing ? "ON  "[i] : "OFF "[i]; }
            updateLine(settings, mainFont, toneTb, NULL, 999999, 999999, WHITE);

            clear_voice(0);
            if(playing && state != 0){
                set_voice(0, VOICE_16|VOICE_BALANCE(pan)|VOICE_VOLUME(settings->masterVolume)|VOICE_FREQ(C7, freq),
                          (char*)DSPSample, sampleSize*2,
                          (char*)DSPSample, sampleSize*2);
            }
            redraw = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_SOUND);
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            state = (state + 1) & 0x03;
            redraw = 1;
        }

        if((settings->joy1 & JOYPAD_B) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            playing = !playing;
            redraw = 1;
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    clear_voice(0);
    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    helpTb  = freeTextBox(helpTb);
    toneTb  = freeTextBox(toneTb);
    stateTb = freeTextBox(stateTb);
    titleTb = freeTextBox(titleTb);
}

/* ---------------------------------------------------------------------------
 * Hardware tools: System Info screen
 *
 * Reads back-end hardware configuration straight from TOM/JERRY at runtime:
 * region (NTSC/PAL via the existing PALNTSC probe), VMODE register value,
 * memory configuration register (MEMCON1/MEMCON2 if accessible) and main
 * RAM amount. Equivalent to the "Region detection" hardware-tools entry on
 * the Genesis/SNES versions.
 * --------------------------------------------------------------------------- */
void HardwareInfo(void){
    int exit = 0;
    char buf[12] = "00000000\0";

    textBox *titleTb = newTextBox("SYSTEM INFO", 128, 9, mainFont, 0, settings->d, 100, 48, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *regionTb = newTextBox("REGION  : NTSC", 192, 9, mainFont, 0, settings->d, 64, 80, 13, 1);
    if(settings->PALNTSC == 0){
        regionTb->text[10] = 'P';
        regionTb->text[11] = 'A';
        regionTb->text[12] = 'L';
        regionTb->text[13] = ' ';
    }
    updateLine(settings, mainFont, regionTb, NULL, 999999, 999999, WHITE);

    /* TOM VMODE register, hex */
    uint16_t vmode = TOMREGS->vmode;
    itostring(buf, (int)vmode, 16);
    textBox *vmodeTb = newTextBox("VMODE   : 0000", 192, 9, mainFont, 0, settings->d, 64, 96, 13, 1);
    int n = 0; while(buf[n] != '\0' && n < 4) n++;
    int pad;
    for(pad = 0; pad < 4 - n; pad++) vmodeTb->text[10 + pad] = '0';
    int k;
    for(k = 0; k < n; k++) vmodeTb->text[10 + (4 - n) + k] = buf[k];
    updateLine(settings, mainFont, vmodeTb, NULL, 999999, 999999, WHITE);

    /* Main RAM size: walk the standard 2 MiB window in 256 KiB steps until
     * we wrap (Jaguar systems are always 2 MiB but cleanroom homebrew can
     * stub mappers that mirror earlier). We just verify reads at 0x000000,
     * 0x100000, 0x1F0000. */
    volatile uint32_t *m0  = (volatile uint32_t*)0x000004;
    volatile uint32_t *m1f = (volatile uint32_t*)0x1F0000;
    int distinct = (*m0 != *m1f) ? 1 : 0;
    textBox *ramTb = newTextBox("MAIN RAM: 2 MiB", 192, 9, mainFont, 0, settings->d, 64, 112, 13, 1);
    if(!distinct){
        ramTb->text[10] = '?';
        ramTb->text[11] = ' ';
    }
    updateLine(settings, mainFont, ramTb, NULL, 999999, 999999, WHITE);

    textBox *cpuTb = newTextBox("CPU     : 68000 @ 13.3 MHz", 256, 9, mainFont, 0, settings->d, 64, 128, 13, 1);
    updateLine(settings, mainFont, cpuTb, NULL, 999999, 999999, WHITE);

    textBox *gpuTb = newTextBox("GPU/DSP : RISC @ 26.6 MHz", 256, 9, mainFont, 0, settings->d, 64, 144, 13, 1);
    updateLine(settings, mainFont, gpuTb, NULL, 999999, 999999, WHITE);

    textBox *helpTb = newTextBox("OPTION: exit", 128, 9, mainFont, 0, settings->d, 96, 192, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    helpTb   = freeTextBox(helpTb);
    gpuTb    = freeTextBox(gpuTb);
    cpuTb    = freeTextBox(cpuTb);
    ramTb    = freeTextBox(ramTb);
    vmodeTb  = freeTextBox(vmodeTb);
    regionTb = freeTextBox(regionTb);
    titleTb  = freeTextBox(titleTb);
}

/* ---------------------------------------------------------------------------
 * Hardware tools: Pro Controller (CatBox / 6-button) test
 *
 * Addresses upstream BitJag #12 ("Analog controller test") in the part of
 * its scope that is widely owned and emulator-testable: the **Pro
 * Controller** -- the 6-button digital pad with X/Y/Z and L/R shoulders
 * (CatBox layout, also known as Atari's planned-but-mostly-cancelled "6
 * button controller"; aftermarket clones are common, and homebrew like
 * Battlesphere Gold and Painter use the layout).
 *
 * The true early-K-series ADC0844 analog circuit (the *other* meaning of
 * "analog controller test" in #12, per http://www.mdgames.de/janalog.html)
 * is intentionally deferred -- almost no Jaguars have the chip
 * populated, no emulator simulates it, and the test would be unverifiable
 * for >99% of users. It's tracked for a future PR.
 *
 * Removers' Library already aliases the Pro Controller buttons in
 * <joypad.h>:
 *   JOYPAD_L = JOYPAD_4   JOYPAD_R = JOYPAD_6
 *   JOYPAD_X = JOYPAD_9   JOYPAD_Y = JOYPAD_8   JOYPAD_Z = JOYPAD_7
 * so reading them is just standard `read_joypad_state` + bit masking.
 *
 * Layout: large coloured pads light up green when their button is held,
 * arranged in roughly the physical CatBox shape, plus a live raw `joy1`
 * hex dump at the bottom so the user can correlate any unmapped key.
 *
 * Controls: hold any button to light it up. LEFT + OPTION exits.
 *           (The standard A/B/C buttons are also shown so users can spot
 *           which ports their controller is plugged into; the Pro
 *           controller's L/R/X/Y/Z **physically alias** to keypad
 *           4/6/9/8/7 on a stock pad, so you can also test this with a
 *           regular controller by pressing those number keys -- a useful
 *           sanity check that doesn't require buying a CatBox.)
 * --------------------------------------------------------------------------- */

/// Local pad descriptor for the on-screen Pro Controller layout. Keeping it
/// here (file-scope static, not in extra_tests.h) avoids polluting the
/// header with a struct used by exactly one test.
typedef struct {
    uint32_t    mask;       /// JOYPAD_* bit to query against settings->joy1
    int         x, y;       /// top-left of the pad on screen
    int         w, h;       /// pad dimensions
} proPad;

void ProControllerTest(void){
    int exit = 0;
    int i;

    /// Physical layout (approx. CatBox / 6-button): A B C bottom-row,
    /// X Y Z top-row, L on left side, R on right side, Pause + Option
    /// in the centre. Coordinates picked to fit comfortably in 320x240
    /// (NTSC) with the title/help strings.
    static proPad pads[] = {
        /// Top row: Z Y X (read right-to-left because L/R are on outer edges)
        { JOYPAD_Z,      152, 80,  32, 24 },
        { JOYPAD_Y,      192, 80,  32, 24 },
        { JOYPAD_X,      232, 80,  32, 24 },
        /// Bottom row (red action buttons): C B A
        { JOYPAD_C,      152, 112, 32, 24 },
        { JOYPAD_B,      192, 112, 32, 24 },
        { JOYPAD_A,      232, 112, 32, 24 },
        /// Shoulders: L on far left, R on far right
        { JOYPAD_L,      56,  64,  40, 16 },
        { JOYPAD_R,      264, 64,  40, 16 },
        /// Centre: PAUSE and OPTION
        { JOYPAD_PAUSE,  56, 112, 40, 24 },
        { JOYPAD_OPTION, 56, 144, 40, 24 },
    };
    const int padCount = (int)(sizeof(pads)/sizeof(pads[0]));

    /// Full-width DEPTH16 framebuffer so every pad in `pads[]` (including
    /// the R shoulder at x=264..303) fits without OOB writes -- rectPACK_RGB16
    /// does no clipping, and a previous 256-wide / fbX=32 layout corrupted
    /// the heap when R was redrawn each frame.
    const int fbW = 320;
    const int fbH = 144;
    const int fbX = 0;
    const int fbY = 64;
    uint16_t *fb = malloc(sizeof(uint16_t)*fbW*fbH);

    sprite *fbS = new_sprite(fbW, fbH, fbX, fbY, DEPTH16, (uint8_t*)fb);
    fbS->trans = 0;
    attach_sprite_to_display_at_layer(fbS, settings->d, 12);

    textBox *titleTb = newTextBox("PRO CONTROLLER TEST", 224, 9, mainFont, 0, settings->d, 64, 40, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *rawTb   = newTextBox("RAW JOY1: 00000000", 224, 9, mainFont, 0, settings->d, 80, 184, 13, 1);
    updateLine(settings, mainFont, rawTb, NULL, 999999, 999999, WHITE);

    textBox *helpTb  = newTextBox("Hold buttons to light up   LEFT+OPTION: exit", 256, 9, mainFont, 0, settings->d, 8, 200, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    /// One-time render of static content: black background + every pad's
    /// border. Inside the per-frame loop we ONLY rewrite each pad's filled
    /// interior, which is what changes when buttons are pressed/released.
    /// Avoids the full 320*144 = 92 KB per-frame clear that exceeds NTSC's
    /// ~1.4 ms vblank window and causes visible tearing of the pad borders.
    fillPACK_RGB16(fb, fbW*fbH, COLOR_BLACK);
    for(i = 0; i < padCount; i++){
        int rx = pads[i].x - fbX;
        int ry = pads[i].y - fbY;
        rectPACK_RGB16(fb, fbW, rx, ry,             pads[i].w, 1,         COLOR_WHITE);
        rectPACK_RGB16(fb, fbW, rx, ry+pads[i].h-1, pads[i].w, 1,         COLOR_WHITE);
        rectPACK_RGB16(fb, fbW, rx, ry,             1,         pads[i].h, COLOR_WHITE);
        rectPACK_RGB16(fb, fbW, rx+pads[i].w-1, ry, 1,         pads[i].h, COLOR_WHITE);
        rectPACK_RGB16(fb, fbW, rx+1, ry+1, pads[i].w-2, pads[i].h-2, COLOR_GRAY25);
    }

    hide_or_show_display_layer_range(settings->d, 1, 12, 13);

    /// Per-pad cached "currently lit?" state. Initialised to "unknown" (-1)
    /// so every pad gets one explicit interior-fill on the first frame
    /// regardless of whether the user is holding it.
    int padLit[16];
    for(i = 0; i < padCount && i < 16; i++){ padLit[i] = -1; }

    uint32_t prevJoy = ~settings->joy1;

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        /// Skip the whole render path on frames with no input change.
        /// Reduces the steady-state cost (held/idle button) to ~zero so
        /// the OP never sees a partial fb during scanout.
        if(settings->joy1 != prevJoy){
            for(i = 0; i < padCount; i++){
                int held = (settings->joy1 & pads[i].mask) ? 1 : 0;
                if(held == padLit[i]){ continue; }
                padLit[i] = held;
                int rx = pads[i].x - fbX;
                int ry = pads[i].y - fbY;
                uint16_t bg = held ? COLOR_GREEN : COLOR_GRAY25;
                rectPACK_RGB16(fb, fbW, rx+1, ry+1, pads[i].w-2, pads[i].h-2, bg);
            }

            char buf[12] = "00000000";
            itostring(buf, (int)(settings->joy1 & 0xFFFFFFu), 16);
            int n = 0; while(buf[n] != '\0' && n < 8) n++;
            int p;
            for(p = 0; p < 8; p++){ rawTb->text[10 + p] = '0'; }
            int k;
            for(k = 0; k < n; k++){ rawTb->text[10 + (8 - n) + k] = buf[k]; }
            updateLine(settings, mainFont, rawTb, NULL, 999999, 999999, settings->joy1 ? GREEN : WHITE);

            prevJoy = settings->joy1;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        /// LEFT + OPTION exit pattern (matches the existing ControllerTest)
        /// so users don't accidentally exit by tapping a tested button.
        if((settings->joy1 & JOYPAD_LEFT) && (settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 12, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);

    helpTb  = freeTextBox(helpTb);
    rawTb   = freeTextBox(rawTb);
    titleTb = freeTextBox(titleTb);
    teardownFullscreenSprite(fbS, fb);
}

/* ---------------------------------------------------------------------------
 * Hardware tools: Rotary Controller (Tempest 2000 spinner) test
 *
 * Addresses upstream BitJag #7 ("Rotary Controller Test"). The Tempest
 * 2000 rotary controller is an aftermarket / homebrew accessory that
 * connects a quadrature encoder to the joypad's LEFT/RIGHT column pins
 * (Padport 4 column, Padport 11/12 rows). The encoder generates pairs of
 * out-of-phase pulses as the user spins the wheel; the game (and this
 * test) reads them as rapid alternating LEFT/RIGHT presses on a single
 * joypad and decodes direction from the pulse pattern.
 *
 * Reference: AtariAge Topic 202166 "Tempest 2000 and Rotary Encoders"
 *            ConsoleMods Wiki: Jaguar:Rotary_Controller
 *            RetroRGB: jaguartempest.html
 *
 * Visualisation (per upstream issue, "something spinning while printing
 * applicable values"):
 *   - Rising-edge counter for LEFT and RIGHT pulses (independent totals)
 *   - Signed cumulative value (right pulses positive, left pulses negative)
 *   - Pulses-per-second rolling rate
 *   - Live LEFT / RIGHT bit indicator squares (lit while bit is held)
 *   - Position bar that wraps: bar fills based on (signed value) mod 256,
 *     so the user gets visual feedback of spin direction and speed without
 *     any sin/cos / dial-rotation gymnastics.
 *
 * Without a rotary controller plugged in this test still works -- it just
 * shows whatever the D-pad LEFT / RIGHT do (one pulse per press), which is
 * a useful sanity check that the read path is correct on its own.
 *
 * Controls: spin the wheel (or press LEFT/RIGHT). A resets all counters.
 *           LEFT + OPTION exits.
 * --------------------------------------------------------------------------- */
void RotaryControllerTest(void){
    int exit = 0;
    int redraw = 1;

    /// Edge-detection state: previous-frame raw bits, so we count *rising*
    /// transitions, not held-down frames. A held LEFT counts as one pulse,
    /// not 60/sec.
    int prevLeft  = 0;
    int prevRight = 0;
    int leftCnt   = 0;
    int rightCnt  = 0;
    int signedVal = 0;

    /// Rate measurement: total edges in the last 1 second.
    /// NTSC = 60 fps, PAL = 50 fps; PALNTSC > 0 selects NTSC. The ring is
    /// sized for the larger window and we only walk `rateFrames` slots so
    /// the displayed "p/s" is honest on both regions.
    const int rateFrames = (settings->PALNTSC > 0) ? 60 : 50;
    int rateRing[60];
    int rateIdx = 0;
    int i;
    for(i = 0; i < rateFrames; i++){ rateRing[i] = 0; }

    /// --- Reference frame + position-bar fb -----------------------------
    /// Reuse the screen-saver sprite recipe: 320 x 80 DEPTH16 strip across
    /// the bottom half of the screen for the position bar visualisation.
    const int barW = 320;
    const int barH = 16;
    const int barX = 0;
    const int barY = 152;
    uint16_t *barBuf = malloc(sizeof(uint16_t)*barW*barH);
    sprite *barS = new_sprite(barW, barH, barX, barY, DEPTH16, (uint8_t*)barBuf);
    barS->trans = 0;
    attach_sprite_to_display_at_layer(barS, settings->d, 12);

    /// LEFT / RIGHT bit indicator squares -- 16x16 each, side by side near
    /// the title so the user can see the raw pulse signal in real time.
    const int indSize = 16;
    uint16_t *indL = malloc(sizeof(uint16_t)*indSize*indSize);
    uint16_t *indR = malloc(sizeof(uint16_t)*indSize*indSize);
    sprite *indLs = new_sprite(indSize, indSize, 80,  56, DEPTH16, (uint8_t*)indL);
    sprite *indRs = new_sprite(indSize, indSize, 224, 56, DEPTH16, (uint8_t*)indR);
    indLs->trans = 0;
    indRs->trans = 0;
    attach_sprite_to_display_at_layer(indLs, settings->d, 12);
    attach_sprite_to_display_at_layer(indRs, settings->d, 12);

    textBox *titleTb = newTextBox("ROTARY CONTROLLER TEST", 256, 9, mainFont, 0, settings->d, 48, 32, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *lLabelTb  = newTextBox("LEFT",  64, 9, mainFont, 0, settings->d, 100, 60,  13, 1);
    textBox *rLabelTb  = newTextBox("RIGHT", 64, 9, mainFont, 0, settings->d, 244, 60,  13, 1);
    updateLine(settings, mainFont, lLabelTb, NULL, 999999, 999999, GREY);
    updateLine(settings, mainFont, rLabelTb, NULL, 999999, 999999, GREY);

    textBox *leftTb  = newTextBox("LEFT  PULSES: 00000", 192, 9, mainFont, 0, settings->d, 32, 88,  13, 1);
    textBox *rightTb = newTextBox("RIGHT PULSES: 00000", 192, 9, mainFont, 0, settings->d, 32, 104, 13, 1);
    textBox *signTb  = newTextBox("SIGNED VALUE: +00000", 192, 9, mainFont, 0, settings->d, 32, 120, 13, 1);
    textBox *rateTb  = newTextBox("RATE (1s)   : 000 p/s", 192, 9, mainFont, 0, settings->d, 32, 136, 13, 1);

    textBox *helpTb  = newTextBox("Spin or press LEFT/RIGHT. A: reset  LEFT+OPT: exit", 320, 9, mainFont, 0, settings->d, 0, 200, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    /// One-time render of the static bar chrome: black background, white
    /// 1-px border, red centre tick. Per-frame work in the loop is then
    /// just "erase the old indicator column, draw the new one" -- ~16
    /// short writes per direction change instead of a 5120-short rebuild.
    fillPACK_RGB16(barBuf, barW*barH, COLOR_BLACK);
    rectPACK_RGB16(barBuf, barW, 0, 0,      barW, 1,    COLOR_WHITE);
    rectPACK_RGB16(barBuf, barW, 0, barH-1, barW, 1,    COLOR_WHITE);
    rectPACK_RGB16(barBuf, barW, 0, 0,      1,    barH, COLOR_WHITE);
    rectPACK_RGB16(barBuf, barW, barW-1, 0, 1,    barH, COLOR_WHITE);
    rectPACK_RGB16(barBuf, barW, barW/2, 1, 1, barH-2, COLOR_RED);

    /// Indicator squares: render initial "off" state once.
    fillPACK_RGB16(indL, indSize*indSize, COLOR_GRAY25);
    fillPACK_RGB16(indR, indSize*indSize, COLOR_GRAY25);

    hide_or_show_display_layer_range(settings->d, 1, 12, 13);

    /// Tracking state for incremental updates:
    /// - prevPos: where the moving column was last frame (-1 = "no previous,
    ///   skip erase on first frame").
    /// - prevIndL/R: cached indicator state so we only refill on a flip.
    int prevPos    = -1;
    int prevIndL   = 0;
    int prevIndR   = 0;

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        /// Edge detect: rising transition only (0 -> 1). Counts each
        /// physical pulse exactly once, regardless of how many vsyncs
        /// the user holds the button.
        int curLeft  = (settings->joy1 & JOYPAD_LEFT)  ? 1 : 0;
        int curRight = (settings->joy1 & JOYPAD_RIGHT) ? 1 : 0;
        int edgeL = (curLeft  && !prevLeft)  ? 1 : 0;
        int edgeR = (curRight && !prevRight) ? 1 : 0;
        prevLeft  = curLeft;
        prevRight = curRight;

        if(edgeL){ leftCnt++;  signedVal--; redraw = 1; }
        if(edgeR){ rightCnt++; signedVal++; redraw = 1; }

        /// Rolling rate ring: store this frame's edge total; rate = sum.
        rateRing[rateIdx] = edgeL + edgeR;
        rateIdx = (rateIdx + 1) % rateFrames;
        int rate = 0;
        for(i = 0; i < rateFrames; i++){ rate += rateRing[i]; }

        /// Indicator squares: only refill on a state flip.
        if(curLeft != prevIndL){
            fillPACK_RGB16(indL, indSize*indSize, curLeft  ? COLOR_GREEN : COLOR_GRAY25);
            prevIndL = curLeft;
        }
        if(curRight != prevIndR){
            fillPACK_RGB16(indR, indSize*indSize, curRight ? COLOR_GREEN : COLOR_GRAY25);
            prevIndR = curRight;
        }

        /// Wrap signedVal into [0, barW) for the indicator column position.
        int pos = signedVal % barW;
        if(pos < 0){ pos += barW; }

        /// Incremental column update: only touch pixels when the position
        /// actually changes. Erase the previous 4-px column to black (with
        /// a centre-tick re-stamp if it overlapped), then paint the new
        /// 4-px green column. Total: ~32 short writes per move.
        if(pos != prevPos){
            int col;
            if(prevPos >= 0){
                for(col = -2; col < 2; col++){
                    int x = prevPos + col;
                    if(x < 0)     x += barW;
                    if(x >= barW) x -= barW;
                    rectPACK_RGB16(barBuf, barW, x, 1, 1, barH-2, COLOR_BLACK);
                    if(x == barW/2){
                        rectPACK_RGB16(barBuf, barW, x, 1, 1, barH-2, COLOR_RED);
                    }
                }
            }
            for(col = -2; col < 2; col++){
                int x = pos + col;
                if(x < 0)     x += barW;
                if(x >= barW) x -= barW;
                rectPACK_RGB16(barBuf, barW, x, 1, 1, barH-2, COLOR_GREEN);
            }
            prevPos = pos;
        }

        /// Refresh number text only when totals actually change -- no need
        /// to repaint every frame.
        if(redraw){
            char buf[16];
            int p, k, n;

            buf[0] = '\0'; itostring(buf, leftCnt, 10);
            n = 0; while(buf[n] != '\0' && n < 5) n++;
            for(p = 0; p < 5; p++){ leftTb->text[14 + p] = '0'; }
            for(k = 0; k < n; k++){ leftTb->text[14 + (5 - n) + k] = buf[k]; }
            updateLine(settings, mainFont, leftTb, NULL, 999999, 999999, WHITE);

            buf[0] = '\0'; itostring(buf, rightCnt, 10);
            n = 0; while(buf[n] != '\0' && n < 5) n++;
            for(p = 0; p < 5; p++){ rightTb->text[14 + p] = '0'; }
            for(k = 0; k < n; k++){ rightTb->text[14 + (5 - n) + k] = buf[k]; }
            updateLine(settings, mainFont, rightTb, NULL, 999999, 999999, WHITE);

            int absV = signedVal < 0 ? -signedVal : signedVal;
            buf[0] = '\0'; itostring(buf, absV, 10);
            n = 0; while(buf[n] != '\0' && n < 5) n++;
            signTb->text[14] = (signedVal < 0) ? '-' : '+';
            for(p = 0; p < 5; p++){ signTb->text[15 + p] = '0'; }
            for(k = 0; k < n; k++){ signTb->text[15 + (5 - n) + k] = buf[k]; }
            updateLine(settings, mainFont, signTb, NULL, 999999, 999999,
                       signedVal == 0 ? WHITE : (signedVal > 0 ? GREEN : RED));

            redraw = 0;
        }

        /// Rate row updates every frame because it's a moving window.
        char buf2[16];
        int p, k, n;
        buf2[0] = '\0'; itostring(buf2, rate, 10);
        n = 0; while(buf2[n] != '\0' && n < 3) n++;
        for(p = 0; p < 3; p++){ rateTb->text[14 + p] = '0'; }
        for(k = 0; k < n; k++){ rateTb->text[14 + (3 - n) + k] = buf2[k]; }
        updateLine(settings, mainFont, rateTb, NULL, 999999, 999999,
                   rate == 0 ? GREY : (rate > 30 ? GREEN : WHITE));

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        /// A resets the counters so the user can start a fresh spin from zero.
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            leftCnt = 0; rightCnt = 0; signedVal = 0;
            for(i = 0; i < rateFrames; i++){ rateRing[i] = 0; }
            redraw = 1;
        }

        /// LEFT + OPTION exit -- rotary protocol uses LEFT/RIGHT alone, so
        /// this combo can't fire as a side effect of normal spinning.
        if((settings->joy1 & JOYPAD_LEFT) && (settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 12, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);

    helpTb    = freeTextBox(helpTb);
    rateTb    = freeTextBox(rateTb);
    signTb    = freeTextBox(signTb);
    rightTb   = freeTextBox(rightTb);
    leftTb    = freeTextBox(leftTb);
    rLabelTb  = freeTextBox(rLabelTb);
    lLabelTb  = freeTextBox(lLabelTb);
    titleTb   = freeTextBox(titleTb);

    teardownFullscreenSprite(indRs, indR);
    teardownFullscreenSprite(indLs, indL);
    teardownFullscreenSprite(barS,  barBuf);
}

/* ---------------------------------------------------------------------------
 * Hardware tools: Video Mode / Resolution test
 *
 * Addresses upstream BitJag #2 ("Resolution Switching") within the bounds of
 * what is actually safe to flip at runtime in the Removers' Library display
 * model. The library has no `delete_display` and the menu's long-lived
 * sprites (backdrop, status line, lineTextBox[]) are sized for the boot-time
 * vmode. So a "global runtime resolution change" would require a second boot
 * path -- out of scope for a single test.
 *
 * What we *can* do safely while a test is the only thing on the display:
 *   - read every relevant TOM video register live (vmode, vp, hp, hdb1/hde,
 *     vdb/vde, vs) and show the user what the hardware is actually doing,
 *   - flip the PWIDTH (pixel-clock divider) and pixel-format bits inside
 *     `vmode` and let the user see how the connected display reacts -- this
 *     does not change any framebuffer dimensions, only how pixels are
 *     clocked out of the line buffer, so the test sprites remain valid,
 *   - restore the boot defaults on exit so the rest of the menu / tests
 *     come back exactly as before.
 *
 * This is the minimum viable answer to "give users a way to see and probe
 * Jaguar video modes" without lying about a feature we can't actually
 * deliver across every test in the suite.
 *
 * Controls: UP/DOWN cycles PWIDTH (1..8), LEFT/RIGHT cycles pixel format
 *           (CRY16 / RGB24 / DIRECT16 / RGB16), A restores the boot vmode,
 *           B toggles auto-recenter (rescales display->x as PWIDTH changes
 *           so the image stays visually centered on real hardware -- any
 *           drift you see with this on is your emulator's PWIDTH handling
 *           being broken), OPTION exits (and restores the boot vmode).
 *
 * Visual aids: a 320 x activeH reference frame is drawn on layer 12 with
 * a 1px white border and a center crosshair, so off-center / squashed
 * modes are obvious without measuring. On real hardware the frame edges
 * should kiss the visible screen edges; on a buggy emulator they won't.
 *
 * Mode tagging: well-known commercial uses are tagged inline so users
 * can connect the test to actual games -- e.g. PWIDTH8 is what Jaguar
 * Doom uses (each pixel 2x wide so 160 logical columns fill the screen),
 * CRY16 is the native format for Cybermorph / AvP / Tempest 2000.
 * --------------------------------------------------------------------------- */
void ResolutionTest(void){
    /// Snapshot the boot vmode and display->x so we can always get back to a
    /// sane display. main.c sets `RGB16 | CSYNC | BGEN | PWIDTH4 | VIDEN` and
    /// display->x = 19 (NTSC) / 14 (PAL) before show_display().
    const uint16_t vmodeBoot   = TOMREGS->vmode;
    const int      bootDisplayX = settings->d->x;

    /// Pixel-format names line up with the bit pattern in (vmode >> 1) & 0x3.
    /// Tag the commercially-recognisable formats so users connect the test
    /// to actual Jaguar games they have lying around.
    static const char *fmtName[4] = {
        "CRY16  (Cybermorph)",
        "RGB24             ",
        "DIRECT16          ",
        "RGB16             "      /// boot default
    };

    /// PWIDTH divisor table. PWIDTH(n) clocks one pixel every (n+1) cycles
    /// of the 26.59 MHz video clock (NTSC) / 26.42 MHz (PAL); the resulting
    /// active pixels-per-line at 320 line-buffer columns work out roughly
    /// to: PWIDTH4 ~= 320 native, PWIDTH2 ~= 640 squeezed, PWIDTH8 ~= 160
    /// stretched (the famous Doom mode). On real hardware all eight
    /// settings cover the same horizontal screen area; only the per-pixel
    /// width changes. Buggy emulators that ignore PWIDTH will render the
    /// stretched modes (5..8) at less than full screen width.
    static const char *pwHintTbl[8] = {
        "1280 sqsh ",   /// PWIDTH1 -- pixels 1/4 of normal
        "640 squash",   /// PWIDTH2
        "427 squash",   /// PWIDTH3
        "320 native",   /// PWIDTH4 -- boot default
        "256 stretch",  /// PWIDTH5 -- approx. SNES horizontal density
        "213 stretch",  /// PWIDTH6
        "183 stretch",  /// PWIDTH7
        "160=Doom!! "   /// PWIDTH8 -- Jaguar Doom's resolution hack
    };

    int exit = 0;
    int redraw = 1;
    int pwidth     = ((vmodeBoot >> 9) & 0x7);   /// 0..7 maps to PWIDTH1..PWIDTH8
    int fmt        = ((vmodeBoot >> 1) & 0x3);   /// 0..3 maps to fmtName[]
    int autoCenter = 0;                          /// off by default; B toggles

    int i, k, p;
    char buf[12];

    /// --- Reference frame sprite ----------------------------------------
    /// 320 x activeH DEPTH16 sprite with a 1px white border and a single-pixel
    /// crosshair through the centre. Lives on layer 12 (text rows are layer 13)
    /// so the frame sits behind the text. Active height tracks region.
    int activeH = settings->PALNTSC ? 240 : 288;
    uint16_t *frameBuf = malloc(sizeof(uint16_t) * 320 * activeH);
    fillPACK_RGB16(frameBuf, 320 * activeH, COLOR_BLACK);
    rectPACK_RGB16(frameBuf, 320, 0,           0,           320, 1, COLOR_WHITE); /// top
    rectPACK_RGB16(frameBuf, 320, 0,           activeH - 1, 320, 1, COLOR_WHITE); /// bottom
    rectPACK_RGB16(frameBuf, 320, 0,           0,           1,   activeH, COLOR_WHITE); /// left
    rectPACK_RGB16(frameBuf, 320, 319,         0,           1,   activeH, COLOR_WHITE); /// right
    rectPACK_RGB16(frameBuf, 320, 159,         0,           2,   activeH, COLOR_WHITE); /// vertical centre line
    rectPACK_RGB16(frameBuf, 320, 0,           activeH/2,   320, 2,       COLOR_WHITE); /// horizontal centre line
    /// Quarter ticks on the top/bottom borders -- helps spot horizontal scaling.
    rectPACK_RGB16(frameBuf, 320, 80,  0,           1, 4,           COLOR_WHITE);
    rectPACK_RGB16(frameBuf, 320, 240, 0,           1, 4,           COLOR_WHITE);
    rectPACK_RGB16(frameBuf, 320, 80,  activeH - 4, 1, 4,           COLOR_WHITE);
    rectPACK_RGB16(frameBuf, 320, 240, activeH - 4, 1, 4,           COLOR_WHITE);

    sprite *frameSp = new_sprite(320, activeH, 0, 0, DEPTH16, (uint8_t*)frameBuf);
    frameSp->trans = 0;
    attach_sprite_to_display_at_layer(frameSp, settings->d, 12);

    textBox *titleTb = newTextBox("VIDEO MODE TEST", 192, 9, mainFont, 0, settings->d, 80, 40, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    /// Region label is fixed for the lifetime of this test, so pick the
    /// right initial string instead of patching characters in place. The
    /// previous in-place rewrite of "NTSC" -> "PAL" left a double space
    /// before the Hz value because the tokens differ in length.
    textBox *regionTb = newTextBox(
        (settings->PALNTSC == 0) ? "REGION : PAL 50Hz " : "REGION : NTSC 60Hz",
        224, 9, mainFont, 0, settings->d, 56, 64, 13, 1);
    updateLine(settings, mainFont, regionTb, NULL, 999999, 999999, WHITE);

    /// Mutable / informative rows. Refreshed in the `redraw` block so
    /// pwidth/fmt/autoCenter edits show up next vsync. Initial strings
    /// are pre-formatted to match the post-redraw layout exactly (no
    /// parens around the hint text) so the first frame doesn't flash a
    /// different format before the redraw fires.
    textBox *fmtTb     = newTextBox("FORMAT : RGB16              ", 256, 9, mainFont, 0, settings->d, 32, 88,  13, 1);
    textBox *pwTb      = newTextBox("PWIDTH : 4 320 native       ", 256, 9, mainFont, 0, settings->d, 32, 104, 13, 1);
    textBox *vmodeTb   = newTextBox("VMODE  : 0x0000             ", 256, 9, mainFont, 0, settings->d, 32, 120, 13, 1);
    textBox *geomTb    = newTextBox("GEOM   : 320x000            ", 256, 9, mainFont, 0, settings->d, 32, 136, 13, 1);
    textBox *regsTb    = newTextBox("VP=000 HP=000               ", 256, 9, mainFont, 0, settings->d, 32, 152, 13, 1);
    textBox *recenTb   = newTextBox("RECENTER: OFF               ", 256, 9, mainFont, 0, settings->d, 32, 168, 13, 1);

    textBox *help1Tb   = newTextBox("UP/DOWN: PWIDTH   LEFT/RIGHT: format", 256, 9, mainFont, 0, settings->d, 24, 184, 13, 1);
    textBox *help2Tb   = newTextBox("A: boot mode   B: recenter   OPT: exit", 256, 9, mainFont, 0, settings->d, 16, 200, 13, 1);
    updateLine(settings, mainFont, help1Tb, NULL, 999999, 999999, GREY);
    updateLine(settings, mainFont, help2Tb, NULL, 999999, 999999, GREY);

    /// Layers 12 (frame) and 13 (text) only.
    hide_or_show_display_layer_range(settings->d, 1, 12, 13);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(redraw){
            /// Compose new vmode: keep non-PWIDTH/non-format bits from the
            /// boot value, splice in the user's PWIDTH and pixel-format.
            uint16_t newVmode = (vmodeBoot & ~((0x7u << 9) | (0x3u << 1)))
                              | ((uint16_t)(pwidth & 0x7) << 9)
                              | ((uint16_t)(fmt    & 0x3) << 1);
            TOMREGS->vmode = newVmode;

            /// Auto-recenter: scale display->x by the PWIDTH ratio. The
            /// boot divisor comes from vmodeBoot's PWIDTH bits (not a hard
            /// 4) so this stays correct if main.c ever changes its boot
            /// vmode. Wider pixels (larger divisor) shrink the centering
            /// offset proportionally; narrower pixels grow it. Integer
            /// math, +divisor/2 for round-to-nearest.
            int newDispX = bootDisplayX;
            if(autoCenter){
                int divisor = pwidth + 1;
                int bootDivisor = ((vmodeBoot >> 9) & 0x7) + 1;
                newDispX = (bootDisplayX * bootDivisor + divisor/2) / divisor;
            }
            settings->d->x = (short int)newDispX;

            /// FORMAT row -- variable-width hint, copy until null or 18 chars.
            for(i = 0; i < 18; i++){ fmtTb->text[9 + i] = ' '; }
            const char *fn = fmtName[fmt & 0x3];
            for(i = 0; i < 18 && fn[i] != '\0'; i++){ fmtTb->text[9 + i] = fn[i]; }
            updateLine(settings, mainFont, fmtTb, NULL, 999999, 999999, WHITE);

            /// PWIDTH row.
            for(i = 0; i < 14; i++){ pwTb->text[9 + i] = ' '; }
            pwTb->text[9]  = (char)('1' + pwidth);
            pwTb->text[10] = ' ';
            const char *pwh = pwHintTbl[pwidth];
            for(i = 0; i < 12 && pwh[i] != '\0'; i++){ pwTb->text[11 + i] = pwh[i]; }
            updateLine(settings, mainFont, pwTb, NULL, 999999, 999999, pwidth == 7 ? RED : WHITE);

            /// VMODE register, hex.
            buf[0] = '\0';
            itostring(buf, (int)newVmode, 16);
            int n = 0; while(buf[n] != '\0' && n < 4) n++;
            for(p = 0; p < 4; p++){ vmodeTb->text[11 + p] = '0'; }
            for(k = 0; k < n; k++){ vmodeTb->text[11 + (4 - n) + k] = buf[k]; }
            updateLine(settings, mainFont, vmodeTb, NULL, 999999, 999999, WHITE);

            /// GEOM row -- effective active resolution. Width is line buffer
            /// span (always 320 in this build); height comes from PALNTSC.
            buf[0] = '\0';
            itostring(buf, activeH, 10);
            int hn = 0; while(buf[hn] != '\0' && hn < 3) hn++;
            for(p = 0; p < 3; p++){ geomTb->text[13 + p] = ' '; }
            for(k = 0; k < hn; k++){ geomTb->text[13 + (3 - hn) + k] = buf[k]; }
            updateLine(settings, mainFont, geomTb, NULL, 999999, 999999, WHITE);

            /// VP / HP raw register dump, decimal -- handy for documenting
            /// what an emulator vs real HW reports for region timing.
            uint16_t vpReg = TOMREGS->vp;
            uint16_t hpReg = TOMREGS->hp;
            buf[0] = '\0';
            itostring(buf, (int)vpReg, 10);
            int vn = 0; while(buf[vn] != '\0' && vn < 3) vn++;
            for(p = 0; p < 3; p++){ regsTb->text[3 + p] = ' '; }
            for(k = 0; k < vn; k++){ regsTb->text[3 + (3 - vn) + k] = buf[k]; }
            buf[0] = '\0';
            itostring(buf, (int)hpReg, 10);
            int hpn = 0; while(buf[hpn] != '\0' && hpn < 3) hpn++;
            for(p = 0; p < 3; p++){ regsTb->text[10 + p] = ' '; }
            for(k = 0; k < hpn; k++){ regsTb->text[10 + (3 - hpn) + k] = buf[k]; }
            updateLine(settings, mainFont, regsTb, NULL, 999999, 999999, WHITE);

            /// RECENTER row -- show state and the actual display->x being used.
            recenTb->text[10] = 'O';
            recenTb->text[11] = autoCenter ? 'N' : 'F';
            recenTb->text[12] = autoCenter ? ' ' : 'F';
            recenTb->text[13] = ' ';
            recenTb->text[14] = '(';
            recenTb->text[15] = 'x';
            recenTb->text[16] = '=';
            buf[0] = '\0';
            itostring(buf, newDispX, 10);
            int xn = 0; while(buf[xn] != '\0' && xn < 3) xn++;
            for(p = 0; p < 3; p++){ recenTb->text[17 + p] = ' '; }
            for(k = 0; k < xn; k++){ recenTb->text[17 + (3 - xn) + k] = buf[k]; }
            recenTb->text[20] = ')';
            updateLine(settings, mainFont, recenTb, NULL, 999999, 999999, autoCenter ? GREEN : GREY);

            redraw = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            pwidth = (pwidth + 1) & 0x7;
            redraw = 1;
        }
        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            pwidth = (pwidth + 7) & 0x7;
            redraw = 1;
        }
        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            fmt = (fmt + 1) & 0x3;
            redraw = 1;
        }
        if((settings->joy1 & JOYPAD_LEFT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            fmt = (fmt + 3) & 0x3;
            redraw = 1;
        }
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            pwidth = ((vmodeBoot >> 9) & 0x7);
            fmt    = ((vmodeBoot >> 1) & 0x3);
            redraw = 1;
        }
        if((settings->joy1 & JOYPAD_B) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            autoCenter = !autoCenter;
            redraw = 1;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    /// Always restore the boot vmode + display->x before handing control back
    /// to the menu, even if the user exited via OPTION mid-stretch. Otherwise
    /// the main menu sprites would render at the wrong pixel clock / origin.
    TOMREGS->vmode  = vmodeBoot;
    settings->d->x  = (short int)bootDisplayX;

    /// Tear down test-owned layers, then restore everything else for the menu.
    hide_or_show_display_layer_range(settings->d, 0, 12, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);

    help2Tb  = freeTextBox(help2Tb);
    help1Tb  = freeTextBox(help1Tb);
    recenTb  = freeTextBox(recenTb);
    regsTb   = freeTextBox(regsTb);
    geomTb   = freeTextBox(geomTb);
    vmodeTb  = freeTextBox(vmodeTb);
    pwTb     = freeTextBox(pwTb);
    fmtTb    = freeTextBox(fmtTb);
    regionTb = freeTextBox(regionTb);
    titleTb  = freeTextBox(titleTb);

    teardownFullscreenSprite(frameSp, frameBuf);
}

/* ---------------------------------------------------------------------------
 * Options menu
 *
 * Replaces the (x)Options stubs that appeared in every sub-menu. For now the
 * options surface is intentionally narrow but real -- we expose a master
 * audio volume that's wired into the global settings and shown on screen.
 * Future options (PAL/NTSC override, scanline test mode, controller layout)
 * can be added here without touching the menu plumbing in main.c.
 * --------------------------------------------------------------------------- */
void OptionsMenu(void){
    /* Read/write the shared master volume directly so the value survives
     * across OptionsMenu invocations *and* is visible to every audio test
     * (AudioBalanceTest, MDFourierTest, SoundTest, etc) the next time they
     * call set_voice(). No private static -- the previous static-local copy
     * was the bug Qodo + Copilot flagged. */
    int exit = 0;
    int redraw = 1;
    int sel = 0;

    textBox *titleTb = newTextBox("OPTIONS", 128, 9, mainFont, 0, settings->d, 116, 48, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *volTb   = newTextBox("AUDIO VOLUME : 63", 192, 9, mainFont, 0, settings->d, 64, 96, 13, 1);
    updateLine(settings, mainFont, volTb, NULL, 999999, 999999, WHITE);

    textBox *helpTb  = newTextBox("LEFT/RIGHT: change   OPTION: exit", 256, 9, mainFont, 0, settings->d, 32, 192, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(redraw){
            char nbuf[4] = "00\0";
            itostring(nbuf, settings->masterVolume, 10);
            volTb->text[15] = ' ';
            volTb->text[16] = ' ';
            int len = 0; while(nbuf[len] != '\0' && len < 2) len++;
            int j;
            for(j = 0; j < len; j++) volTb->text[15 + (2 - len) + j] = nbuf[j];
            updateLine(settings, mainFont, volTb, NULL, 999999, 999999, sel == 0 ? RED : WHITE);
            redraw = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if((settings->joy1 & JOYPAD_LEFT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(settings->masterVolume > 0){ settings->masterVolume--; redraw = 1; }
        }
        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(settings->masterVolume < 63){ settings->masterVolume++; redraw = 1; }
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    helpTb  = freeTextBox(helpTb);
    volTb   = freeTextBox(volTb);
    titleTb = freeTextBox(titleTb);
}

/* ---------------------------------------------------------------------------
 * Audio test: MDFourier-style frequency sweep
 *
 * The canonical MDFourier reference signal is a long series of pure sine
 * tones at known frequencies that, when captured and FFT'd, fingerprints
 * a console's audio hardware (DAC linearity, low-pass filter, output
 * impedance, etc). The full upstream signal is ~200 tones over ~30s and
 * needs an external recorder + the MDFourier desktop app to interpret.
 *
 * Here we generate a representative subset on-cart: a 7-octave sweep in
 * octaves of C (C2 -> C8) using JERRY's 128-sample ROM sine wavetable
 * resampled at each target frequency via VOICE_FREQ. Each tone holds for
 * ~1 second so the user can capture the output and feed the resulting
 * .wav into MDFourier offline.
 *
 * Controls: A = play/pause sweep, B = step to next tone manually,
 *           OPTION = exit. Current tone index + Hz shown on screen.
 * --------------------------------------------------------------------------- */
void MDFourierTest(void){
    /* Octave-spaced sine sweep. Each entry pairs the target frequency in Hz
     * with a printable label so we can show it without sprintf. The Jaguar
     * SDK's C2..C8 macros encode the playback rate divisor for VOICE_FREQ. */
    static const int sweepFreq[]   = { C2,    C3,    C4,    C5,    C6,    C7,    C8    };
    static const char *sweepLabel[] = { "65Hz","131Hz","262Hz","523Hz","1kHz","2kHz","4kHz" };
    const int sweepCount = (int)(sizeof(sweepFreq)/sizeof(sweepFreq[0]));

    int exit = 0;
    int redraw = 1;
    int playing = 0;        /* sweep auto-advancing or idle */
    int tone = 0;           /* current index into sweepFreq[] */
    int holdFrames = 0;     /* frames remaining on current tone in auto mode */
    int i, ii;

    /* 128-sample wavetable replicated 75x so the voice loop covers ~1s of
     * sustain at C7. set_voice() loops the buffer so a longer buffer just
     * hides the loop seam from the listener. */
    int sampleRepeat = 75;
    int sampleSize = 128 * sampleRepeat;
    int16_t DSPSample[sampleSize];
    for(ii = 0; ii < sampleRepeat; ii++){
        for(i = 0; i < 128; i++){
            DSPSample[(ii*128) + i] = (int16_t)JERRYREGS->rom_sine[i];
        }
    }

    settings->fadeToColor = 0x0000;

    textBox *titleTb = newTextBox("MDFOURIER SWEEP", 192, 9, mainFont, 0, settings->d, 80, 48, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *toneTb  = newTextBox("TONE 1/7  : 65Hz   ", 192, 9, mainFont, 0, settings->d, 64, 88, 13, 1);
    updateLine(settings, mainFont, toneTb, NULL, 999999, 999999, WHITE);

    textBox *stateTb = newTextBox("STATE     : IDLE   ", 192, 9, mainFont, 0, settings->d, 64, 104, 13, 1);
    updateLine(settings, mainFont, stateTb, NULL, 999999, 999999, WHITE);

    textBox *helpTb  = newTextBox("A: play/pause  B: next  OPTION: exit", 256, 9, mainFont, 0, settings->d, 24, 184, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    textBox *infoTb  = newTextBox("Capture line-out then run MDFourier", 256, 9, mainFont, 0, settings->d, 24, 200, 13, 1);
    updateLine(settings, mainFont, infoTb, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        /* In auto mode, hold each tone for ~60 frames (~1s NTSC) then step. */
        if(playing){
            if(holdFrames > 0){
                holdFrames--;
            }
            else{
                tone++;
                if(tone >= sweepCount){
                    tone = 0;
                    playing = 0;        /* sweep complete -- stop auto-advance */
                }
                holdFrames = 60;
                redraw = 1;
            }
        }

        if(redraw){
            /* Patch tone index ("1/7") + label in-place so we don't keep
             * re-allocating textBox->text. The buffer was sized for the
             * widest possible value at construction. */
            toneTb->text[5]  = (char)('0' + (tone + 1));
            toneTb->text[7]  = (char)('0' + sweepCount);
            for(i = 0; i < 7; i++){
                char c = sweepLabel[tone][i];
                if(c == '\0'){ c = ' '; }
                toneTb->text[12 + i] = c;
            }
            updateLine(settings, mainFont, toneTb, NULL, 999999, 999999, WHITE);

            const char *st = playing ? "PLAYING" : "IDLE   ";
            for(i = 0; i < 7; i++){ stateTb->text[12 + i] = st[i]; }
            updateLine(settings, mainFont, stateTb, NULL, 999999, 999999, playing ? RED : WHITE);

            clear_voice(0);
            if(playing){
                set_voice(0, VOICE_16|VOICE_BALANCE(8)|VOICE_VOLUME(settings->masterVolume)|VOICE_FREQ(sweepFreq[tone], freq),
                          (char*)DSPSample, sampleSize*2,
                          (char*)DSPSample, sampleSize*2);
            }
            redraw = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            playing = !playing;
            holdFrames = 60;
            redraw = 1;
        }

        if((settings->joy1 & JOYPAD_B) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            tone = (tone + 1) % sweepCount;
            holdFrames = 60;
            redraw = 1;
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    clear_voice(0);
    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    infoTb  = freeTextBox(infoTb);
    helpTb  = freeTextBox(helpTb);
    stateTb = freeTextBox(stateTb);
    toneTb  = freeTextBox(toneTb);
    titleTb = freeTextBox(titleTb);
}

/* ---------------------------------------------------------------------------
 * Hardware test: Jaguar CD detection / probe screen
 *
 * The Jaguar CD attachment maps Butch (the CD audio + transport ASIC) into
 * the host bus at $F14000+, and on power-on the CD BIOS (the well-known
 * "Memory Track" boot) gets paged in at $00800000. When no CD unit is
 * attached, those addresses read open-bus / 0xFFFF.
 *
 * We don't ship the upstream Memory-Track flow here -- that requires
 * actually driving the CD transport -- but we DO probe a few well-known
 * register addresses, print their raw values in hex, and apply a simple
 * heuristic: if at least one of the probed words returns something other
 * than 0x0000 / 0xFFFF, the unit is almost certainly attached.
 *
 * On a stock Jaguar console (no CD) this screen will report "NOT DETECTED"
 * and is purely informational. On a Jag CD it gives the user a quick
 * sanity check that the CD's data bus is alive without booting media.
 * --------------------------------------------------------------------------- */
void JaguarCDTest(void){
    int exit = 0;
    char buf[12] = "00000000\0";
    int i;

    /* Three Butch register addresses we sample. The first two are part of
     * the CD audio interface (subcode + status); the third is well inside
     * the paged-in CD BIOS window when the unit is attached. */
    volatile uint16_t *butchA  = (volatile uint16_t*)0xF14000;
    volatile uint16_t *butchB  = (volatile uint16_t*)0xF14002;
    volatile uint16_t *cdBios  = (volatile uint16_t*)0x00800000;

    uint16_t vA = *butchA;
    uint16_t vB = *butchB;
    uint16_t vC = *cdBios;

    /* Heuristic: open bus on this region tends to read 0x0000 or 0xFFFF.
     * If anything else comes back from at least one probe, the CD is
     * almost certainly mapped in. This is identical in spirit to the
     * "CD detected?" probe used by Skunkboard and homebrew loaders. */
    int detected = 0;
    if(vA != 0x0000 && vA != 0xFFFF) detected = 1;
    if(vB != 0x0000 && vB != 0xFFFF) detected = 1;
    if(vC != 0x0000 && vC != 0xFFFF) detected = 1;

    textBox *titleTb = newTextBox("JAGUAR CD PROBE", 192, 9, mainFont, 0, settings->d, 80, 48, 13, 1);
    updateLine(settings, mainFont, titleTb, NULL, 999999, 999999, GREEN);

    textBox *statusTb = newTextBox("STATUS  : NOT DETECTED  ", 256, 9, mainFont, 0, settings->d, 48, 80, 13, 1);
    if(detected){
        const char *yes = "DETECTED      ";
        for(i = 0; i < 14; i++){ statusTb->text[10 + i] = yes[i]; }
    }
    updateLine(settings, mainFont, statusTb, NULL, 999999, 999999, detected ? GREEN : RED);

    /* Print each probed value as a 4-char hex word so the user can sanity
     * check against schematics or compare to a known-good unit. */
    textBox *aTb = newTextBox("$F14000 : 0000 ", 192, 9, mainFont, 0, settings->d, 48, 104, 13, 1);
    itostring(buf, (int)vA, 16);
    {
        int n = 0; while(buf[n] != '\0' && n < 4) n++;
        int pad;
        for(pad = 0; pad < 4 - n; pad++) aTb->text[10 + pad] = '0';
        for(i = 0; i < n; i++) aTb->text[10 + (4 - n) + i] = buf[i];
    }
    updateLine(settings, mainFont, aTb, NULL, 999999, 999999, WHITE);

    textBox *bTb = newTextBox("$F14002 : 0000 ", 192, 9, mainFont, 0, settings->d, 48, 120, 13, 1);
    itostring(buf, (int)vB, 16);
    {
        int n = 0; while(buf[n] != '\0' && n < 4) n++;
        int pad;
        for(pad = 0; pad < 4 - n; pad++) bTb->text[10 + pad] = '0';
        for(i = 0; i < n; i++) bTb->text[10 + (4 - n) + i] = buf[i];
    }
    updateLine(settings, mainFont, bTb, NULL, 999999, 999999, WHITE);

    textBox *cTb = newTextBox("$800000 : 0000 ", 192, 9, mainFont, 0, settings->d, 48, 136, 13, 1);
    itostring(buf, (int)vC, 16);
    {
        int n = 0; while(buf[n] != '\0' && n < 4) n++;
        int pad;
        for(pad = 0; pad < 4 - n; pad++) cTb->text[10 + pad] = '0';
        for(i = 0; i < n; i++) cTb->text[10 + (4 - n) + i] = buf[i];
    }
    updateLine(settings, mainFont, cTb, NULL, 999999, 999999, WHITE);

    textBox *noteTb = newTextBox("0000/FFFF on all = open bus, no CD", 256, 9, mainFont, 0, settings->d, 24, 168, 13, 1);
    updateLine(settings, mainFont, noteTb, NULL, 999999, 999999, GREY);

    textBox *helpTb = newTextBox("OPTION: exit", 128, 9, mainFont, 0, settings->d, 96, 200, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    helpTb   = freeTextBox(helpTb);
    noteTb   = freeTextBox(noteTb);
    cTb      = freeTextBox(cTb);
    bTb      = freeTextBox(bTb);
    aTb      = freeTextBox(aTb);
    statusTb = freeTextBox(statusTb);
    titleTb  = freeTextBox(titleTb);
}

/* ---------------------------------------------------------------------------
 * Screen Saver: Color Cycle
 *
 * Pure background-color sweep through the HSV-ish hue space using TOM's
 * BG color register. Costs zero sprite bandwidth -- we just rewrite the
 * BG register every frame -- which makes it a perfect OLED-burn check
 * because the color is uniform across the entire active picture area.
 *
 * Controls: any non-OPTION button = pause/resume cycling. OPTION = exit.
 * --------------------------------------------------------------------------- */
void ColorCycleSaver(void){
    int exit = 0;
    int paused = 0;
    /* Walk the 16-bit RGB16 color cube along the standard 6-step rainbow
     * (R -> Y -> G -> C -> B -> M -> R). step counter tracks the current
     * leg + position within it; tone toggles between dim and full
     * saturation across passes so we exercise both ends of the DAC. */
    int leg = 0;          /* 0..5 */
    int pos = 0;          /* 0..63 */
    int frameDelay = 0;   /* sub-frame counter for slowing the cycle */

    /* Hide the menu sprite layer + background. Screen savers want a fully
     * empty active picture so only the BG color shows through. */
    hide_or_show_display_layer_range(settings->d, 0, 0, 15);

    textBox *helpTb = newTextBox("OPTION: exit  A: pause", 192, 9, mainFont, 0, settings->d, 64, 8, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);
    hide_or_show_display_layer_range(settings->d, 1, 13, 13);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(!paused){
            frameDelay++;
            if(frameDelay >= 2){     /* ~30 Hz hue updates -- silky on a CRT */
                frameDelay = 0;
                pos++;
                if(pos > 63){
                    pos = 0;
                    leg = (leg + 1) % 6;
                }
            }

            uint16_t r = 0, g = 0, b = 0;
            int p = pos & 63;
            switch(leg){
                case 0: r = 31; g = (uint16_t)p;        b = 0;             break;     /* R -> Y */
                /* Red is a 5-bit channel (0..31) -- earlier (63 - p) overflowed
                 * into the upper RGB16 bits and made Y->G hitch instead of fading
                 * smoothly. Mirror the (p>>1) scaling used by the other legs so
                 * red ramps cleanly from 31 down to 0 across the 64-step leg. */
                case 1: r = (uint16_t)(31 - (p>>1)); g = 63; b = 0;        break;     /* Y -> G */
                case 2: r = 0;  g = 63;                 b = (uint16_t)(p>>1); break;  /* G -> C */
                case 3: r = 0;  g = (uint16_t)(63 - p); b = 31;            break;     /* C -> B */
                case 4: r = (uint16_t)(p>>1); g = 0;    b = 31;            break;     /* B -> M */
                case 5: r = 31; g = 0;                  b = (uint16_t)(31 - (p>>1)); break; /* M -> R */
            }
            /* Jaguar RGB16 packed format: r<<11 | b<<6 | g (R5 B5 G6). */
            TOMREGS->bg = (uint16_t)((r << 11) | (b << 6) | g);
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            paused = !paused;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    TOMREGS->bg = 0x0000;
    hide_or_show_display_layer_range(settings->d, 0, 13, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
    helpTb = freeTextBox(helpTb);
}

/* ---------------------------------------------------------------------------
 * Screen Saver: Bouncing Square
 *
 * A 32x32 DEPTH16 white sprite that bounces around the active area on a
 * fixed trajectory, similar to the classic DVD logo screen saver. Forces
 * every pixel of the panel to light up at least once over a few minutes,
 * which is the canonical OLED burn-in mitigation use case. The bounce
 * trail is intentionally NOT drawn so that any latent pixels show up
 * against the otherwise-black background.
 *
 * Implementation note: uses DEPTH16 with explicit RGB16 white (0xFFFF)
 * rather than DEPTH8 + CLUT lookup, so the saver doesn't depend on
 * (or have to restore) any particular state in TOMREGS->clut1.
 *
 * Controls: OPTION = exit. Velocity is hard-coded; A presses do nothing
 * by design (any sub-frame button latency would bias the trajectory).
 * --------------------------------------------------------------------------- */
void BouncingSquareSaver(void){
    int exit = 0;
    int i;
    const int sw = 32;
    const int sh = 32;
    const int screenH = (settings->PALNTSC > 0) ? 240 : 288;

    /* 32x32 DEPTH16 sprite filled with explicit white = R5+B5+G6 all max.
     *
     * The earlier DEPTH8 implementation indexed CLUT entry 1, but the menu
     * doesn't guarantee any particular value at TOMREGS->clut1[1] (the LED
     * test in tests.c explicitly populates clut1 entries; we'd have had to
     * do the same and then restore the menu's CLUT on exit). Going DEPTH16
     * with literal RGB16 pixels sidesteps the CLUT plumbing entirely and
     * survives any future menu palette changes. */
    const uint16_t white = (uint16_t)((31u << 11) | (31u << 6) | 63u);
    uint16_t *sqData = malloc(sizeof(uint16_t)*sw*sh);
    for(i = 0; i < sw*sh; i++){ sqData[i] = white; }

    /* Layer state on entry: the menu has just hidden every layer it owns,
     * so by default *no* layer is visible -- attaching to layer 12 is not
     * enough on its own, we still have to explicitly hide_or_show it.
     * Hide everything first as a known-good baseline, then bring up only
     * the two layers we actually use (12 = bouncing square, 13 = help
     * text). The previous version skipped the layer-12 show and so the
     * sprite was attached but invisible -- pure black screen. */
    hide_or_show_display_layer_range(settings->d, 0, 0, 15);

    sprite *sq = new_sprite(sw, sh, 80, 80, DEPTH16, (uint8_t*)sqData);
    /* Match every other DEPTH16 sprite in the project -- new_sprite()'s
     * default trans is non-zero, which would otherwise punch holes through
     * texels that happen to match the magic transparent value. */
    sq->trans = 0;
    attach_sprite_to_display_at_layer(sq, settings->d, 12);
    hide_or_show_display_layer_range(settings->d, 1, 12, 12);

    int vx = 2;
    int vy = 1;
    int x = 80;
    int y = 80;

    textBox *helpTb = newTextBox("OPTION: exit", 128, 9, mainFont, 0, settings->d, 96, 8, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);
    hide_or_show_display_layer_range(settings->d, 1, 13, 13);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        x += vx;
        y += vy;
        if(x < 0)             { x = 0;          vx = -vx; }
        if(x + sw > 320)      { x = 320 - sw;   vx = -vx; }
        if(y < 0)             { y = 0;          vy = -vy; }
        if(y + sh > screenH)  { y = screenH - sh; vy = -vy; }
        sq->x = x;
        sq->y = y;

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    sq->invisible = 1;
    detach_sprite_from_display(sq);
    free(sq);
    free(sqData);

    /* Hide the saver-owned layers, then re-show every layer so the menu
     * (layers 0-11) is visible again on return. Matches ColorCycleSaver. */
    hide_or_show_display_layer_range(settings->d, 0, 12, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
    helpTb = freeTextBox(helpTb);
}

/* ---------------------------------------------------------------------------
 * Screen Saver: Scrolling Color Bars
 *
 * Builds a single 320 x screenH RGB16 framebuffer of vertical rainbow bars
 * and attaches it as TWO sprites side-by-side, both pointing at the same
 * pixel data. Scrolling just nudges both sprite x positions in lockstep:
 * as one slides off the left edge, the other slides in from the right.
 * When the leading sprite has fully exited (sx hits -320), we snap sx
 * back to 0 -- the picture is identical at sx=0 and sx=-320, so the seam
 * is invisible.
 *
 * Doubles as a chroma-bleed / scroll-jitter eyeball test because the color
 * transitions never sit still long enough for the display's color
 * converter to settle on any one transition.
 *
 * The previous single-sprite implementation let sx range across [-320, 320]
 * and so spent half its cycle with the sprite fully off-screen, which
 * looked like a blank frame followed by a sudden snap. The two-sprite
 * trick is the same idea every Genesis/SNES scrolling-background routine
 * uses, just expressed via the Jaguar OP instead of an HW scroll register.
 *
 * Controls: A = reverse direction, OPTION = exit.
 * --------------------------------------------------------------------------- */
void ScrollingBarsSaver(void){
    int exit = 0;
    int dir = 1;
    int i, x;
    const int screenH = (settings->PALNTSC > 0) ? 240 : 288;

    /* Single 320 x screenH RGB16 framebuffer. Both sprites read from this
     * one buffer -- no need to double the memory just to get seamless
     * wraparound. */
    uint16_t *fb = malloc(sizeof(uint16_t)*320*screenH);

    /* Six-color rainbow bars (R, Y, G, C, B, M) repeated to fill 320 wide.
     * Each bar is ~53px (320/6 = 53.3) so the cycle wraps cleanly at the
     * screen edge -- both halves of the bar pair line up at sx == -320. */
    static const uint16_t bars[6] = {
        (uint16_t)((31<<11) | (0 <<6) | 0 ),    /* red    */
        (uint16_t)((31<<11) | (0 <<6) | 63),    /* yellow */
        (uint16_t)((0 <<11) | (0 <<6) | 63),    /* green  */
        (uint16_t)((0 <<11) | (31<<6) | 63),    /* cyan   */
        (uint16_t)((0 <<11) | (31<<6) | 0 ),    /* blue   */
        (uint16_t)((31<<11) | (31<<6) | 0 )     /* magenta*/
    };
    for(x = 0; x < 320; x++){
        uint16_t color = bars[(x / 54) % 6];
        for(i = 0; i < screenH; i++){
            fb[i*320 + x] = color;
        }
    }

    /* Same layer-state baseline as BouncingSquareSaver: hide everything,
     * then bring up only the layers we need. Without an explicit show on
     * layer 12 the sprites attach but never become visible (the menu had
     * everything hidden) -- previously caused a black screen. */
    hide_or_show_display_layer_range(settings->d, 0, 0, 15);

    /* Two sprites, identical pixel source, x positions kept 320 apart so
     * the visible scroll is always covered. trans=0 for both -- DEPTH16
     * fullscreen sprites in this project always disable trans (matches
     * Draw100IRE, DrawWhiteScreen, all the new procedural patterns).
     * Both sprites share layer 12 -- a single show call covers both. */
    sprite *fbS1 = new_sprite(320, screenH, 0,   0, DEPTH16, (uint8_t*)fb);
    fbS1->trans = 0;
    attach_sprite_to_display_at_layer(fbS1, settings->d, 12);
    sprite *fbS2 = new_sprite(320, screenH, 320, 0, DEPTH16, (uint8_t*)fb);
    fbS2->trans = 0;
    attach_sprite_to_display_at_layer(fbS2, settings->d, 12);
    hide_or_show_display_layer_range(settings->d, 1, 12, 12);

    textBox *helpTb = newTextBox("A: reverse  OPTION: exit", 192, 9, mainFont, 0, settings->d, 64, 8, 13, 1);
    updateLine(settings, mainFont, helpTb, NULL, 999999, 999999, GREY);
    hide_or_show_display_layer_range(settings->d, 1, 13, 13);

    /* sx is the x position of the LEFT sprite; the right sprite always
     * sits at sx + 320. Confined to (-320, 0]: when sx slides below
     * -320, snap to 0 (right sprite has just become the left one). When
     * scrolling the other direction, sx slides above 0 and we snap to
     * -320 (left sprite has just become the right one). */
    int sx = 0;
    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        sx += dir;
        if(sx <= -320) sx = 0;
        if(sx >    0)  sx = -320;
        fbS1->x = sx;
        fbS2->x = sx + 320;

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            dir = -dir;
        }
        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    fbS1->invisible = 1;
    fbS2->invisible = 1;
    detach_sprite_from_display(fbS1);
    detach_sprite_from_display(fbS2);
    free(fbS1);
    free(fbS2);
    free(fb);

    /* Hide saver-owned layers, then re-show every layer so the menu
     * (layers 0-11) is visible again on return. */
    hide_or_show_display_layer_range(settings->d, 0, 12, 13);
    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
    helpTb = freeTextBox(helpTb);
}

/* ---------------------------------------------------------------------------
 * Test pattern: Y/C Delay
 *
 * Canonical chroma-subsampling visualizer ported from the Artemio Urbina
 * suite. We render six vertical strips -- red, green, blue, yellow, cyan
 * and magenta -- separated by 1-pixel pure-white dividers. Cabling that
 * carries true RGB (SCART RGB, VGA, component on a clean path) keeps the
 * dividers perfectly white -- the luma and chroma edges of each strip line
 * up. Composite or S-Video routes the chroma through a delay/notch filter,
 * and the divider picks up coloured fringing on either side as chroma
 * trails luma.
 *
 * Useful as a quick "is my SCART cable actually RGB?" check before trusting
 * the rest of the colour-bar suite.
 * --------------------------------------------------------------------------- */
void DrawYCDelay(void){
    const int W = 320, H = 240;
    /* 6 colour strips on a black background, each strip flanked by a 1-pixel
     * white divider. Width chosen so chroma fringing has room to develop
     * across a typical CRT's PAL/NTSC chroma-delay window without strips
     * bleeding into one another. */
    const int STRIP_W = 48;
    const int STRIP_COUNT = 6;
    static const uint16_t strips[6] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA
    };

    int exit = 0;
    int i, x0;
    int totalW = STRIP_COUNT * STRIP_W + (STRIP_COUNT + 1);
    int xStart = (W - totalW) / 2;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillPACK_RGB16(buf, W * H, COLOR_BLACK);

    for(i = 0; i < STRIP_COUNT; i++){
        x0 = xStart + i * (STRIP_W + 1);
        rectPACK_RGB16(buf, W, x0 + 1, 16, STRIP_W, H - 32, strips[i]);
        /* 1-pixel white divider on the leading edge -- this is the actual
         * test target. On RGB it stays white; on composite it fringes. */
        rectPACK_RGB16(buf, W, x0,             16, 1, H - 32, COLOR_WHITE);
    }
    /* Trailing divider after the last strip. */
    rectPACK_RGB16(buf, W, xStart + STRIP_COUNT * (STRIP_W + 1), 16, 1, H - 32, COLOR_WHITE);

    {
        sprite *s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
        s->trans = 0;
        attach_sprite_to_display_at_layer(s, settings->d, 13);

        hide_or_show_display_layer_range(settings->d, 1, 3, 15);

        while(!exit){
            read_joypad_state(settings->j_state);
            settings->joy1 = settings->j_state->j1;
            vsync();

            if((settings->joy1 & 0xFFFFFF) == 0){
                settings->controllerLock = 0;
            }

            if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
                settings->controllerLock = 1;
                DrawHelp(HELP_YCDELAY);
            }

            if(extraExitPressed()){
                settings->controllerLock = 1;
                exit = 1;
            }
        }

        hide_or_show_display_layer_range(settings->d, 0, 3, 15);
        teardownFullscreenSprite(s, buf);
    }
}

/* ---------------------------------------------------------------------------
 * Test pattern: Diagonal / Clock
 *
 * Parallel 1-pixel diagonals at 45 degrees on a flat background. Stair-
 * stepping is invisible on a CRT (the analogue beam smears the steps) and
 * minimal on a 1:1 digital path, but upscalers (HDMI, OSSC, RetroTink,
 * generic TV scaler ASICs) produce visibly different artefacts depending
 * on their interpolation kernel. Shipping the same pattern at multiple
 * spacings makes it easy to spot scaler kernels that handle dense edges
 * differently from sparse ones.
 *
 * Controls:
 *   D-pad UP/DOWN   -- cycle spacing 4 / 8 / 16 / 32 px
 *   A               -- invert (white-on-black <-> black-on-white)
 *   OPTION          -- exit
 * --------------------------------------------------------------------------- */
void DrawDiagonal(void){
    const int W = 320, H = 240;
    static const int SPACINGS[4] = { 4, 8, 16, 32 };
    int spacingIdx = 1;        /* default 8 px to match upstream canonical */
    int invert = 0;
    int exit = 0;
    int needRedraw = 1;

    uint16_t *buf;
    sprite *s;
    textBox *labelTb;

    settings->fadeToColor = 0x0000;

    buf = malloc(sizeof(uint16_t) * W * H);
    /* Pre-clear so the first vsync after attach/show never displays the
     * uninitialised malloc payload. The needRedraw branch below repaints
     * the real diagonals on the first loop iteration. */
    fillPACK_RGB16(buf, W * H, COLOR_BLACK);

    s = new_sprite(W, H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    /* Worst-case-sized label so newTextBox allocates a framebuffer wide
     * enough; later updateLine calls only ever shorten the visible string. */
    labelTb = newTextBox("SPACING: 32  INVERT: ON ", 192, 9, mainFont, 0, settings->d, 8, 8, 14, 1);
    updateLine(settings, mainFont, labelTb, NULL, 999999, 999999, GREEN);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(needRedraw){
            int spacing = SPACINGS[spacingIdx];
            uint16_t bg = invert ? COLOR_WHITE : COLOR_BLACK;
            uint16_t fg = invert ? COLOR_BLACK : COLOR_WHITE;
            int x, y, k;
            char buf2[24];
            int i;

            fillPACK_RGB16(buf, W * H, bg);

            /* Each diagonal satisfies x + y = k. Stepping k by `spacing`
             * gives a family of equally-spaced parallel 45-degree lines
             * that cover the full frame regardless of aspect. */
            for(k = 0; k < (W + H); k += spacing){
                for(y = 0; y < H; y++){
                    x = k - y;
                    if(x >= 0 && x < W){
                        buf[y * W + x] = fg;
                    }
                }
            }

            /* Rebuild the label in-place. Width chosen to match the worst-
             * case string so the sprite framebuffer never needs to grow. */
            for(i = 0; i < 24; i++){ buf2[i] = ' '; }
            buf2[0]='S'; buf2[1]='P'; buf2[2]='A'; buf2[3]='C'; buf2[4]='I'; buf2[5]='N'; buf2[6]='G'; buf2[7]=':';
            if(spacing >= 10){
                buf2[9] = '0' + (spacing / 10);
                buf2[10] = '0' + (spacing % 10);
            } else {
                buf2[9] = '0' + spacing;
                buf2[10] = ' ';
            }
            buf2[12]='I'; buf2[13]='N'; buf2[14]='V'; buf2[15]='E'; buf2[16]='R'; buf2[17]='T'; buf2[18]=':';
            buf2[20] = invert ? 'O' : 'O';
            buf2[21] = invert ? 'N' : 'F';
            buf2[22] = invert ? ' ' : 'F';
            /* No trailing '\0' inside the visible 24-byte window: textBox's
             * lineBreakAndWordWrap() walks i < tb->char_count and computes
             * char_widths[tb->text[i]-32], so an embedded NUL would index
             * the table at -32. The terminator already lives at
             * tb->text[char_count] from newTextBox's allocation. */
            for(i = 0; i < 24; i++){ labelTb->text[i] = buf2[i]; }
            updateLine(settings, mainFont, labelTb, NULL, 999999, 999999, GREEN);

            needRedraw = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            DrawHelp(HELP_DIAGONAL);
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_UP) && !(settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            spacingIdx = (spacingIdx + 1) & 3;
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_DOWN) && !(settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            spacingIdx = (spacingIdx + 3) & 3;
            needRedraw = 1;
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            invert ^= 1;
            needRedraw = 1;
        }

        if(extraExitPressed()){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    labelTb = freeTextBox(labelTb);
    teardownFullscreenSprite(s, buf);
}

/* ---------------------------------------------------------------------------
 * Video test: Vertical Scroll
 *
 * Mirror of the existing horizontal Scroll Test from tests.c, scrolling
 * top->bottom (or bottom->top) instead of side-to-side. Implemented
 * procedurally so it doesn't need an LZ77-packed asset.
 *
 * The pattern is a 240-line repeating stripe (16-pixel-tall colour bands
 * separated by 1-pixel white rules) that we shift by one pixel per frame
 * by changing the sprite's y origin. Vertical OP/blitter pacing bugs --
 * tearing, jitter, dropped lines -- show up here even when the existing
 * horizontal scroll looks clean, because horizontal and vertical fetch
 * paths exercise different parts of the OP list-walker.
 *
 * Controls:
 *   D-pad UP/DOWN    -- speed (1, 2, 4 px/frame; 0 = paused)
 *   D-pad LEFT/RIGHT -- reverse direction
 *   A                -- pause toggle
 *   OPTION           -- exit
 * --------------------------------------------------------------------------- */

/* HUD label rebuild for VertScrollTest, factored out so the per-frame loop
 * stays small enough not to trip GCC 4.x's reload pass on m68k at -O2. */
static void vertScrollUpdateLabel(textBox *tb, int speed, int dir, int paused){
    int i;
    char line[24];
    for(i = 0; i < 24; i++){ line[i] = ' '; }
    line[0]='S'; line[1]='P'; line[2]='E'; line[3]='E'; line[4]='D'; line[5]=':';
    line[7] = (char)('0' + (paused ? 0 : speed));
    line[9]='D'; line[10]='I'; line[11]='R'; line[12]=':';
    if(dir > 0){ line[14]='D'; line[15]='N'; }
    else       { line[14]='U'; line[15]='P'; }
    if(paused){
        line[18]='P'; line[19]='A'; line[20]='U'; line[21]='S'; line[22]='E';
    }
    /* No embedded NUL inside the visible 24-byte window -- see DrawDiagonal
     * label loop above. The terminator lives at tb->text[char_count]. */
    for(i = 0; i < 24; i++){ tb->text[i] = line[i]; }
    updateLine(settings, mainFont, tb, NULL, 999999, 999999, GREEN);
}

/* Stripe-pattern fill for the doubled framebuffer. Pulled out so the
 * caller's stack frame stays slim. */
static void vertScrollFillBuffer(uint16_t *buf, int W, int H, int BUF_H){
    static const uint16_t bands[6] = {
        COLOR_RED, COLOR_YELLOW, COLOR_GREEN,
        COLOR_CYAN, COLOR_BLUE, COLOR_MAGENTA
    };
    const int BAND_H = 16;
    int x, y;
    for(y = 0; y < BUF_H; y++){
        int yMod = y % H;
        int b = (yMod / BAND_H) % 6;
        uint16_t c = ((yMod % BAND_H) == 0) ? COLOR_WHITE : bands[b];
        for(x = 0; x < W; x++){
            buf[y * W + x] = c;
        }
    }
}

void VertScrollTest(void){
    const int W = 320;
    const int H = 240;
    const int BUF_H = 480;

    int exit = 0;
    int paused = 0;
    int speed = 1;
    int dir = 1;
    int yOff = 0;
    int needLabel = 1;

    uint16_t *buf;
    sprite *s;
    textBox *labelTb;

    settings->fadeToColor = 0x0000;

    buf = malloc(sizeof(uint16_t) * W * BUF_H);
    fillPACK_RGB16(buf, W * BUF_H, COLOR_BLACK);
    vertScrollFillBuffer(buf, W, H, BUF_H);

    s = new_sprite(W, BUF_H, 0, 0 + settings->PALOffset, DEPTH16, buf);
    s->trans = 0;
    attach_sprite_to_display_at_layer(s, settings->d, 13);

    labelTb = newTextBox("SPEED: 4 DIR: UP   PAUSE", 200, 9, mainFont, 0, settings->d, 8, 8, 14, 1);
    updateLine(settings, mainFont, labelTb, NULL, 999999, 999999, GREEN);

    hide_or_show_display_layer_range(settings->d, 1, 3, 15);

    while(!exit){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(!paused){
            yOff += dir * speed;
            if(yOff <= -H) yOff += H;
            if(yOff > 0)   yOff -= H;
            s->y = yOff + settings->PALOffset;
        }

        if(needLabel){
            vertScrollUpdateLabel(labelTb, speed, dir, paused);
            needLabel = 0;
        }

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if(settings->controllerLock != 0){
            continue;
        }

        if((settings->joy1 & JOYPAD_DOWN) && (settings->joy1 & JOYPAD_OPTION)){
            settings->controllerLock = 1;
            DrawHelp(HELP_VERTSCROLL);
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_UP)){
            settings->controllerLock = 1;
            if(speed < 4) speed <<= 1;
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_DOWN)){
            settings->controllerLock = 1;
            if(speed > 1) speed >>= 1;
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_LEFT)){
            settings->controllerLock = 1;
            dir = -1;
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_RIGHT)){
            settings->controllerLock = 1;
            dir = 1;
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_A)){
            settings->controllerLock = 1;
            paused ^= 1;
            needLabel = 1;
        }
        else if((settings->joy1 & JOYPAD_OPTION)){
            settings->controllerLock = 1;
            exit = 1;
        }
    }

    hide_or_show_display_layer_range(settings->d, 0, 3, 15);
    labelTb = freeTextBox(labelTb);
    teardownFullscreenSprite(s, buf);
}
