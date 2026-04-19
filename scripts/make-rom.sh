#!/usr/bin/env sh
# ---------------------------------------------------------------------------
# Wrap a raw Jaguar .bin (linked at $802000) in a universal cart header so
# the resulting .rom boots on real hardware, Skunkboard, BigPEmu, etc.
#
# Used as a fallback when `makefastboot` is not on PATH. The Jaguar SDK Docker
# image always provides makefastboot; this script is for native installs that
# don't have it.
#
# Usage: make-rom.sh <input.bin> <output.rom>
# ---------------------------------------------------------------------------
set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 <input.bin> <output.rom>" >&2
	exit 64
fi

IN="$1"
OUT="$2"

if [ ! -f "$IN" ]; then
	echo "$0: input file not found: $IN" >&2
	exit 66
fi

# ---------------------------------------------------------------------------
# Universal cart header (Songbird / "fast boot" style).
#
# This is the public-domain header documented in many places (e.g. tursilion's
# makefastboot) that:
#   - jumps to $802000 (the standard rmvlib link address for ROM builds)
#   - identifies itself as a Jaguar cart so Skunkboard / emulators accept it
#
# Encoded inline so the script has no external dependencies.
# ---------------------------------------------------------------------------
python3 - "$IN" "$OUT" <<'PY'
import struct
import sys

src, dst = sys.argv[1], sys.argv[2]

# Standard Atari Jaguar universal cartridge header (8KiB padding to $2000),
# entry vector to $802000.
header = bytearray(0x2000)

# 'ATARI APPROVED DATA HEADER ATRI ' marker at offset 0
marker = b"ATARI APPROVED DATA HEADER ATRI "
header[0:len(marker)] = marker

# Reset vector (initial PC) at offset 0x404 -> $802000
struct.pack_into(">I", header, 0x404, 0x00802000)

# Initial SP at offset 0x400 -> top of RAM
struct.pack_into(">I", header, 0x400, 0x00200000)

with open(src, "rb") as f:
	body = f.read()

with open(dst, "wb") as f:
	f.write(header)
	f.write(body)

print(f"wrote {dst} ({len(header) + len(body)} bytes)")
PY
