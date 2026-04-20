#!/usr/bin/env python3
"""Smoke-test a Jaguar libretro core against a built ROM.

Runs the libretro init / load_game pipeline against
virtualjaguar_libretro.dylib (or any other Jaguar libretro core) and
reports whether the core accepted the file. Useful when standalone
emulators reject the build with an opaque error -- libretro.py surfaces
the actual rejection (extension filter, BIOS missing, content too big,
checksum mismatch, etc.) as a clear Python exception.

Usage:
  scripts/libretro-load-test.py CORE.dylib CONTENT.j64
  scripts/libretro-load-test.py --frames 30 CORE.dylib CONTENT.j64

Why we don't use `python -m libretro.py.test.loads_content` directly:
  the upstream test scripts use `typer`, which has an annotation-parsing
  bug under Python 3.14 (`tuple[Path, ...]` blows up in Click). This
  script bypasses typer entirely so it runs on whatever CPython >= 3.12
  the user already has.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    p.add_argument("core", type=Path, help="path to the libretro core .dylib/.so/.dll")
    p.add_argument("content", type=Path, help="path to the ROM (.j64, .jag, etc.)")
    p.add_argument(
        "--frames",
        type=int,
        default=0,
        help="run this many frames after load (default: 0 = load only)",
    )
    p.add_argument(
        "--dump-frames",
        type=Path,
        metavar="DIR",
        help=(
            "save every rendered frame as DIR/frame-NNNN.png "
            "(requires --frames > 0; auto-installs Pillow into the venv)"
        ),
    )
    p.add_argument(
        "--dump-every",
        type=int,
        default=1,
        help="when --dump-frames is set, only save every Nth frame (default: 1)",
    )
    p.add_argument(
        "--summary",
        action="store_true",
        help="print one line per frame describing pixel-format / size / non-black ratio",
    )
    args = p.parse_args(argv[1:])

    if not args.core.is_file():
        print(f"core not found: {args.core}", file=sys.stderr)
        return 66
    if not args.content.is_file():
        print(f"content not found: {args.content}", file=sys.stderr)
        return 66

    try:
        from libretro import SessionBuilder
    except ImportError:
        print(
            "libretro.py is not installed. From the repo root:\n"
            "  python3 -m venv .venv-libretro\n"
            "  .venv-libretro/bin/pip install 'libretro.py[cli]'\n"
            "  .venv-libretro/bin/python scripts/libretro-load-test.py ...",
            file=sys.stderr,
        )
        return 1

    builder = SessionBuilder.defaults(str(args.core)).with_content(str(args.content))

    print(f">> core:    {args.core}", flush=True)
    print(f">> content: {args.content} ({args.content.stat().st_size:,} bytes)", flush=True)

    save_png = None
    if args.dump_frames is not None:
        if args.frames <= 0:
            print("--dump-frames requires --frames > 0", file=sys.stderr)
            return 64
        args.dump_frames.mkdir(parents=True, exist_ok=True)
        save_png = _resolve_png_writer()
        if save_png is None:
            return 1

    # Note on flushing: every print uses flush=True. The Virtual Jaguar
    # libretro core on macOS calls raw _exit(0) from inside the first
    # retro_run() invocation (upstream bug -- reproduces with any
    # non-RetroArch harness), which bypasses Python's stdio buffering and
    # atexit hooks. Without flush=True the user would see zero output and
    # think the script silently did nothing. Even with flush=True, anything
    # printed AFTER session.run() may not appear if the core takes that
    # exit path.
    if args.frames > 0:
        print(
            ">> NOTE: requesting >0 frames against virtualjaguar on macOS will\n"
            "         likely cause the core to call _exit(0) from inside\n"
            "         retro_run() -- this is an upstream core bug, not a\n"
            "         harness issue. The ROM is valid if 'load_game returned\n"
            "         True' prints above.",
            flush=True,
        )

    try:
        with builder.build() as session:
            print(">> load_game returned True; core initialised successfully.", flush=True)
            for i in range(args.frames):
                session.run()
                if not (args.summary or save_png):
                    continue
                shot = session.video.screenshot()
                if shot is None:
                    if args.summary:
                        print(f"  frame {i:04d}: <no framebuffer yet>", flush=True)
                    continue
                if args.summary:
                    nb = _non_black_ratio(shot)
                    print(
                        f"  frame {i:04d}: {shot.width}x{shot.height} "
                        f"{shot.pixel_format.name} non_black={nb:.1%}",
                        flush=True,
                    )
                if save_png and (i % args.dump_every == 0):
                    out = args.dump_frames / f"frame-{i:04d}.png"
                    save_png(shot, out)
            if args.frames:
                print(f">> ran {args.frames} frame(s) without raising.", flush=True)
                if save_png:
                    print(f">> dumped frames to {args.dump_frames}/", flush=True)
            sys.stdout.flush()
            sys.stderr.flush()
    except Exception as exc:
        print(f"!! load failed: {type(exc).__name__}: {exc}", file=sys.stderr, flush=True)
        return 2

    return 0


def _non_black_ratio(shot) -> float:
    """Fraction of pixels whose RGB isn't (0,0,0). The screenshot buffer is
    a tightly packed RGBA8888 stream (4 bytes per pixel: R,G,B,A) regardless
    of the core's source pixel format -- ArrayVideoDriver.screenshot() does
    the conversion. Byte 3 (alpha) is ignored; we only care about visible
    luminance to detect a fully-blank frame."""
    buf = bytes(shot.data)
    total = shot.width * shot.height
    if total == 0:
        return 0.0
    nz = 0
    for off in range(0, total * 4, 4):
        if buf[off] | buf[off + 1] | buf[off + 2]:
            nz += 1
    return nz / total


def _resolve_png_writer():
    """Return a callable(shot, path) -> None that writes a PNG, importing
    Pillow lazily and auto-installing it into the active venv if missing."""
    try:
        from PIL import Image  # noqa: WPS433
    except ImportError:
        import subprocess
        print(">> Pillow not installed; pip-installing into the active interpreter...")
        rc = subprocess.call(
            [sys.executable, "-m", "pip", "install", "--quiet", "Pillow"]
        )
        if rc != 0:
            print("!! pip install Pillow failed", file=sys.stderr)
            return None
        from PIL import Image  # noqa: WPS433

    def _save(shot, path: Path) -> None:
        ## ArrayVideoDriver.screenshot() unpacks the source pixel format into
        ## a tightly-packed RGBA8888 buffer (despite a stale ABGR comment in
        ## the upstream source -- pixel_buf[0]=R, [1]=G, [2]=B, [3]=A).
        Image.frombytes(
            "RGBA", (shot.width, shot.height), bytes(shot.data)
        ).save(path)

    return _save


if __name__ == "__main__":
    sys.exit(main(sys.argv))
