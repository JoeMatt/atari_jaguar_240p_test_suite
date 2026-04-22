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
void DrawYCDelay(void);
void DrawDiagonal(void);

/* Video tests */
void ManualLagTest(void);
void Alternate240p480iTest(void);
void VertScrollTest(void);

/* Audio tests */
void AudioBalanceTest(void);
void MDFourierTest(void);
/* Broadband + reference noise generators and L/R isolation, mirrors the
 * canonical Audio menu items in the Genesis / GameCube / Dreamcast 240p
 * Test Suites. White noise is for full-bandwidth driver / cable stress;
 * pink noise (equal energy per octave) is the standard frequency-response
 * test signal; channel separation isolates L vs R to catch crossed
 * wiring or a mono'd output. */
void WhiteNoiseTest(void);
void PinkNoiseTest(void);
void ChannelSeparationTest(void);

/* Hardware tests */
void HardwareInfo(void);
void JaguarCDTest(void);
void ResolutionTest(void);
void ProControllerTest(void);
void RotaryControllerTest(void);

/* Screen savers (animated full-screen patterns for OLED burn-in / demo)
 * Mirrors the Screensavers section that ships with the canonical 240p
 * test suite (Genesis / SNES / Dreamcast). All three are procedural, no
 * extra LZ77 assets required. */
void ColorCycleSaver(void);
void BouncingSquareSaver(void);
void ScrollingBarsSaver(void);

/* Options menu (replaces the old (x)Options stub everywhere it appeared) */
void OptionsMenu(void);

#endif
