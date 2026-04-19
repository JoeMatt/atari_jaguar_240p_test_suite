#!/usr/bin/env python3
"""Wrap a raw Jaguar .bin (linked at $802000) in a fastboot cart header.

This is a pure-Python port of Tursilion's makefastboot.cpp
(https://github.com/tursilion/makefastboot), which is the de-facto standard
header used by homebrew Jaguar ROMs (BigPEmu, Virtual Jaguar, Skunkboard,
real hardware via flash carts all accept it).

Compared to the original C++ tool this version:
  - takes input/output filenames as CLI args (no interactive prompts)
  - hard-codes 32-bit cart width @ 10 clocks (the safe defaults)
  - has no Windows / MFC dependencies, runs anywhere Python 3 does

Usage: make-rom.py <input.bin> <output.rom>

The input must be a raw Jaguar binary linked at the standard ROM address
($802000) -- exactly what `aln`'s -a 802000 flag produces.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Encrypted fastboot signature, 132 bytes, lifted verbatim from
# makefastboot.cpp (the "newer version that checksums the cart" branch).
# These bytes are the RSA-encrypted boot stub that the Jaguar BIOS verifies
# before transferring control to the cart at $802000.
#
# IMPORTANT: every byte must match the upstream signature exactly -- a single
# bit flip is enough for the BIOS to reject the cart and halt at the boot
# splash. (Byte 0x69 was previously typo'd 0x52 instead of 0x76 in this port,
# which matters on real hardware and on BIOS-strict emulators like BigPEmu.
# Most libretro / standalone emulators bypass BIOS verification entirely, so
# they happily run a signature-broken cart -- don't use those for validation.)
# Verified against tursilion/makefastboot @ master with scripts/verify-sig.py.
# ---------------------------------------------------------------------------
FASTBOOT_SIGNATURE = bytes.fromhex(
    "FE82D98755E1DE2A08A664B92D9D29EF"
    "C93AE8ECAEAE730F758E96D35C90820C"
    "E4929919CADDD5E022D381004CEBC4A9"
    "25CDFC21DB91C394148D61B1CC5D4985"
    "701210BC9A30530056AC8086B11CD33F"
    "2920D166CA53EA9A8AD8ECEB82BC3489"
    "F4C19C77929543197F765246392E74AC"
    "BAC73CC45A296DE62F59CB4DFEA749A3"
    "530C2E1C"
)
assert len(FASTBOOT_SIGNATURE) == 0x84, len(FASTBOOT_SIGNATURE)

HEADER_SIZE = 0x2000           # cart header is 8 KiB
ROM_BASE = 0x00802000          # standard rmvlib link address
JAG_MAGIC = 0x03D0DEAD         # checksum salt the BIOS expects
MIB = 1024 * 1024

# Cart width / speed byte for offsets 0x400-0x403:
#   width: 0=8-bit, 2=16-bit, 4=32-bit
#   speed bits: 0/8/16/24 -> 10/8/6/5 clocks
# 32-bit @ 10 clocks (slowest, safest) matches makefastboot's defaults.
CART_WIDTH_SPEED = 0x04


def build_rom(in_path: Path, out_path: Path) -> None:
    payload = in_path.read_bytes()
    if not payload:
        raise SystemExit(f"{in_path}: empty input")

    # Virtual Jaguar/libretro's file-type detection only recognises cart images
    # when total file size is a multiple of 1 MiB (JST_ROM) or Alpine images
    # when (size + 8 KiB) is a multiple of 1 MiB (JST_ALPINE). A non-padded ROM
    # can therefore "load" but never actually execute. We emit a cart image and
    # pad to the next MiB boundary with 0xFF (erased flash state), which is
    # harmless on real hardware and keeps emulator detection reliable.
    unpadded_size = HEADER_SIZE + len(payload)
    padded_size = ((unpadded_size + MIB - 1) // MIB) * MIB
    pad_len = padded_size - unpadded_size
    body = payload + (b"\xFF" * pad_len)

    header = bytearray(HEADER_SIZE)

    header[0x00:0x84] = FASTBOOT_SIGNATURE

    header[0x400] = CART_WIDTH_SPEED
    header[0x401] = CART_WIDTH_SPEED
    header[0x402] = CART_WIDTH_SPEED
    header[0x403] = CART_WIDTH_SPEED

    struct.pack_into(">I", header, 0x404, ROM_BASE)
    struct.pack_into(">I", header, 0x408, 0x00000000)

    # ---- Checksum the body ------------------------------------------------
    # makefastboot.cpp computes:
    #   end = (sz + 0x800000) & 0xfffffc        # sz includes the 8KB header
    #   cnt = (end - 0x802000) / 4              # words to checksum
    # In our case `body` is the post-header payload (including any 0xFF
    # cart-size padding), so the equivalent count is simply len(body) rounded
    # down to a 4-byte boundary.
    #
    # The original payload is preserved verbatim in the output ROM (with only
    # optional 0xFF padding appended for cart-size compatibility); checksum
    # range is rounded down to avoid silently dropping trailing 1-3 bytes.
    word_count = len(body) // 4
    if word_count == 0:
        raise SystemExit(f"{in_path}: input is too small to checksum")

    aligned_len = word_count * 4
    checksum = JAG_MAGIC
    for offset in range(0, aligned_len, 4):
        checksum = (checksum + struct.unpack_from(">I", body, offset)[0]) & 0xFFFFFFFF

    if len(body) != aligned_len:
        print(
            f"note: input is {len(body)} bytes, not 32-bit aligned; "
            f"checksum covers the first {aligned_len} bytes, "
            f"trailing {len(body) - aligned_len} byte(s) preserved verbatim",
            file=sys.stderr,
        )

    struct.pack_into(">I", header, 0x410, ROM_BASE)
    struct.pack_into(">I", header, 0x414, word_count)
    struct.pack_into(">I", header, 0x418, checksum)

    out_path.write_bytes(bytes(header) + body)
    size = HEADER_SIZE + len(body)
    print(
        f"wrote {out_path} ({size} bytes, {word_count} words checksummed, "
        f"checksum 0x{checksum:08X}, padded +{pad_len} byte(s))"
    )


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {argv[0]} <input.bin> <output.rom>", file=sys.stderr)
        return 64
    in_path = Path(argv[1])
    out_path = Path(argv[2])
    if not in_path.is_file():
        print(f"{argv[0]}: input file not found: {in_path}", file=sys.stderr)
        return 66
    build_rom(in_path, out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
