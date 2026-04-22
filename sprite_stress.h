#ifndef SPRITE_STRESS
#define SPRITE_STRESS

#include <jagdefs.h>
#include <jagtypes.h>
#include <stdlib.h>
#include <string.h>
#include <interrupt.h>
#include <display.h>
#include <sprite.h>
#include <joypad.h>
#include <screen.h>
#include <blit.h>
#include <fb2d.h>

#include "common_assets.h"
#include "help.h"

/* ---------------------------------------------------------------------------
 * Sprite Stress Test (Jaguar Object Processor saturation)
 *
 * The Jaguar has no traditional sprite-per-line hardware limit -- the
 * Object Processor (OP) executes a programmable display list of
 * "objects" once per scanline, walking the list from start until the
 * line's horizontal time runs out. The practical ceiling is therefore
 * a *bandwidth* limit, not a count limit: ~8-9 KiB of pixel-data
 * fetch per active scanline at PWIDTH4, plus per-object header
 * overhead. Once a scanline blows past that budget the OP simply
 * stops processing the remainder of the list for that line, and any
 * "below-the-fold" sprites visibly drop out (or tear) until the next
 * scanline -- where the OP starts over from the top of the list.
 *
 * This test exists to characterise that limit on:
 *
 *   - Real hardware (target reference)
 *   - Virtual Jaguar / BigPEmu / MAME (validate emulator OP timing)
 *   - CRT vs scaler chains (saturation-induced tearing reads
 *     differently through a deinterlacer or upscaler)
 *
 * Controls:
 *   D-pad LEFT/RIGHT  -- spriteCount -/+ 1
 *   D-pad DOWN/UP     -- spriteCount -/+ 8 (fast ramp)
 *   A                 -- cycle sprite size (8x8 -> 16x16 -> 32x32)
 *   B                 -- toggle SINGLE-LINE (worst-case OP saturation,
 *                        all sprites stacked on one Y) vs TILED
 *                        (sprites spread vertically across ~128 px)
 *   C                 -- reset spriteCount to 1
 *   OPTION            -- exit back to the Hardware menu
 *
 * Sprites are DEPTH8 with phrase-aligned widths (8/16/32 px), all
 * sharing one statically-allocated 32x32 byte buffer filled with a
 * known CLUT index. The test saves & restores TOMREGS->clut1 entry
 * STRESS_CLUT_IDX on entry/exit so the menu's palette is left
 * exactly as we found it.
 *
 * No equivalent test exists in the Genesis/Dreamcast/SNES 240p
 * suites -- the OP is unique to the Jaguar.
 * --------------------------------------------------------------------------- */

void SpriteStressTest(void);

#endif
