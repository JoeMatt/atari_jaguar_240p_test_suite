#ifndef EXTRA_TESTS
#define EXTRA_TESTS

#include <jagdefs.h>
#include <jagtypes.h>
#include <stdlib.h>
#include <interrupt.h>
#include <display.h>
#include <sprite.h>
#include <joypad.h>
#include <screen.h>
#include <blit.h>
#include <fb2d.h>
#include <lz77.h>
#include <sound.h>

#include "common_assets.h"
#include "help.h"

/* ---------------------------------------------------------------------------
 * Extra 240p Test Suite tests, ported in spirit from the canonical Artemio
 * Urbina suite (Genesis / Mega Drive, SNES, Dreamcast, Wii, Neo Geo, etc).
 *
 * Everything in here is generated procedurally on the Jaguar at runtime so
 * we don't have to ship more LZ77-packed PNG assets. The Jaguar's blitter +
 * `screen` primitives let us draw color bars, gradients, and stripe
 * patterns directly into a framebuffer that becomes a sprite layer, so the
 * end result is visually identical to the upstream PNG-baked patterns
 * without growing the cart image past its current 1 MiB footprint.
 * --------------------------------------------------------------------------- */

/* Test patterns */
void DrawColorBarsGray(void);
void DrawLinearity(void);
void DrawPhase(void);
void DrawBrightness(void);
void DrawContrast(void);

/* Video tests */
void ManualLagTest(void);
void Alternate240p480iTest(void);

/* Audio tests */
void AudioBalanceTest(void);

/* Hardware tests */
void HardwareInfo(void);

/* Options menu (replaces the old (x)Options stub everywhere it appeared) */
void OptionsMenu(void);

#endif
