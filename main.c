#include "./main.h"

int main () {
  
    int i = 0;
    int lastMenuLine = 8; //counting from zero (Screen Savers added as slot 5)
  
    TOMREGS->vmode = RGB16|CSYNC|BGEN|PWIDTH4|VIDEN;

    init_interrupts();
    init_display_driver();
  
    settings = initGlobalSettings();
    settings->d = new_display(0);
        settings->d->x = (settings->PALNTSC ? 19 : 14);
        settings->d->y = 0;
        
    gpu_addr = &_GPU_FREE_RAM;
    lz77_init(gpu_addr);
    
    freq = 16000;
    freq = init_sound_driver(freq);
    
//     debugConsole = open_custom_console(settings->d,8,12,45,40,4,15);
    
    //init fontload
    //load palette
    memcpy((void*)TOMREGS->clut1, &sonicPal, 128);
    
    int fontMapWidths[95];
    int fontMapOffsets[95];
    
    for(i = 0; i != 95; i++){
        fontMapWidths[i] = 5;
        fontMapOffsets[i] = i * 5;
    } 
    
    //load font
    fontMapWhiteData = malloc(sizeof(uint8_t)*512*8);
    fontMapGreenData = malloc(sizeof(uint8_t)*512*8);
    fontMapRedData = malloc(sizeof(uint8_t)*512*8);
    fontMapGreyData = malloc(sizeof(uint8_t)*512*8);
    lz77_unpack(gpu_addr, &fontMap, (uint8_t*)fontMapWhiteData);
    lz77_unpack(gpu_addr, &fontMap, (uint8_t*)fontMapGreenData);
    lz77_unpack(gpu_addr, &fontMap, (uint8_t*)fontMapRedData);
    lz77_unpack(gpu_addr, &fontMap, (uint8_t*)fontMapGreyData);
    
    //adjust colors for highlights
    for(i = 0; i != 4096; i++){
        uint8_t byte = 0x00;
        //green
        memcpy(&byte, (uint8_t*)fontMapGreenData+i, sizeof(byte));
        if(byte == 0x01){
            memset((uint8_t*)fontMapGreenData+i, 0x02, sizeof(byte));
        }
        //red
        memcpy(&byte, (uint8_t*)fontMapRedData+i, sizeof(byte));
        if(byte == 0x01){
            memset((uint8_t*)fontMapRedData+i, 0x03, sizeof(byte));
        }
        //grey
        memcpy(&byte, (uint8_t*)fontMapGreyData+i, sizeof(byte));
        if(byte == 0x01){
            memset((uint8_t*)fontMapGreyData+i, 0x05, sizeof(byte));
        }
    }
    
    mainFont = newFont(8, 95, fontMapWidths, fontMapOffsets, 512, fontMapWhiteData, fontMapGreyData);
    mainFont->kerning = 1;
    
    for(i = 0; i != LINESOFTEXT; i++){
        lineTextBox[i] = newTextBox("       ", 256, 11, mainFont, 0, settings->d, settings->lineXOffset, setLineYPos(i), 2, 1);
        drawTextBoxAtOnce(lineTextBox[i]);
    }
    
    loadMainMenuLines(1);
    
    //set random colors in color lookup table
    for(i = 0; i != 20; i++){
        uint16_t red = (rand()%19+6) << 11;
        uint16_t blue = (rand()%12) << 6;
        uint16_t green = rand()%16;
        settings->bgColorClut[i] = red | blue | green;
    }
    
    settings->fadeToColor = 0x0000;
    
    //setup background image
    backData = malloc(sizeof(uint8_t)*(320*240*2));
    lz77_unpack(gpu_addr, &back, (uint8_t*)backData);
    backSprite = new_sprite(320, 240, 0, 0 + settings->PALOffset, DEPTH16, backData);
    attach_sprite_to_display_at_layer(backSprite, settings->d, 0);
    backSprite->trans = 0;
    
    SDData = malloc(sizeof(uint8_t)*(56*101*2));
    lz77_unpack(gpu_addr, &SD, (uint8_t*)SDData);
    SDSprite = new_sprite(56, 101, 216, 71 + settings->PALOffset, DEPTH16, SDData);
    attach_sprite_to_display_at_layer(SDSprite, settings->d, 1);
    
    //PAL/NTSC check - PAL = 0, NTSC = 1
    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
    
    show_display(settings->d); 
    
    while(1){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
//         debugConsole->seek(debugConsole,0,SEEK_SET);
//         fprintf(debugConsole,"%d",MODE_D);
        
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 5;
            }
        }
        
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine+1){
                settings->menuState++;
            }
            else{
                settings->menuState = 1;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }

        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
            else{
                settings->menuState = lastMenuLine;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }
        
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            
            /* Remember which main-menu slot launched the sub-menu so we can
             * restore the highlight on it when the user comes back, instead
             * of forcing the cursor back to "Test Patterns" every time. */
            int returnSlot = settings->menuState;
        
            switch(settings->menuState){
                
                //test pattern menu
                    
                //test pattern menu
                case 1:
                    
                    testPatternMenu();

                break;

                //Video Tests
                case 2:

                    VideoTestsMenu();
                    
                break;
                
                //Audio Tests
                case 3:
                    
                    AudioTestsMenu();
                    
                break;
                    
                //Hardware Tests
                case 4:
                    
                    HardwareMenu();
                    
                break;
                    
                //Screen Savers
                case 5:

                    ScreenSaversMenu();

                break;
                    
                //Help Menu
                case 6:  

                    hide_or_show_display_layer_range(settings->d, 0, 0, 15);
                    hide_or_show_display_layer_range(settings->d, 1, 14, 14);
                    settings->controllerLock = 1;
                    DrawHelp(HELP_GENERAL);
                    hide_or_show_display_layer_range(settings->d, 0, 14, 14);
                    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
                    
                break;
                    
                //Options Menu
                case 7:

                    OptionsMenu();

                break;
                    
                //Credits
                case 8:

                    SDSprite->invisible = 1;
                    drawCredits();
                    SDSprite->invisible = 0;

                break;
                
                default:
                    TOMREGS->bg = 0x000F;
                
            }
            
            /* Restore the highlight to whichever slot the user activated and
             * redraw the main menu. loadMainMenuLines() also wipes every
             * leftover sub-menu slot before painting (see its body for why). */
            settings->menuState = returnSlot;
            loadMainMenuLines(returnSlot);
            
            /* Drain whatever button(s) are still held from the sub-menu's
             * exit (typically A on "Back to Main Menu", or A inside a test
             * that returns on its own). Without this, scrollLock counts
             * down to zero a few frames later and silently auto-fires A
             * here, which re-enters whichever sub-menu the cursor is on
             * and looks to the user like the highlight or focus jumped to
             * a wrong slot on its own. */
            while((settings->joy1 & 0xFFFFFF) != 0){
                read_joypad_state(settings->j_state);
                settings->joy1 = settings->j_state->j1;
                vsync();
            }
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        
        }
    
    }
  
};

void loadMainMenuLines(int highlightLine){
    
    hide_display_layer(settings->d, 2);
    vsync();
    vsync();
    
    /* Set the main menu's coordinate band BEFORE wiping every line slot.
     * Sub-menus use their own (smaller) lineXOffset / lineYOffset and may
     * touch up to LINESOFTEXT-1 slots; if we reset with stale sub-menu
     * offsets, slots 8..LINESOFTEXT-1 stay parked at the sub-menu's
     * positions (still showing the previously-highlighted red "Back"
     * entry, etc). Resetting at the main-menu offsets relocates every
     * leftover slot into the same consistent band as the items we are
     * about to draw, so the blanked " " sprites overwrite the old text
     * cleanly and nothing red lingers in the middle of the screen. */
    settings->lineXOffset = 64;
    settings->lineYOffset = 88 + settings->PALOffset;
    
    resetAllLines();
    
    /* Clamp to the legal slot range so a stale settings->menuState from a
     * sub-menu that grew past 8 (e.g. testPatternMenu's 16) can never paint
     * RED on a slot that doesn't exist on the main menu. */
    if(highlightLine < 1 || highlightLine > 8){
        highlightLine = 1;
    }
    
    updateLine(settings, mainFont, lineTextBox[1], "Test Patterns",  settings->lineXOffset, setLineYPos(0), highlightLine == 1 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[2], "Video Tests",    settings->lineXOffset, setLineYPos(1), highlightLine == 2 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "Audio Tests",    settings->lineXOffset, setLineYPos(2), highlightLine == 3 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "Hardware Tools", settings->lineXOffset, setLineYPos(3), highlightLine == 4 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[5], "Screen Savers",  settings->lineXOffset, setLineYPos(4), highlightLine == 5 ? RED : WHITE);
    
    updateLine(settings, mainFont, lineTextBox[6], "Help",    settings->lineXOffset, setLineYPos(6), highlightLine == 6 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[7], "Options", settings->lineXOffset, setLineYPos(7), highlightLine == 7 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[8], "Credits", settings->lineXOffset, setLineYPos(8), highlightLine == 8 ? RED : WHITE);
    
    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
       
    show_display_layer(settings->d, 2);
    
};

/* Draw the Test Patterns menu's text lines. Extracted so the parent menu can
 * redraw itself after morePatternsMenu() (which shares lineTextBox[]) returns. */
static void drawTestPatternMenuLines(int highlightLine){
    settings->lineXOffset = 38;
    settings->lineYOffset = 52 + settings->PALOffset;

    resetAllLines();

    updateLine(settings, mainFont, lineTextBox[1], "Pluge", settings->lineXOffset, setLineYPos(0), highlightLine == 1 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[2], "Color Bars", settings->lineXOffset, setLineYPos(1), highlightLine == 2 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "EBU Color Bars", settings->lineXOffset, setLineYPos(2), highlightLine == 3 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "SMPTE Color Bars", settings->lineXOffset, setLineYPos(3), highlightLine == 4 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[5], "Referenced Color Bars", settings->lineXOffset, setLineYPos(4), highlightLine == 5 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[6], "Color Bleed Check", settings->lineXOffset, setLineYPos(5), highlightLine == 6 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[7], "Monoscope", settings->lineXOffset, setLineYPos(6), highlightLine == 7 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[8], "Grid", settings->lineXOffset, setLineYPos(7), highlightLine == 8 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[9], "Gray Ramp", settings->lineXOffset, setLineYPos(8), highlightLine == 9 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[10], "White & RGB Screens", settings->lineXOffset, setLineYPos(9), highlightLine == 10 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[11], "100 IRE", settings->lineXOffset, setLineYPos(10), highlightLine == 11 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[12], "Sharpness", settings->lineXOffset, setLineYPos(11), highlightLine == 12 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[13], "Overscan", settings->lineXOffset, setLineYPos(12), highlightLine == 13 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[14], "Convergence", settings->lineXOffset, setLineYPos(13), highlightLine == 14 ? RED : WHITE);
    updateLine(settings, mainFont, lineTextBox[15], "More Patterns...", settings->lineXOffset, setLineYPos(14), highlightLine == 15 ? RED : WHITE);

    updateLine(settings, mainFont, lineTextBox[16], "Back to Main Menu", settings->lineXOffset, setLineYPos(15), highlightLine == 16 ? RED : WHITE);

    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
}

void testPatternMenu(){

    int done = 0;
    /* Original 14 patterns + "More Patterns..." entry + "Back" = 16 lines.
     * The 5 new procedural patterns live behind morePatternsMenu() so this
     * menu still fits inside the 240p safe area without overlap. */
    int lastMenuLine = 16; //counting from zero
    settings->menuState = 1;

    hide_display_layer(settings->d, 2);
    vsync();

    drawTestPatternMenuLines(1);

    show_display_layer(settings->d, 2);
        
    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }
        
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine+1){
                settings->menuState++;
            }
			else{
				settings->menuState = 1;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}

		if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
			else{
				settings->menuState = lastMenuLine;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}
		
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            
            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }
        
            switch(settings->menuState){
                    
                //DrawPluge
                case 1:
                    
                    DrawPluge();
                    
                break;
                
                //DrawColorBars
                case 2:
                
                    DrawColorBars();
                    
                break;
                
                //DrawEBU
                case 3:
                    
                    DrawEBU();
                    
                break;
                    
                //DrawSMPTE
                case 4:
                    
                    DrawSMPTE();
                    
                break;
                    
                //Draw601ColorBars
                case 5:  
                    
                    Draw601ColorBars();
                    
                break;
                    
                //DrawCoorBleed
                case 6:
                    
                    DrawCoorBleed();
                    
                break;
                    
                //DrawMonoScope
                case 7:
                    
                    DrawMonoScope();
                    
                break;
                    
                //DrawGrid
                case 8:
                    
                    DrawGrid();
                    
                break;
                    
                //DrawGreyRamp
                case 9:
                    
                    DrawGreyRamp();
                    
                break;
                    
                //DrawWhiteScreen
                case 10:
                    DrawWhiteScreen();
                break;
                    
                //Draw100IRE
                case 11:
                    
                    Draw100IRE();
                    
                break;
                    
                //DrawSharpness
                case 12:
                    
                    DrawSharpness();
                    
                break;
                    
                //DrawOverscan
                case 13:
                    
                    DrawOverscan();
                    
                break;
                    
                //DrawConvergence
                case 14:
                    
                    DrawConvergence();
                    
                break;

                //More Patterns submenu
                case 15:

                    morePatternsMenu();
                    /* morePatternsMenu shares lineTextBox[]; redraw our own
                     * menu state before the loop continues. */
                    hide_display_layer(settings->d, 2);
                    vsync();
                    drawTestPatternMenuLines(15);
                    show_display_layer(settings->d, 2);

                break;

                //return to main menu
                case 16:
                    
                    done = 1;
                    
                break;
                
                default:
                    TOMREGS->bg = 0x000F;
                
            }
        
        }
        
        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
        
    }
    
};

/* Sub-menu for the procedurally-generated patterns added to the Jaguar build,
 * separated from testPatternMenu so neither page overflows the 240p safe area. */
void morePatternsMenu(){

    int done = 0;
    int lastMenuLine = 6; //counting from zero
    settings->menuState = 1;

    hide_display_layer(settings->d, 2);
    vsync();

    settings->lineXOffset = 38;
    settings->lineYOffset = 80 + settings->PALOffset;

    resetAllLines();

    updateLine(settings, mainFont, lineTextBox[1], "Color Bars w/ Gray", settings->lineXOffset, setLineYPos(0), RED);
    updateLine(settings, mainFont, lineTextBox[2], "Linearity", settings->lineXOffset, setLineYPos(1), WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "Phase", settings->lineXOffset, setLineYPos(2), WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "Brightness", settings->lineXOffset, setLineYPos(3), WHITE);
    updateLine(settings, mainFont, lineTextBox[5], "Contrast", settings->lineXOffset, setLineYPos(4), WHITE);

    updateLine(settings, mainFont, lineTextBox[6], "Back to Test Patterns", settings->lineXOffset, setLineYPos(6), WHITE);

    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);

    show_display_layer(settings->d, 2);

    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }

        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine + 1){
                settings->menuState++;
            }
            else{
                settings->menuState = 1;
            }
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }

        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
            else{
                settings->menuState = lastMenuLine;
            }
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;

            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }

            switch(settings->menuState){

                case 1: DrawColorBarsGray(); break;
                case 2: DrawLinearity();    break;
                case 3: DrawPhase();        break;
                case 4: DrawBrightness();   break;
                case 5: DrawContrast();     break;

                //return to Test Patterns menu
                case 6:
                    done = 1;
                break;

                default:
                    TOMREGS->bg = 0x000F;
            }
        }

        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
    }
};

/* Screen Savers sub-menu. Mirrors the Screensavers section that ships in
 * the canonical 240p test suite (Genesis / SNES / Dreamcast). All three
 * patterns are procedural (see extra_tests.c) so this menu doesn't drag
 * any new LZ77 assets into the cart. Layout follows morePatternsMenu()'s
 * conventions for consistency. */
void ScreenSaversMenu(){

    int done = 0;
    int lastMenuLine = 4; //counting from zero
    settings->menuState = 1;

    hide_display_layer(settings->d, 2);
    vsync();

    settings->lineXOffset = 38;
    settings->lineYOffset = 80 + settings->PALOffset;

    resetAllLines();

    updateLine(settings, mainFont, lineTextBox[1], "Color Cycle",      settings->lineXOffset, setLineYPos(0), RED);
    updateLine(settings, mainFont, lineTextBox[2], "Bouncing Square",  settings->lineXOffset, setLineYPos(1), WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "Scrolling Bars",   settings->lineXOffset, setLineYPos(2), WHITE);

    updateLine(settings, mainFont, lineTextBox[4], "Back to Main Menu", settings->lineXOffset, setLineYPos(4), WHITE);

    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);

    show_display_layer(settings->d, 2);

    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }

        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine + 1){
                settings->menuState++;
            }
            else{
                settings->menuState = 1;
            }
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }

        if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
            else{
                settings->menuState = lastMenuLine;
            }
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
        }

        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;

            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }

            switch(settings->menuState){

                case 1: ColorCycleSaver();     break;
                case 2: BouncingSquareSaver(); break;
                case 3: ScrollingBarsSaver();  break;

                //return to main menu
                case 4:
                    done = 1;
                break;

                default:
                    TOMREGS->bg = 0x000F;
            }
        }

        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
    }
};

void VideoTestsMenu(){
    
    int done = 0;
    int lastMenuLine = 14; //counting from zero
    settings->menuState = 1;
    
    hide_display_layer(settings->d, 2);
    vsync();
    
    settings->lineXOffset = 38;
    settings->lineYOffset = 56 + settings->PALOffset;
    
    resetAllLines();
        
    updateLine(settings, mainFont, lineTextBox[1], "Drop Shadow Test", settings->lineXOffset, setLineYPos(0), RED);
    updateLine(settings, mainFont, lineTextBox[2], "Striped Sprite Test", settings->lineXOffset, setLineYPos(1), WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "Lag Test", settings->lineXOffset, setLineYPos(2), WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "Manual Lag Test", settings->lineXOffset, setLineYPos(3), WHITE);
    updateLine(settings, mainFont, lineTextBox[5], "Timing & Reflex Test", settings->lineXOffset, setLineYPos(4), WHITE);
    updateLine(settings, mainFont, lineTextBox[6], "Scroll Test", settings->lineXOffset, setLineYPos(5), WHITE);
    updateLine(settings, mainFont, lineTextBox[7], "Grid Scroll Test", settings->lineXOffset, setLineYPos(6), WHITE);
    updateLine(settings, mainFont, lineTextBox[8], "Horiz/Vert Stripes", settings->lineXOffset, setLineYPos(7), WHITE);
    updateLine(settings, mainFont, lineTextBox[9], "Checkerboard", settings->lineXOffset, setLineYPos(8), WHITE);
    updateLine(settings, mainFont, lineTextBox[10], "Backlit Zone Test", settings->lineXOffset, setLineYPos(9), WHITE);
    updateLine(settings, mainFont, lineTextBox[11], "Alternate 240p/480i", settings->lineXOffset, setLineYPos(10), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[12], "Help", settings->lineXOffset, setLineYPos(12), WHITE);
    updateLine(settings, mainFont, lineTextBox[13], "Options", settings->lineXOffset, setLineYPos(13), WHITE);
    updateLine(settings, mainFont, lineTextBox[14], "Back to Main Menu", settings->lineXOffset, setLineYPos(14), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
       
    show_display_layer(settings->d, 2);
        
    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }
        
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine+1){
                settings->menuState++;
            }
			else{
				settings->menuState = 1;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}

		if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
			else{
				settings->menuState = lastMenuLine;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}
		
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            
            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }
        
            switch(settings->menuState){
                    
                //DropShadowTest
                case 1:
                    
                    DropShadowTest(0);
                    
                break;
                
                //StripedSpriteTest
                case 2:
                    
                    DropShadowTest(1);
                    
                break;
                
                //PassiveLagTest
                case 3:
                    
                    PassiveLagTest();
                    
                break;

                //ManualLagTest
                case 4:

                    ManualLagTest();

                break;
                    
                //ReflexNTiming
                case 5:
                    
                    ReflexNTiming();
                    
                break;
                    
                //HScrollTest
                case 6:  
                    
                    HScrollTest();
                    
                break;
                    
                //VScrollTest
                case 7:
                    
                    VScrollTest();
                    
                break;
                    
                //DrawStripes
                case 8:
                    
                    DrawStripes();
                    
                break;
                    
                //DrawCheckBoard
                case 9:
                    
                    DrawCheckBoard();
                    
                break;
                    
                //LEDZoneTest
                case 10:
                    
                    LEDZoneTest();
                    
                break;
                    
                //Alternate240p480i
                case 11:

                    Alternate240p480iTest();

                break;
                    
                //DrawHelp
                case 12:

                    hide_or_show_display_layer_range(settings->d, 0, 0, 15);
                    hide_or_show_display_layer_range(settings->d, 1, 14, 14);
                    settings->controllerLock = 1;
                    DrawHelp(HELP_GENERAL);
                    hide_or_show_display_layer_range(settings->d, 0, 14, 14);
                    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
                    
                break;
                    
                //OptionsMenu
                case 13:

                    OptionsMenu();

                break;
                    
                //return to main menu
                case 14:
                    
                    done = 1;
                    
                break;
                
                default:
                    TOMREGS->bg = 0x000F;
                
            }
        
        }
        
        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
        
    }
    
};

void AudioTestsMenu(){
    
    int done = 0;
    int lastMenuLine = 7; //counting from zero
    settings->menuState = 1;
    
    hide_display_layer(settings->d, 2);
    vsync();
    
    settings->lineXOffset = 38;
    settings->lineYOffset = 86 + settings->PALOffset;
    
    resetAllLines();
        
    updateLine(settings, mainFont, lineTextBox[1], "Sound Test", settings->lineXOffset, setLineYPos(0), RED);
    updateLine(settings, mainFont, lineTextBox[2], "Audio Sync Test", settings->lineXOffset, setLineYPos(1), WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "L/R Balance + 1kHz Tone", settings->lineXOffset, setLineYPos(2), WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "MDFourier Sweep", settings->lineXOffset, setLineYPos(3), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[5], "Help", settings->lineXOffset, setLineYPos(5), WHITE);
    updateLine(settings, mainFont, lineTextBox[6], "Options", settings->lineXOffset, setLineYPos(6), WHITE);
    updateLine(settings, mainFont, lineTextBox[7], "Back to Main Menu", settings->lineXOffset, setLineYPos(7), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
       
    show_display_layer(settings->d, 2);
        
    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }
        
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine + 1){
                settings->menuState++;
            }
			else{
				settings->menuState = 1;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}

		if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
			else{
				settings->menuState = lastMenuLine;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}
		
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            
            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }
        
            switch(settings->menuState){
                    
                //SoundTest
                case 1:
                    
                    SoundTest();
                    
                break;
                    
                //AudioSyncTest
                case 2:
                    
                    AudioSyncTest();
                    
                break;

                //AudioBalanceTest
                case 3:

                    AudioBalanceTest();

                break;
                    
                //MDFourier
                case 4:
                    
                    MDFourierTest();
                    
                break;
                    
                //DrawHelp
                case 5:

                    hide_or_show_display_layer_range(settings->d, 0, 0, 15);
                    hide_or_show_display_layer_range(settings->d, 1, 14, 14);
                    settings->controllerLock = 1;
                    DrawHelp(HELP_GENERAL);
                    hide_or_show_display_layer_range(settings->d, 0, 14, 14);
                    hide_or_show_display_layer_range(settings->d, 1, 0, 15);
                    
                break;
                    
                //OptionsMenu
                case 6:

                    OptionsMenu();

                break;
                    
                case 7:
                    
                    done = 1;
                    
                break;
                
                default:
                    TOMREGS->bg = 0x000F;
                
            }
        
        }
        
        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
        
    }
    
};

void HardwareMenu(){
    
    int done = 0;
    int lastMenuLine = 9; //counting from zero
    settings->menuState = 1;
    
    hide_display_layer(settings->d, 2);
    vsync();
    
    settings->lineXOffset = 38;
    settings->lineYOffset = 76 + settings->PALOffset;
    
    resetAllLines();
        
    updateLine(settings, mainFont, lineTextBox[1], "Controller Test", settings->lineXOffset, setLineYPos(0), RED);
    updateLine(settings, mainFont, lineTextBox[2], "GPU Memory Viewer", settings->lineXOffset, setLineYPos(1), WHITE);
    updateLine(settings, mainFont, lineTextBox[3], "DSP Memory Viewer", settings->lineXOffset, setLineYPos(2), WHITE);
    updateLine(settings, mainFont, lineTextBox[4], "DRAM Memory Viewer", settings->lineXOffset, setLineYPos(3), WHITE);
    updateLine(settings, mainFont, lineTextBox[5], "System Info", settings->lineXOffset, setLineYPos(4), WHITE);
    updateLine(settings, mainFont, lineTextBox[6], "Jaguar CD Probe", settings->lineXOffset, setLineYPos(5), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[7], "Help", settings->lineXOffset, setLineYPos(7), WHITE);
    updateLine(settings, mainFont, lineTextBox[8], "Options", settings->lineXOffset, setLineYPos(8), WHITE);
    updateLine(settings, mainFont, lineTextBox[9], "Back to Main Menu", settings->lineXOffset, setLineYPos(9), WHITE);
    
    updateLine(settings, mainFont, lineTextBox[0], settings->PALNTSC ? "NTSC VDP 320x240p" : "PAL VDP 320x288p", 184, 200 + settings->PALOffset, WHITE);
       
    show_display_layer(settings->d, 2);
        
    while(!done){
        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
            settings->scrollLock = 12;
        }
        else if(settings->scrollLock > 0){
            settings->scrollLock--;
            if(settings->scrollLock == 0){
                settings->controllerLock = 0;
                settings->scrollLock = 2;
            }
        }
        
        if((settings->joy1 & JOYPAD_DOWN) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState + 1) < lastMenuLine + 1){
                settings->menuState++;
            }
			else{
				settings->menuState = 1;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}

		if((settings->joy1 & JOYPAD_UP) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            settings->menuStateOld = settings->menuState;
            if((settings->menuState - 1) > 0){
                settings->menuState--;
            }
			else{
				settings->menuState = lastMenuLine;
            }
            
            //visually update menu
            updateLine(settings, mainFont, lineTextBox[settings->menuStateOld], '\0', 999999, 999999, WHITE);
            updateLine(settings, mainFont, lineTextBox[settings->menuState], '\0', 999999, 999999, RED);
            
		}
		
        if((settings->joy1 & JOYPAD_A) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            
            if(settings->menuState != lastMenuLine){
                hide_or_show_display_layer_range(settings->d, 0, 0, 15);
            }
        
            switch(settings->menuState){
                    
                //controller test
                case 1:
                    
                    ControllerTest();
                    
                break;
                    
                //GPU test
                case 2:
                    
                    GPURAMTest();
                    
                break;
                    
                //DSP test
                case 3:
                    
                    DSPRAMTest();
                    
                break;
                    
                case 4:
                    
                    DRAMTest();
                    
                break;
                    
                //System Info
                case 5:

                    HardwareInfo();

                break;
                    
                //Jaguar CD Tests
                case 6:
                    
                    JaguarCDTest();
                    
                break;
                    
                //Help
                case 7:

                    hide_or_show_display_layer_range(settings->d, 0, 0, 15);
                    hide_or_show_display_layer_range(settings->d, 1, 14, 14);
                    settings->controllerLock = 1;
                    DrawHelp(HELP_GENERAL);
                    hide_or_show_display_layer_range(settings->d, 0, 14, 14);
                    hide_or_show_display_layer_range(settings->d, 1, 0, 15);

                break;
                    
                //Options
                case 8:

                    OptionsMenu();

                break;
                    
                case 9:
                    
                    done = 1;
                    
                break;
                
                default:
                    TOMREGS->bg = 0x000F;
                
            }
        
        }
        
        hide_or_show_display_layer_range(settings->d, 1, 0, 2);
        
    }
    
};

void drawCredits(){

    int page = 0;
    int totalPages = 1;
    int exit = 0;

    int redraw = 1;

    settings->lineXOffset = 32;
    settings->lineYOffset = 52 + settings->PALOffset;

    while(!exit){

        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();

        if(redraw){

            hide_display_layer(settings->d, 2);
            vsync();

            resetAllLines();

            switch(page){

                case 0:

                    updateLine(settings, mainFont, lineTextBox[1], "Code:", settings->lineXOffset, setLineYPos(0), GREEN);
                    updateLine(settings, mainFont, lineTextBox[2], " Artemio Urbina @Artemio", settings->lineXOffset, setLineYPos(1), WHITE);
                    updateLine(settings, mainFont, lineTextBox[3], " William Thorup @BitJag", settings->lineXOffset, setLineYPos(2), WHITE);
                    updateLine(settings, mainFont, lineTextBox[4], "Patterns:", settings->lineXOffset, setLineYPos(3), GREEN);
                    updateLine(settings, mainFont, lineTextBox[5], " Artemio Urbina", settings->lineXOffset, setLineYPos(4), WHITE);
                    updateLine(settings, mainFont, lineTextBox[6], "SDK:", settings->lineXOffset, setLineYPos(5), GREEN);
                    updateLine(settings, mainFont, lineTextBox[7], " https://github.com/theRemovers/rmvlib", settings->lineXOffset, setLineYPos(6), WHITE);
                    updateLine(settings, mainFont, lineTextBox[8], "Monoscope Pattern:", settings->lineXOffset, setLineYPos(7), GREEN);
                    updateLine(settings, mainFont, lineTextBox[9], " Keith Raney", settings->lineXOffset, setLineYPos(8), WHITE);
                    updateLine(settings, mainFont, lineTextBox[10], "Donna Art:", settings->lineXOffset, setLineYPos(9), GREEN);
                    updateLine(settings, mainFont, lineTextBox[11], " Jose Salot @pepe_salot", settings->lineXOffset, setLineYPos(10), WHITE);
                    updateLine(settings, mainFont, lineTextBox[12], "Menu Art:", settings->lineXOffset, setLineYPos(11), GREEN);
                    updateLine(settings, mainFont, lineTextBox[13], " Asher", settings->lineXOffset, setLineYPos(12), WHITE);
                    updateLine(settings, mainFont, lineTextBox[14], "Jaguar Port Updates:", settings->lineXOffset, setLineYPos(13), GREEN);
                    updateLine(settings, mainFont, lineTextBox[15], " Joe Mattiello @JoeMatt", settings->lineXOffset, setLineYPos(14), WHITE);
                    updateLine(settings, mainFont, lineTextBox[16], "Advisor:", settings->lineXOffset, setLineYPos(15), GREEN);
                    updateLine(settings, mainFont, lineTextBox[17], "  ()  ", settings->lineXOffset, setLineYPos(16), WHITE);

                    updateLine(settings, mainFont, lineTextBox[18], "                   Ver. 0.6.6 - 04/19/2026", settings->lineXOffset, setLineYPos(0) - 11, GREEN);

                    updateLine(settings, mainFont, lineTextBox[19], "Option - Return To Main Menu", settings->lineXOffset, setLineYPos(18) + 4, WHITE);
                break;

            }

            show_display_layer(settings->d, 2);
            vsync();
            redraw = 0;

        }

        //controller
        if((settings->joy1 & 0xFFFFFF) == 0){
            settings->controllerLock = 0;
        }

        if((settings->joy1 & JOYPAD_RIGHT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if((page + 1) < totalPages){
                page++;
                redraw = 1;
            }
        }

        if((settings->joy1 & JOYPAD_LEFT) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            if(page != 0){
                page--;
                redraw = 1;
            }
        }

        if((settings->joy1 & JOYPAD_OPTION) && settings->controllerLock == 0){
            settings->controllerLock = 1;
            exit = 1;
        }

    }

};
