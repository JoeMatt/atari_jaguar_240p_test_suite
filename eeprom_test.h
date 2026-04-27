#ifndef EEPROM_TEST
#define EEPROM_TEST

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
#include <sound.h>

#include "common_assets.h"

/* ---------------------------------------------------------------------------
 * EEPROM Read & Write Test (NM93C46 / 93Cxx, 64 x 16-bit)
 *
 * Closes BitJag upstream issue #5. The Jaguar's EEPROM is the only
 * persistent storage available to a cart -- 128 bytes, organised as
 * 64 16-bit words -- and it sits behind a 3-wire serial protocol
 * exposed through three Jerry I/O ports:
 *
 *     $F14001 R/W  bit 0 = DO  (data out, EEPROM -> cart)
 *     $F14801 W    bit 0 = DI  (data in, cart -> EEPROM; write also pulses clock)
 *     $F15001 R/W  any access  = strobe CS  (resets state machine, starts new op)
 *
 * The driver here is intentionally minimal: enough to issue READ /
 * WRITE / EWEN opcodes for the menu test, no more. Comments inline
 * call out which assumptions are Virtual-Jaguar-specific so any
 * future port to real hardware (or BigPEmu / MAME) knows what to
 * adjust. See ``eeprom_test.c`` for the actual protocol implementation.
 *
 * The test UI saves the original contents on entry and restores them
 * on exit, so running it never costs the user their real save data.
 * --------------------------------------------------------------------------- */

/* 93C46 organisation: 64 words of 16 bits each. */
#define EE_NWORDS 64

/* Low-level 93C46 command wrappers shared by the cart EEPROM test and
 * the Memory Track test (same Jerry bus, different physical chip). */
uint16_t eeprom_read_word_drv(uint8_t addr);
int eeprom_write_word_drv(uint8_t addr, uint16_t data);
void eeprom_ewen_drv(void);
void eeprom_ewds_drv(void);

void EepromTest(void);

#endif
