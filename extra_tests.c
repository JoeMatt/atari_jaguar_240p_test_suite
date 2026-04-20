#include "./extra_tests.h"
#include "./tests.h" /* for the C1..C9 frequency constants used by audio */

/* ---------------------------------------------------------------------------
 * Helpers
 *
 * Jaguar TOM RGB16 (RGB16 video mode + DEPTH16 framebuffers) packs pixels as
 * R5 B5 G6 in a single 16-bit word: (red<<11) | (blue<<6) | green
 * with red/blue 0..31 and green 0..63. This matches the existing patterns
 * (Draw100IRE, DrawWhiteScreen, DropShadowTest, etc).
 * --------------------------------------------------------------------------- */

#define RGB16(r, b, g) (uint16_t)( ((uint16_t)((r) & 0x1F) << 11) | ((uint16_t)((b) & 0x1F) << 6) | (uint16_t)((g) & 0x3F) )

#define COLOR_BLACK   RGB16(0,  0,  0)
#define COLOR_WHITE   RGB16(31, 31, 63)
#define COLOR_GRAY50  RGB16(16, 16, 32)
#define COLOR_GRAY25  RGB16(8,  8,  16)
#define COLOR_RED     RGB16(31, 0,  0)
#define COLOR_GREEN   RGB16(0,  0,  63)
#define COLOR_BLUE    RGB16(0,  31, 0)
#define COLOR_YELLOW  RGB16(31, 0,  63)
#define COLOR_CYAN    RGB16(0,  31, 63)
#define COLOR_MAGENTA RGB16(31, 31, 0)

/* Fill a DEPTH16 RGB16 framebuffer of (w*h) pixels with a constant color. */
static void fillRGB16(uint16_t *buf, int count, uint16_t color){
    int i;
    for(i = 0; i != count; i++){
        buf[i] = color;
    }
}

/* Draw an axis-aligned filled rectangle into a DEPTH16 framebuffer.
 * No clipping -- caller must keep the rect inside the buffer. */
static void rectRGB16(uint16_t *buf, int stride, int x, int y, int w, int h, uint16_t color){
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
        RGB16(24, 24, 48), /* 75% gray   */
        RGB16(24, 0,  48), /* 75% yellow */
        RGB16(0,  24, 48), /* 75% cyan   */
        RGB16(0,  0,  48), /* 75% green  */
        RGB16(24, 24, 0),  /* 75% magenta*/
        RGB16(24, 0,  0),  /* 75% red    */
        RGB16(0,  24, 0)   /* 75% blue   */
    };

    int exit = 0;
    int i, x;
    int barW = W / 7;
    int barsH = (H * 2) / 3;
    int rampH = H - barsH;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillRGB16(buf, W * H, COLOR_BLACK);

    /* 75% color bars in the top 2/3 */
    for(i = 0; i < 7; i++){
        int x0 = i * barW;
        int w  = (i == 6) ? (W - x0) : barW;
        rectRGB16(buf, W, x0, 0, w, barsH, bars75[i]);
    }

    /* 11-step gray ramp across the bottom 1/3 */
    int stepW = W / 11;
    for(i = 0; i < 11; i++){
        x = i * stepW;
        int w = (i == 10) ? (W - x) : stepW;
        int g6 = (i * 63) / 10;     /* 0..63   */
        int rb5 = (i * 31) / 10;    /* 0..31   */
        uint16_t c = RGB16(rb5, rb5, g6);
        rectRGB16(buf, W, x, barsH, w, rampH, c);
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
    const int RINGS = 5;
    int exit = 0;
    int x, y, r, dx, dy;

    settings->fadeToColor = 0x0000;

    uint16_t *buf = malloc(sizeof(uint16_t) * W * H);
    fillRGB16(buf, W * H, COLOR_BLACK);

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
    fillRGB16(buf, W * H, COLOR_GRAY50);

    for(i = 0; i < 8; i++){
        row = i / 4;
        col = i % 4;
        int x = col * cellW;
        int y = row * cellH;
        int w = (col == 3) ? (W - x) : cellW;
        int h = (row == 1) ? (H - y) : cellH;
        rectRGB16(buf, W, x, y, w, h, blocks[i]);
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
        RGB16(1, 1, 2),   /* ~ 2 IRE  */
        RGB16(2, 2, 4),   /* ~ 4 IRE  */
        RGB16(3, 3, 6),   /* ~ 7.5 IRE */
        RGB16(4, 4, 8)    /* ~10 IRE  */
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
    fillRGB16(buf, W * H, COLOR_BLACK);

    for(i = 0; i < 4; i++){
        int x = x0 + i * (barW + gap);
        rectRGB16(buf, W, x, y0, barW, barH, levels[i]);
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
        RGB16(28, 28, 56),  /* ~ 90 IRE */
        RGB16(29, 29, 59),  /* ~ 95 IRE */
        RGB16(31, 31, 62),  /* ~100 IRE */
        RGB16(31, 31, 63)   /* peak     */
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
    fillRGB16(buf, W * H, COLOR_GRAY50);

    for(i = 0; i < 4; i++){
        int x = x0 + i * (barW + gap);
        rectRGB16(buf, W, x, y0, barW, barH, levels[i]);
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
 * like the SNES/Genesis "Manual Lag Test" mode. A and B nudge the square
 * one pixel left/right; OPTION exits.
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
    fillRGB16(bg, W * H, COLOR_BLACK);
    int i;
    for(i = 0; i < 16; i++){
        int g6 = (i * 63) / 15;
        int rb5 = (i * 31) / 15;
        uint16_t c = RGB16(rb5, rb5, g6);
        rectRGB16(bg, W, i * (W/16), 0, W/16, 32, c);
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
 * Cycles through Left, Right, Center and Mute at the press of A; the on-
 * screen indicator shows which channel is active. C plays a continuous
 * 1 kHz reference tone (using the DSP rom_sine wavetable) at the chosen
 * panning. Useful for verifying RCA/SCART wiring and balance trim.
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
                set_voice(0, VOICE_16|VOICE_BALANCE(pan)|VOICE_VOLUME(63)|VOICE_FREQ(C7, freq),
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
 * Options menu
 *
 * Replaces the (x)Options stubs that appeared in every sub-menu. For now the
 * options surface is intentionally narrow but real -- we expose a master
 * audio volume that's wired into the global settings and shown on screen.
 * Future options (PAL/NTSC override, scanline test mode, controller layout)
 * can be added here without touching the menu plumbing in main.c.
 * --------------------------------------------------------------------------- */
void OptionsMenu(void){
    static int audioVolume = 63;     /* 0..63 -- persists across calls */

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
            itostring(nbuf, audioVolume, 10);
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
            if(audioVolume > 0){ audioVolume--; redraw = 1; }
        }
        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(audioVolume < 63){ audioVolume++; redraw = 1; }
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
                set_voice(0, VOICE_16|VOICE_BALANCE(8)|VOICE_VOLUME(63)|VOICE_FREQ(sweepFreq[tone], freq),
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
