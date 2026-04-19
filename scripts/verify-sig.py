#!/usr/bin/env python3
"""Regression test: verify scripts/make-rom.py's FASTBOOT_SIGNATURE matches
tursilion/makefastboot upstream byte-for-byte.

A single-byte typo here is invisible to most libretro / standalone emulators
(they bypass BIOS verification) but bricks the cart on real Jaguar hardware
and on BIOS-strict emulators like BigPEmu. Run this from CI so the wrong
signature can never silently re-land.

Usage:
  scripts/verify-sig.py [path/to/local/makefastboot.cpp]

If no path is supplied, the script clones tursilion/makefastboot into a temp
directory (cached across runs) and reads makefastboot/makefastboot.cpp from
there. Exits 0 on match, 1 on mismatch, 2 on infrastructure failure.
"""

from __future__ import annotations

import importlib.util
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

UPSTREAM = "https://github.com/tursilion/makefastboot.git"
CACHE_DIR = Path(tempfile.gettempdir()) / "makefastboot-verify-cache"
SIG_LEN = 132


def _fetch_reference(local: Path | None) -> Path:
    if local is not None:
        if not local.is_file():
            raise SystemExit(f"reference not found: {local}")
        return local

    cpp = CACHE_DIR / "makefastboot" / "makefastboot.cpp"
    if cpp.is_file():
        return cpp

    CACHE_DIR.parent.mkdir(parents=True, exist_ok=True)
    rc = subprocess.call(
        ["git", "clone", "--depth", "1", "--quiet", UPSTREAM, str(CACHE_DIR)]
    )
    if rc != 0 or not cpp.is_file():
        raise SystemExit(f"failed to clone {UPSTREAM} into {CACHE_DIR}")
    return cpp


def _extract_sig(cpp_path: Path) -> bytes:
    text = cpp_path.read_text()
    m = re.search(r"#if 1\b(.*?)#elif 0\b", text, re.S)
    if not m:
        raise SystemExit(f"could not find #if 1 / #elif 0 block in {cpp_path}")
    chunk = re.sub(r"//[^\n]*", "", m.group(1))
    raw = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", chunk))
    start = raw.find(bytes([0xFE, 0x82, 0xD9, 0x87]))
    if start < 0:
        raise SystemExit("could not locate signature magic 0xFE82D987 in source")
    return raw[start : start + SIG_LEN]


def _load_local_sig() -> bytes:
    here = Path(__file__).resolve().parent
    spec = importlib.util.spec_from_file_location("make_rom", here / "make-rom.py")
    if spec is None or spec.loader is None:
        raise SystemExit("could not import scripts/make-rom.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.FASTBOOT_SIGNATURE


def main(argv: list[str]) -> int:
    local_path = Path(argv[1]) if len(argv) > 1 else None
    try:
        ref_cpp = _fetch_reference(local_path)
        ref_sig = _extract_sig(ref_cpp)
        our_sig = _load_local_sig()
    except SystemExit as exc:
        print(f"!! {exc}", file=sys.stderr)
        return 2

    if len(ref_sig) != SIG_LEN or len(our_sig) != SIG_LEN:
        print(
            f"!! length mismatch: reference={len(ref_sig)}, local={len(our_sig)}",
            file=sys.stderr,
        )
        return 1

    diffs = [
        (i, ref_sig[i], our_sig[i]) for i in range(SIG_LEN) if ref_sig[i] != our_sig[i]
    ]
    if diffs:
        print("!! FASTBOOT_SIGNATURE diverges from upstream:", file=sys.stderr)
        for off, ref, ours in diffs:
            print(
                f"   byte 0x{off:02X}: reference=0x{ref:02X}  ours=0x{ours:02X}",
                file=sys.stderr,
            )
        return 1

    print(f"OK: scripts/make-rom.py FASTBOOT_SIGNATURE matches {ref_cpp} ({SIG_LEN} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
