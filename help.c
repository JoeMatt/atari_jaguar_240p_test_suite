#include "./help.h"

/* helpStrlen() / helpStrstrIndex()
 *
 * Tiny in-house substring search. The Jaguar SDK's jlibc ships strlen() but
 * not strstr(), so we roll our own naive O(n*m) scan. Only ever called
 * during help-screen redraws on user-pressed OPTION+DOWN, so the
 * worst-case ~360-char haystack search is irrelevant for performance.
 * helpStrstrIndex() returns the byte index of the first occurrence of
 * `needle` in `hay`, or -1 if not found. NULL inputs are treated as not-
 * found. */
static int helpStrlen(const char *s){
    int n = 0;
    while(s[n] != '\0'){ n++; }
    return n;
}

static int helpStrstrIndex(const char *hay, const char *needle){
    int i, j;
    if(hay == NULL || needle == NULL){ return -1; }
    if(needle[0] == '\0'){ return 0; }
    for(i = 0; hay[i] != '\0'; i++){
        for(j = 0; needle[j] != '\0' && hay[i + j] == needle[j]; j++){ /* match */ }
        if(needle[j] == '\0'){ return i; }
        if(hay[i + j] == '\0'){ return -1; }
    }
    return -1;
}

/* highlightHelpPhrase()
 *
 * Recolors `phrase` inside `tb`'s currently-set text by locating it at
 * runtime rather than by hard-coding a character offset.
 *
 * Rationale: textRangeColorChange() takes an absolute character index, so
 * any edit to the surrounding help string (typo fix, rewording, translation)
 * silently misaligns the highlight onto the wrong characters. Locating the
 * substring at runtime keeps the highlight pinned to the intended phrase
 * regardless of upstream edits. Caught by Qodo on PR #3 after the
 * "Evalute" -> "Evaluate" + "procesors" -> "processors" typo fixes shifted
 * "DOWN + OPTION" two characters to the right of its old offset (291).
 *
 * No-op (and safe) if the phrase isn't found, e.g. after a future rewrite. */
static void highlightHelpPhrase(textBox *tb, const char *phrase){
    const char *base;
    int start;

    if(tb == NULL || tb->text == NULL || phrase == NULL){ return; }
    base = (const char *)tb->text;
    start = helpStrstrIndex(base, phrase);
    if(start < 0){ return; }
    textRangeColorChange(tb, start, helpStrlen(phrase), WHITE, GREEN);
}

void DrawHelp(int option){
    
    int i = 0;
    int exit = 0;
    int redraw = 1;
    int page = 0;
    int totalPages = 1;
    
    hide_or_show_display_layer_range(settings->d, 0, 14, 15);
    
    //add background for help screen
    helpData = malloc(sizeof(uint8_t) * 280 * 198 * 2);
    lz77_unpack(gpu_addr, &help, (uint8_t*)helpData);
    helpSprite = new_sprite(280, 198, 20, 22 + settings->PALOffset, DEPTH16, helpData);
    attach_sprite_to_display_at_layer(helpSprite, settings->d, 14);
    
    //init textBoxes
    //about 41~42 characters per line
    for(i = 0 ; i != HELPTOTALLINES; i++){
        helpLineTextBox[i] = newTextBox("       ", 320, 9, mainFont, 0, settings->d, helpSprite->x + 10, helpSprite->y + 25 + (i * 9), 15, 1);
    }
    //set line positions for consitent text
    updateLine(settings, mainFont, helpLineTextBox[0], "        ", helpSprite->x + 40, helpSprite->y + 23, GREEN);
    updateLine(settings, mainFont, helpLineTextBox[1], "        ", helpSprite->x + 196, helpSprite->y + 170, WHITE);
    updateLine(settings, mainFont, helpLineTextBox[2], "Press Option to Exit Help", helpSprite->x + 64, helpSprite->y + 182, WHITE);
    
    //Used for the actual informational text
    helpTextBox = newTextBox("       ", 256, 320, mainFont, 0, settings->d, helpSprite->x + 10, helpSprite->y + 25 + (2 * 9), 15, 1);
    
    //set total pages depending on help screen
    switch(option){
        
        case HELP_BARS:
        case HELP_BLEED:
        case HELP_IRE:
        case HELP_601CB:
        case HELP_SHARPNESS:
        case HELP_OVERSCAN:
        case HELP_CHECK:
        case HELP_WHITE:
        case HELP_GRAY:
        case HELP_SMPTE:
        case HELP_VSCROLL:
        case HELP_LED:
        case HELP_SHADOW:
        case HELP_CONVERGENCE:
        case HELP_STRIPED:
        case HELP_SOUND:
        case HELP_SPRITE_STRESS:
        case HELP_YCDELAY:
        case HELP_DIAGONAL:
        case HELP_VERTSCROLL:
        case HELP_WHITE_NOISE:
        case HELP_PINK_NOISE:
        case HELP_CHANNEL_SEP:
        case HELP_JAGUAR_CD:
        case HELP_MEMORY_TRACK:
            totalPages = 1;
        break;

        case HELP_GENERAL:
        case HELP_STRIPES:
        case HELP_MANUALLAG:
        case HELP_HSCROLL:
        case HELP_LAG:
            totalPages = 2;
        break;
        
        case HELP_PLUGE:
        case HELP_MONOSCOPE:
            totalPages = 3;
        break;
        
    };
    
    while(!exit){

        read_joypad_state(settings->j_state);
        settings->joy1 = settings->j_state->j1;
        vsync();
        
        if(redraw){
            
            hide_or_show_display_layer_range(settings->d, 0, 15, 15);
            resetHelpLines();
            
            switch(option){


                case HELP_GENERAL:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "HELP GENERAL (1/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "The 240p Test Suite was designed with two goals in mind:^^1) Evaluate 240p signals on TV sets and video processors; and...^^2) provide calibration patterns from a game console to help in properly calibrating the display black, white and color levels.^^Help is available everywhere by pressing DOWN + OPTION.", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "DOWN + OPTION");

                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);

                            break;

                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "HELP GENERAL (2/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "The Jaguar port of the 240p Test Suite is a work in progress. Some functionality is still missing. For more information about the current state of this software, visit:^  https://jagcorner.com/240p-test-suite^^More general information about the 240p Test Suite:^  https://junkerhq.net/240p  ", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "https://jagcorner.com/240p-test-suite");
                            highlightHelpPhrase(helpTextBox, "https://junkerhq.net/240p");

                        break;
                    }

                break;
           
                
                case HELP_PLUGE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "PLUGE (1/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "NTSC-M levels set black at a 7.5 IRE setup pedestal for video. The Jaguar's HW analog floor is 6 IRE (6%), so using that value for general 240p use is not recommended.^^Of course using it as reference will work perfectly for games on this platform.^^In PAL - and console gaming in general - it is advised to use a value of 2 IRE as black.", 999999, 999999, WHITE);
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "PLUGE (2/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "The PLUGE pattern is used to help adjust the black level to a correct value.^^The inner bars on the sides are black at 6%, the outer at 12%. If these bars are not visible, adjust the \"brightness\" control until they are.^^You should lower it until they are not visible, and raise it until they show.", 999999, 999999, WHITE);
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 2:
                            updateLine(settings, mainFont, helpLineTextBox[0], "PLUGE (3/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can change to a contrast test with C.^^Within it A button changes palettes between the original highest and lowest values the hardware can display.", 999999, 999999, WHITE);
                        break;
                    }
                    
                break;
                
                case HELP_BARS:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "COLOR BARS", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern allows you to calibrate each color: Red, Green and BLUE; as well as White.^^Adjust the white level first, using the \"Contrast\" control on your TV set. Raise it until you cannot distinguish between the blocks under \"C\" and \"E\", and lower it slowly until you can clearly tell them apart.^^Do the same for each color.", 999999, 999999, WHITE);
                        break;
                    }
                    
                break;
                
                case HELP_GRID:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "GRID", 999999, 999999, GREEN);
                    
                            if(settings->PALNTSC == 0){//PAL
                            
                                updateLine(settings, mainFont, helpTextBox, "This grid uses the full 320x240 PAL resolution.^^You can use it to verify that all the visible area is being displayed, and that there is no distortion present.^^The full active video signal can be filled with gray by pressing the 'A' button.", 999999, 999999, WHITE);
                            }
                            else{//NTSC
                            
                                updateLine(settings, mainFont, helpTextBox, "This grid uses the full 320x224 resolution.^^You can use it to verify that all the visible area is being displayed, and that there is no distortion present.^^The full active video signal can be filled with gray by pressing the 'A' button.", 999999, 999999, WHITE);
                            }
                        break;
                    }
                    
                break;
                
                case HELP_MONOSCOPE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "MONOSCOPE (1/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern contains elements to calibrate multiple aspects of a CRT.^^Read your monitor's service manual to learn how, and use 'A' button to change IRE.^^Brightness Adjustment: Adjust convergence at low brightness (13/25 IRE). An overly bright pattern can mask convergence issues.", 999999, 999999, WHITE);
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "MONOSCOPE (2/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "Convergence: Use the center crosshair to check static (center of screen) convergence. Use the patterns at the sides to check dynamic (edge) convergence.^^Corners: After setting center and edge convergence, use magnets to adjust corner purity and geometry.", 999999, 999999, WHITE);
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 2:
                        
                            updateLine(settings, mainFont, helpLineTextBox[0], "MONOSCOPE (3/3)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "Size and aspect ratio: If vertical and horizontal sizes are correct, the red squares in the pattern will be perfect squares. After setting H size, use the tape measure to adjust V size to match it.^^Linearity: the squares in each corner should get you started. Confirm your adjustment using the scroll tests.^^Designed by Keith Raney", 999999, 999999, WHITE);
                        break;
                    }
                    
                break;
                
                case HELP_BLEED:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "COLOR BLEED", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern helps diagnose color bleed caused by unneeded color upsampling.^^You can toggle between vertical bars and checkerboard with 'A'.", 999999, 999999, WHITE);
                        break;
                    }
                    
                break;
                
                case HELP_IRE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "100 IRE", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can vary IRE intensity with A and B. Values are: 13, 25, 41, 53, 66, 82, 94.^^Each step is computed as g = round((IRE/100) * 64) on the Jaguar's 6-bit green channel (0..63); red and blue (5-bit, 0..31) are driven at g >> 1 to stay neutral.^^NTSC-J / PAL / PC RGB use 0 IRE as black; NTSC-M (US) uses a 7.5 IRE setup pedestal -- check your display's setup-level menu. The Jaguar DAC clamps below ~6 IRE, which is why this ladder starts at 13.", 999999, 999999, WHITE);        
                        break;
                    }
                    
                break;
                
                case HELP_601CB:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "601 COLORBARS", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can use color filters or the blue only option in your display to confirm color balance.", 999999, 999999, WHITE);   
                        break;
                    }
                    
                break;
                
                case HELP_SHARPNESS:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SHARPNESS", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You should set the sharpness of your CRT to a value that shows clean black and grey transitions with no white ghosting between.", 999999, 999999, WHITE);           
                        break;
                    }
                    
                break;
                
                case HELP_OVERSCAN:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "OVERSCAN", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "With this pattern you can interactively find out the overscan in pixels of each edge in a display.^^Use the d-pad to move the overscan box for the selected edge until you see white, then go back one pixel. The resulting number is the amount of overscan in pixels in each direction.^^Use 'A' and 'B' to change selected edge.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_SMPTE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SMPTE COLOR BARS", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern can be used to approximate for NTSC levels regarding contrast, brightness and colors.^^You can toggle between 75% and 100% SMPTE color bars with A. Of course the percentages are relative to the console output.^^You can use color filters or the blue only option in your display to confirm color balance.^^This HW lowest black is ??????.", 999999, 999999, WHITE);    
                        break;
                    }
                    
                break;
                
                case HELP_GRAY:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "GRAY RAMP", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This gray ramp pattern can be used to check color balance.^^You should make sure the bars are gray, with no color bias.", 999999, 999999, WHITE); 
                        break;
                    }
                    
                break;
                
                case HELP_WHITE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "WHITE SCREEN", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern can be changed between white, black, red, green and blue screens with the 'A' and 'B' buttons.^^A custom color mode is available by pressing 'C' when on the white screen. Use left and right to select a color channel, and up and down to adjust the color channel.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_CHECK:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "CHECKERBOARD", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This pattern shows all the visible pixels in an alternating white and black grid array.^^You can toggle the pattern with button 'Up', or turn on auto-toggle each frame with the 'A' button. A frame counter is also available with 'B'.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_SHADOW:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "DROP SHADOW TEST", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This is a crucial test for 240p. It displays a simple sprite shadow against a background, but the shadow is shown only on each other frame achieving a transparency effect.^^The user can toggle the frame used to draw the shadow with button 'A'. Backgrounds can be switched with the 'B' button and button 'C' toggles sprites.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_STRIPED:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "STRIPED SPRITE TEST", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "There are deinterlacers out there that can display the drop shadows correctly and still interpret 240p as 480i. With a striped sprite it should be easy to tell if a processor tries to deinterlace (plus interpolate).^^You can change backgrounds with 'A'.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_STRIPES:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "HOR/VER STRIPES (1/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You should see a pattern of lines, each one pixel in height starting with a white one.^^You can toggle the pattern with 'Up', or turn on auto-toggle each frame with the 'A' button. A frame counter is also available with 'B'.^^When auto-toggle is set, you should see the lines alternating rapidly.", 999999, 999999, WHITE);  
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "HOR/VER STRIPES (2/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can also display vertical bars by pressing 'LEFT'. That pattern will help you evaluate if the signal is not distorted horizontally, since all lines should be one pixel wide.", 999999, 999999, WHITE);   
                        break;
                    }
                    
                break;
                
                case HELP_MANUALLAG:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "TIMING & REFLEX (1/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "The main intention is to show a changing pattern on the screen, which can be complemented with audio. This should show to some degree any lag when processing the signal.^^As an added feature, the user can click the 'A' button when the sprite is aligned with the one on the background, and the offset in frames from the actual intersection will be shown on screen. A 1 kHz tone will be played for 1 frame when pressed.", 999999, 999999, WHITE);   
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "TIMING & REFLEX (2/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "Button 'B' can be used to change the direction of the sprite.^^Evaluation is dependent on reflexes and/or rhythm more than anything. The visual and audio cues are the more revealing aspects which the user should consider, but the interactive factor can give an experienced player the hang of the system when testing via different connections.", 999999, 999999, WHITE);   
                        break;
                    }
                    
                break;
                
                case HELP_HSCROLL:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SCROLL TEST (1/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This test shows either a horizontal 320x224 background from sonic or a vertical 256x224 background from Kiki Kaikai.^^Speed can be varied with Up & Down and scroll direction with Left. The 'A' button stops the scroll and 'B' toggles between vertical and horizontal.", 999999, 999999, WHITE);                               
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SCROLL TEST (2/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This can be used to notice any drops in framerate, or pixel width inconsistencies.^^Sonic is a trademark of Sega Enterprises Ltd. Kiki Kaikai is a trademark of Taito.", 999999, 999999, WHITE); 
                        break;
                    }
                    
                break;
                
                case HELP_VSCROLL:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "GRID SCROLL TEST", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "A grid is scrolled vertically or horizontally, which can be used to test linearity of the signal and how well the display or video processor copes with scrolling and framerate.^^'B' button can be used to toggle between horizontal and vertical, while Up/Down regulates speed.^^'A' button stops the scroll and 'Left' changes direction.", 999999, 999999, WHITE); 
                        break;
                    }
                    
                break;
                
                case HELP_LED:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "BACKLIGHT TEST", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This test allows you to check how the display's backlight works when only a small array of pixels is shown.^^The user can move around the white pixel arrays with the d-pad, and change the size of the pixel array with 'A'. The 'B' button allows the user to hide the pixel array in order to alternate a fully black screen.", 999999, 999999, WHITE);  
                        break;
                    }
                    
                break;
                
                case HELP_LAG:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "LAG TEST (1/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "This test is designed to be used with two displays connected at the same time. One being a CRT, or a display with a known lag as reference, the other display to test.^^Using a camera, a picture should be taken of both screens at the same time. The picture will show the frame discrepancy between them.^^The circles in the bottom help determine the frame even when the numbers are blurry.", 999999, 999999, WHITE);                              
                            
                            updateLine(settings, mainFont, helpLineTextBox[1], "Continued...", 999999, 999999, WHITE);
                        break;
                        
                        case 1:
                            updateLine(settings, mainFont, helpLineTextBox[0], "LAG TEST (2/2)", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can split the video signal and feed both displays.^^The vertical bars on the sides change color each frame to help when using LCD photos.^^Press A to start/stop, B to reset and C for Black & White test.", 999999, 999999, WHITE);        
                        break;
                    }
                    
                break;
                
                case HELP_CONVERGENCE:
                    
                    switch(page){
                        
                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "CONVERGENCE TESTS", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "These are used to adjust color convergence in CRT displays.^^A and B buttons change the cross hatch pattern between lines, dots and crosses. Then to a color pattern for transition boundary check with and without a black border.", 999999, 999999, WHITE);   
                        break;
                    }
                    
                break;

                case HELP_SOUND:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SOUND TEST", 999999, 999999, GREEN);
                            
                            updateLine(settings, mainFont, helpTextBox, "You can test the waveforms that are stored in Jerry from here. Use left/right to select the waveform and octave to playback, or to adjust panning. Press A to start/stop playback.^^An additional raw audio sample has been included as an optional test.", 999999, 999999, WHITE); 
                        break;
                    }

                break;

                case HELP_SPRITE_STRESS:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "SPRITE STRESS (OP SATURATION)", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Stresses the Object Processor by spawning many DEPTH8 sprites on a single scanline. The OP walks its display list once per line until horizontal time runs out -- once your sprites exceed the ~8 KiB/line pixel-fetch budget, the OP drops or tears the trailing entries.^^L/R: -/+1 sprite. U/D: +/-8 (fast ramp). A: cycle 8x8/16x16/32x32. B: SINGLE-LINE vs TILED. C: reset to 1. OPTION: exit.^^Compare real-hardware drop-out against Virtual Jaguar / BigPEmu.", 999999, 999999, WHITE);
                        break;
                    }

                break;

                case HELP_YCDELAY:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "Y/C DELAY", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Vertical strips of red, green, blue, yellow, cyan and magenta separated by 1px white dividers.^^On a clean RGB path the dividers stay white. On composite/S-Video, chroma trails luma so the divider picks up coloured fringing on either side -- that is the visible Y/C delay.^^Useful to confirm a SCART/component cable is really running RGB and not falling back to composite.", 999999, 999999, WHITE);
                        break;
                    }

                break;

                case HELP_DIAGONAL:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "DIAGONAL / CLOCK", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Parallel 1-pixel diagonal lines at 45 degrees.^^On a CRT or a clean digital path each line is a clean stair-step. Upscalers (HDMI, OSSC, RetroTink) reveal stair-stepping or blur artefacts here.^^UP/DOWN: change line spacing (4/8/16/32 px). A: invert (white-on-black <-> black-on-white). OPTION: exit.", 999999, 999999, WHITE);
                        break;
                    }

                break;

                case HELP_VERTSCROLL:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "VERTICAL SCROLL", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "A full-screen banded pattern scrolls top-to-bottom (or bottom-to-top) at a user-selectable speed. Mirror of the horizontal Scroll Test, exercising vertical OP/blitter pacing.^^UP/DOWN: speed. LEFT/RIGHT: reverse direction. A: pause. OPTION: exit.^^Vertical jitter, tearing or pacing hitches that don't show up in horizontal scroll usually surface here.", 999999, 999999, WHITE);
                        break;
                    }

                break;

                case HELP_WHITE_NOISE:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "WHITE NOISE", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Plays full-bandwidth white noise (equal energy per Hz) generated by a 16-bit Galois LFSR. Useful for stress-testing the full audio chain - cables, amplifier headroom and speaker drivers should all sound smooth, with no buzz or rattle.^^A: play / pause^B: cycle channel (BOTH / LEFT / RIGHT) to verify L/R wiring.", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "white noise");
                        break;
                    }

                break;

                case HELP_PINK_NOISE:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "PINK NOISE", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Pink noise has equal energy per octave (1/f spectrum) - the canonical reference signal for measuring frequency response. A flat trace on a real-time analyser indicates a flat speaker / room / cable chain.^^Generated by Paul Kellet's economy 5-stage IIR filter applied to the LFSR white noise.^^A: play / pause   B: cycle channel (BOTH / LEFT / RIGHT).", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "Pink noise");
                        break;
                    }

                break;

                case HELP_CHANNEL_SEP:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "CHANNEL SEPARATION", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Plays a 1 kHz sine tone in selected channels to verify left and right outputs are isolated and correctly wired (this is distinct from L/R Balance, which is about volume trim).^^UP   : LEFT only^DOWN : RIGHT only^LEFT : BOTH (in phase)^RIGHT: BOTH (180-degree out of phase)^A    : play / pause", 999999, 999999, WHITE);
                        break;
                    }

                break;

                case HELP_JAGUAR_CD:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "JAGUAR CD PROBE", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Probes 16-bit words in the Butch (CD) ASIC ($F14000-$F1400A) and the CD boot ROM window ($800000-$800002). On a base Jaguar these read 0x0000 or 0xFFFF (open bus).^If any word returns a different value the heuristic reports DETECTED. The BIOS line shows FOUND when the CD boot ROM window contains non-open-bus data.^Values refresh every frame for real-time monitoring.^^DOWN+OPTION: this help.   OPTION: exit", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "DOWN+OPTION");
                        break;
                    }

                break;

                case HELP_MEMORY_TRACK:

                    switch(page){

                        case 0:
                            updateLine(settings, mainFont, helpLineTextBox[0], "MEMORY TRACK TEST", 999999, 999999, GREEN);

                            updateLine(settings, mainFont, helpTextBox, "Exercises the 93C46 serial EEPROM on the bus. On a Jaguar CD with Memory Track this is the CD unit's 128-byte save storage. Without a CD this tests the cart's own EEPROM.^The CD line shows whether CD hardware was detected. Both EEPROMs use the same Jerry bus ($F14001/$F14801/$F15001).^^A: Walking-1s   X: Address-as-data^B: Re-read   Y: Erase cell^OPTION: restore originals and exit^DOWN+OPTION: this help", 999999, 999999, WHITE);
                            highlightHelpPhrase(helpTextBox, "DOWN+OPTION");
                        break;
                    }

                break;

            }
            
            hide_or_show_display_layer_range(settings->d, 1, 14, 15);
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
    
    hide_or_show_display_layer_range(settings->d, 0, 14, 15);
    
    for(i = 0 ; i != HELPTOTALLINES; i++){
        helpLineTextBox[i] = freeTextBox(helpLineTextBox[i]);
    }
    
    freeTextBox(helpTextBox);
    
    helpSprite->invisible = 1;
    detach_sprite_from_display(helpSprite);
    free(helpSprite);
    free(helpData);
    
};

void resetHelpLines(){
    int i = 0;
    for(i = 0 ; i != HELPTOTALLINES-1; i++){
        updateLine(settings, mainFont, helpLineTextBox[i], "        ", 999999, 999999, WHITE);
    }
}
