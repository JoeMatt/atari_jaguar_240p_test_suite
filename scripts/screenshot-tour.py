#!/usr/bin/env python3
"""Walk the Jaguar 240p test suite menus, capture one PNG per screen.

Drives a Jaguar libretro core through a scripted joypad input sequence
and dumps the framebuffer to PNG at named checkpoints. Output lands
under ``screenshots/<group>/<NN>-<slug>.png`` and a ``manifest.json``
indexes everything for the README stitcher.

Usage:
    scripts/screenshot-tour.py CORE.dylib CONTENT.j64
    scripts/screenshot-tour.py --out screenshots --group main \\
        ./virtualjaguar_libretro.dylib jag_240p_test_suite.jag

Local-only: needs the libretro core .dylib + libretro.py venv
(``make libretro-venv``). Run via ``make screenshots`` for the wired
defaults.

RetroPad -> Jaguar mapping used by virtualjaguar-libretro defaults:
    RetroPad A      -> Jaguar A          (confirm in test suite menus)
    RetroPad B      -> Jaguar B          (back / cancel)
    RetroPad Y      -> Jaguar C
    RetroPad SELECT -> Jaguar Pause
    RetroPad START  -> Jaguar Option     (return to main / toggle)

The button names in the tour DSL below are the Jaguar names; the
generator translates them to RetroPad fields automatically.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Step:
    """One frame-window in the scripted tour.

    label   -- if set, capture screenshot at the END of this step
               and record it under ``<group>/<NN>-<label>.png``.
    button  -- Jaguar button name to PRESS at the start of this step
               (None = idle). Repository-wide search-friendly names:
               a, b, c, option, pause, up, down, left, right, x, y, l, r.
    frames  -- total frames the step covers. Unless ``hold=True``, the
               button is asserted for ``min(TAP_PRESS_FRAMES, frames)``
               frames (a short tap -- see TAP_PRESS_FRAMES below for
               why a 1-frame tap was empirically too short to clear
               the cart's debouncer), then released for the remaining
               frames so the test suite sees a clean press+release.
               Most menus need 5+ idle frames after a press to settle
               / debounce / animate. Use ``hold=True`` for the rare
               case you genuinely need a held button (none of the
               current menus require it).
    hold    -- assert the button for ALL ``frames`` instead of just the
               TAP_PRESS_FRAMES tap-width above. Defaults False because
               holding A on most test screens registers as repeated
               taps, immediately exiting.
    note    -- optional human caption surfaced in the README gallery.
    """

    label: str | None
    button: str | None
    frames: int
    note: str = ""
    hold: bool = False


@dataclass
class TourGroup:
    """Logical group of screenshots, becomes a <details> in the README."""

    slug: str
    title: str
    steps: list[Step] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Tour script.
#
# Each TourGroup is a SELF-CONTAINED sequence starting from a fresh
# core boot. We deliberately don't try to navigate back out of a test
# or sub-menu: the cart's controllerLock / scrollLock state-machine
# was designed for human input timing and resists being "rewound" by
# a scripted poll stream (OPTION presses get debounced away when sent
# through libretro.py's IterableInputDriver -- still investigating).
#
# Restarting the session per group costs ~0.5s of dlopen + retro_init
# overhead but eliminates entire classes of cross-group state bleed
# (cursor position memory, lingering controllerLock, half-rendered
# sprites). For 9 groups that's ~5s of fixed cost on top of ~13s of
# emulation, well worth the determinism.
#
# Navigation idioms baked into the helpers below:
#   - Sub-menus list "Back to Main Menu" as the LAST entry; we never
#     need to press it because we just re-boot.
#   - From the boot main menu, item N is reached with N DOWN presses
#     (0=Test Patterns, 1=Video Tests, ..., 7=Credits).
#
# Frame budgets are NTSC-biased (60 Hz). PAL adds ~17% wall-clock but
# is well within debounce tolerance. The virtualjaguar libretro core
# fastboots straight into the cart with no BIOS splash, so the
# 90-frame initial hold just covers the cart's own title delay +
# initial menu fade-in.
# ---------------------------------------------------------------------------

# Frames between distinct button presses. Long enough that the cart's
# scrollLock decay has finished so the next press isn't dropped.
SETTLE_FRAMES = 25
BOOT_FRAMES = 90


def _boot() -> list[Step]:
    return [Step(None, None, BOOT_FRAMES, "boot + initial settle")]


def _press(button: str, frames: int = 5) -> Step:
    return Step(None, button, frames, "")


def _settle(frames: int = SETTLE_FRAMES) -> Step:
    return Step(None, None, frames, "")


def _capture(label: str, note: str) -> Step:
    return Step(label, None, 1, note)


def _open_main_item(n_down: int) -> list[Step]:
    """From boot main menu, scroll DOWN n_down times and press A.
    Used as the entry sequence for every sub-menu group."""
    steps: list[Step] = []
    for _ in range(n_down):
        steps.append(_press("down"))
        steps.append(_settle(8))
    steps.append(_press("a"))
    steps.append(_settle())
    return steps


TOUR: list[TourGroup] = [
    TourGroup(
        slug="main",
        title="Main menu",
        steps=[
            *_boot(),
            _capture("main-menu", "Top-level menu (8 entries)"),
        ],
    ),
    TourGroup(
        slug="patterns",
        title="Test patterns",
        steps=[
            *_boot(),
            *_open_main_item(0),
            _capture("patterns-menu", "Test Patterns sub-menu"),
            _press("a"),
            _settle(),
            _capture("patterns-pluge", "Pluge (first pattern)"),
        ],
    ),
    TourGroup(
        slug="video",
        title="Video tests",
        steps=[
            *_boot(),
            *_open_main_item(1),
            _capture("video-menu", "Video Tests sub-menu"),
        ],
    ),
    TourGroup(
        slug="audio",
        title="Audio tests",
        steps=[
            *_boot(),
            *_open_main_item(2),
            _capture("audio-menu", "Audio Tests sub-menu"),
        ],
    ),
    TourGroup(
        slug="hardware",
        title="Hardware tools",
        steps=[
            *_boot(),
            *_open_main_item(3),
            _capture("hardware-menu", "Hardware Tools sub-menu"),
        ],
    ),
    TourGroup(
        slug="eeprom",
        title="EEPROM test",
        steps=[
            ## Boot -> Hardware Tools sub-menu, then walk DOWN x9 to
            ## land on "EEPROM Test". The Hardware menu cursor starts
            ## on Controller Test (menuState=1, switch case 1), and
            ## EepromTest() lives at switch case 10 (just after Video
            ## Mode Test at case 9), so 9 DOWN presses moves the
            ## cursor from case 1 -> case 10. Press A to enter and
            ## capture the 8x8 hex grid -- the test snapshots the
            ## chip on entry and verifies-on-exit, so this is safe to
            ## script repeatedly.
            *_boot(),
            *_open_main_item(3),
            *(s for _ in range(9) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(),
            _capture("eeprom-grid", "EEPROM Test grid (64 words, hex)"),
        ],
    ),
    TourGroup(
        slug="sprite-stress",
        title="Sprite stress test",
        steps=[
            ## Hardware Tools sub-menu, walk DOWN x10 to land on
            ## "Sprite Stress Test" (lineTextBox[11], one slot below
            ## EEPROM at [10]). The test enters with spriteCount=1 and
            ## SIZE=8x8 / SINGLE-LINE mode -- safe baseline that
            ## doesn't yet saturate the OP, so the framebuffer still
            ## renders cleanly for a screenshot.
            *_boot(),
            *_open_main_item(3),
            *(s for _ in range(10) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(),
            _capture("sprite-stress-default", "Sprite Stress Test default (1 sprite, 8x8, single-line)"),
        ],
    ),
    TourGroup(
        slug="ycdelay",
        title="Y/C delay pattern",
        steps=[
            ## Test Patterns sub-menu cursor starts on Pluge (case 1).
            ## "More Patterns..." is item 15 (14 DOWN), then Y/C Delay
            ## is item 6 inside More Patterns (5 DOWN from default
            ## cursor on Color Bars w/ Gray at item 1). The pattern's
            ## first redraw allocates 6 fullscreen-strip sprites + a
            ## label textBox -- give it 60 frames of settle so the
            ## OP has finished compositing before we grab the FB.
            *_boot(),
            *_open_main_item(0),
            *(s for _ in range(14) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(),
            *(s for _ in range(5) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(60),
            _capture("ycdelay-strips", "Y/C Delay (RGB+CMY strips with 1px white dividers)"),
        ],
    ),
    TourGroup(
        slug="diagonal",
        title="Diagonal / clock pattern",
        steps=[
            ## Same path as ycdelay but Diagonal is item 7 (6 DOWN
            ## inside More Patterns). Default spacing=8px and not
            ## inverted -- canonical form for the screenshot.
            *_boot(),
            *_open_main_item(0),
            *(s for _ in range(14) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(),
            *(s for _ in range(6) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(60),
            _capture("diagonal-default", "Diagonal / Clock pattern (8 px spacing, default invert off)"),
        ],
    ),
    TourGroup(
        slug="vertscroll",
        title="Vertical scroll test",
        steps=[
            ## Video Tests cursor starts on Drop Shadow (case 1).
            ## Vertical Scroll is item 7 (6 DOWN), inserted right
            ## after the existing horizontal Scroll Test. Bumped
            ## settle to 60: the test allocates a 320x16 procedural
            ## bar sprite + label, and the FIRST scroll tick has to
            ## land at a non-trivial Y so the bars are actually in
            ## the visible area when we capture.
            *_boot(),
            *_open_main_item(1),
            *(s for _ in range(6) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(60),
            _capture("vertscroll-running", "Vertical Scroll Test (default speed 1, dir DN)"),
        ],
    ),
    TourGroup(
        slug="white-noise",
        title="White noise test",
        steps=[
            ## Audio Tests cursor starts on Sound Test (case 1).
            ## White Noise is item 5 (4 DOWN). Capture the IDLE state
            ## -- screenshot doesn't need playback, just the UI.
            *_boot(),
            *_open_main_item(2),
            *(s for _ in range(4) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(45),
            _capture("white-noise-idle", "White Noise Test (idle, ready to play)"),
        ],
    ),
    TourGroup(
        slug="pink-noise",
        title="Pink noise test",
        steps=[
            ## Audio Tests, Pink Noise is item 6 (5 DOWN). Pink takes
            ## noticeably longer than white because it runs Paul
            ## Kellet's 5-stage IIR filter over all 16384 samples on
            ## entry; the 32 KiB buffer fill blocks the main loop so
            ## the framebuffer stays black until it finishes. 240
            ## frames (~4 s NTSC) gives a comfortable margin for the
            ## buffer fill to complete before the capture frame.
            ##
            ## Some libretro builds composite only a top band in headless
            ## `retro_run` (main-menu capture <2k non-zero pixels). Use a
            ## core that passes a quick ``--group main`` check before
            ## trusting the pink-noise capture.
            *_boot(),
            *_open_main_item(2),
            *(s for _ in range(5) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(240),
            _capture("pink-noise-idle", "Pink Noise Test (idle, ready to play)"),
        ],
    ),
    TourGroup(
        slug="channel-sep",
        title="L/R channel separation",
        steps=[
            ## Audio Tests, L/R Channel Separation is item 7 (6 DOWN).
            ## Default mode is BOTH (in phase), playback OFF.
            *_boot(),
            *_open_main_item(2),
            *(s for _ in range(6) for s in (_press("down"), _settle(8))),
            _press("a"),
            _settle(45),
            _capture("channel-sep-default", "Channel Separation (BOTH in-phase, OFF)"),
        ],
    ),
    TourGroup(
        slug="screensavers",
        title="Screen savers",
        steps=[
            *_boot(),
            *_open_main_item(4),
            _capture("screensavers-menu", "Screen Savers sub-menu"),
        ],
    ),
    TourGroup(
        slug="help",
        title="Help",
        steps=[
            *_boot(),
            *_open_main_item(5),
            _capture("help-menu", "Help screen"),
        ],
    ),
    TourGroup(
        slug="options",
        title="Options",
        steps=[
            *_boot(),
            *_open_main_item(6),
            _capture("options-menu", "Options screen"),
        ],
    ),
    TourGroup(
        slug="credits",
        title="Credits",
        steps=[
            *_boot(),
            *_open_main_item(7),
            _capture("credits-screen", "Credits"),
        ],
    ),
]


# Translate Jaguar button name -> JoypadState constructor kwargs.
# Mapping derived from virtualjaguar_libretro_core_options.h defaults
# (see module docstring above).
_BUTTON_KWARGS = {
    "a":      {"a": True},
    "b":      {"b": True},
    "c":      {"y": True},
    "pause":  {"select": True},
    "option": {"start": True},
    "x":      {"x": True},
    "l":      {"l": True},
    "r":      {"r": True},
    "up":     {"up": True},
    "down":   {"down": True},
    "left":   {"left": True},
    "right":  {"right": True},
}


# Press width for taps. Empirically, the test suite's controllerLock /
# scrollLock debouncer drops 1-frame and 2-frame inputs (likely a
# vsync-vs-poll race -- the input poll runs early in the frame, before
# the menu loop reads `joy1`, so a single-frame yield can be entirely
# absorbed by the lock state-machine). 3 frames is the empirical floor
# where every tap registers; 4 gives a comfortable margin.
TAP_PRESS_FRAMES = 4


def _flatten(tour: list[TourGroup]) -> list[tuple[str, str | None, str | None, str]]:
    """Flatten the tour DSL into a per-frame action list.

    Returns one entry per frame: (group, button_name_or_None,
    capture_label_or_None, note). The capture label is only set on the
    LAST frame of a labeled step. For a non-hold step the button is
    asserted for the first ``min(TAP_PRESS_FRAMES, frames)`` frames
    (long enough to clear the in-ROM debouncer) and then released for
    the remainder; for a hold step it is asserted for every frame.
    Steps shorter than ``TAP_PRESS_FRAMES`` will press for the entire
    duration and skip the release window -- valid for chaining presses
    but the menu will see two consecutive frames as one held button.
    """
    actions: list[tuple[str, str | None, str | None, str]] = []
    for group in tour:
        for step in group.steps:
            if step.frames < 1:
                raise ValueError(f"step.frames must be >=1, got {step.frames}")
            press_until = step.frames if step.hold else min(TAP_PRESS_FRAMES, step.frames)
            for i in range(step.frames):
                last = i == step.frames - 1
                btn = step.button if i < press_until else None
                actions.append(
                    (
                        group.slug,
                        btn,
                        step.label if (last and step.label) else None,
                        step.note if (last and step.note) else "",
                    )
                )
    return actions


def _input_generator(actions: list[tuple[str, str | None, str | None, str]]):
    """libretro.py SessionBuilder.with_input() consumes an InputStateGenerator
    -- a callable returning an iterator of per-poll JoypadState. The core
    polls once per frame, so this iterator yields exactly one state per
    queued action."""
    from libretro import JoypadState

    def _gen() -> Iterator:
        for _group, button, _label, _note in actions:
            kwargs = _BUTTON_KWARGS.get(button or "", {})
            yield JoypadState(**kwargs)

    return _gen


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    p.add_argument("core", type=Path, help="libretro core .dylib/.so/.dll")
    p.add_argument("content", type=Path, help="ROM (.j64 / .jag / .cof)")
    p.add_argument("--out", type=Path, default=Path("screenshots"),
                   help="output directory (default: ./screenshots)")
    p.add_argument("--group", action="append", default=None,
                   help="only run these tour group slugs (repeatable)")
    p.add_argument("--list", action="store_true",
                   help="list group slugs + step counts and exit")
    p.add_argument("--verbose", action="store_true",
                   help="print per-frame action log")
    args = p.parse_args(argv[1:])

    if args.list:
        for g in TOUR:
            captures = sum(1 for s in g.steps if s.label)
            frames = sum(s.frames for s in g.steps)
            print(f"  {g.slug:10s}  {captures:2d} shots  {frames:4d} frames  {g.title}")
        return 0

    if not args.core.is_file():
        print(f"core not found: {args.core}", file=sys.stderr)
        return 66
    if not args.content.is_file():
        print(f"content not found: {args.content}", file=sys.stderr)
        return 66

    try:
        from libretro import SessionBuilder
        from libretro.drivers.input import IterableInputDriver
    except ImportError:
        print(
            "libretro.py is not installed. From the repo root:\n"
            "  make libretro-venv\n"
            "  .venv-libretro/bin/python scripts/screenshot-tour.py ...",
            file=sys.stderr,
        )
        return 1

    tour = [g for g in TOUR if not args.group or g.slug in args.group]
    if not tour:
        print(f"!! no tour groups matched {args.group!r}", file=sys.stderr)
        print("   available: " + ", ".join(g.slug for g in TOUR), file=sys.stderr)
        return 64

    save_png = _resolve_png_writer()
    if save_png is None:
        return 1

    args.out.mkdir(parents=True, exist_ok=True)
    ## Heterogeneous payload: `width`/`height` are int while everything
    ## else is str -- ``object`` is the honest annotation. Avoids
    ## misleading callers (and Copilot reviewers) into thinking the
    ## manifest is a uniform string map.
    manifest: list[dict[str, object]] = []
    missed: list[str] = []
    total_frames = sum(sum(s.frames for s in g.steps) for g in tour)
    total_caps = sum(sum(1 for s in g.steps if s.label) for g in tour)
    print(f">> tour: {len(tour)} group(s), {total_frames} frame(s), "
          f"{total_caps} screenshot(s)", flush=True)

    for group in tour:
        rc = _run_group(
            group,
            core=args.core,
            content=args.content,
            out=args.out,
            manifest=manifest,
            missed=missed,
            save_png=save_png,
            verbose=args.verbose,
            session_builder_cls=SessionBuilder,
            iterable_input_driver_cls=IterableInputDriver,
        )
        if rc != 0:
            return rc

    manifest_path = args.out / "manifest.json"
    manifest_path.write_text(
        json.dumps(
            {
                "core": str(args.core),
                "content": str(args.content),
                "shots": manifest,
                "groups": [{"slug": g.slug, "title": g.title} for g in tour],
            },
            indent=2,
        )
    )
    print(f">> wrote manifest: {manifest_path} ({len(manifest)} shots)", flush=True)

    ## Treat any labeled-step that returned no framebuffer as a hard
    ## failure: a "successful" make screenshots run with a stale or
    ## incomplete manifest is worse than a noisy red CI (silent drift
    ## defeats the whole point of make screenshots-check).
    if missed:
        print(f"!! {len(missed)} capture(s) had no framebuffer:",
              file=sys.stderr)
        for label in missed:
            print(f"   - {label}", file=sys.stderr)
        return 3
    return 0


def _run_group(
    group: TourGroup,
    *,
    core: Path,
    content: Path,
    out: Path,
    manifest: list,
    missed: list[str],
    save_png,
    verbose: bool,
    session_builder_cls,
    iterable_input_driver_cls,
) -> int:
    """Boot the core fresh and execute one TourGroup. Captures emit
    PNGs and append manifest entries in-place. Labeled steps that
    return no framebuffer are appended to ``missed`` (qualified as
    ``<group>/<label>``) so the caller can fail the whole run.
    Returns 0 on success or a non-zero exit code on failure."""
    actions = _flatten([group])
    captures = sum(1 for _, _, label, _ in actions if label)
    print(f">> [{group.slug}] {len(actions)} frames, "
          f"{captures} shot(s) -- {group.title}", flush=True)
    builder = (
        session_builder_cls.defaults(str(core))
        .with_content(str(content))
        .with_input(iterable_input_driver_cls(_input_generator(actions)))
    )
    try:
        with builder.build() as session:
            for i, (_g, button, label, note) in enumerate(actions):
                session.run()
                if verbose:
                    btn = button or "-"
                    cap = f" -> {label}" if label else ""
                    print(f"   f{i:04d} {btn:7s}{cap}", flush=True)
                if not label:
                    continue
                shot = session.video.screenshot()
                if shot is None:
                    print(f"!! [{group.slug}] frame {i}: "
                          f"no framebuffer for {label}", file=sys.stderr)
                    missed.append(f"{group.slug}/{label}")
                    continue
                group_dir = out / group.slug
                group_dir.mkdir(parents=True, exist_ok=True)
                idx = sum(1 for m in manifest if m["group"] == group.slug)
                rel = f"{group.slug}/{idx:02d}-{label}.png"
                save_png(shot, out / rel)
                manifest.append({
                    "group": group.slug,
                    "title": group.title,
                    "label": label,
                    "note": note,
                    "path": rel,
                    "width": shot.width,
                    "height": shot.height,
                })
                print(f"   {rel}  ({shot.width}x{shot.height})", flush=True)
    except Exception as exc:
        print(f"!! [{group.slug}] failed: {type(exc).__name__}: {exc}",
              file=sys.stderr)
        return 2
    return 0


def _resolve_png_writer():
    """Return ``callable(shot, path)`` that writes PNG via Pillow; auto-installs
    Pillow into the active venv on first use. Lifted from libretro-load-test.py
    to avoid a cross-script import cycle."""
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
        ## ArrayVideoDriver hands us a tightly-packed RGBA8888 buffer
        ## (pixel_buf[0]=R, [1]=G, [2]=B, [3]=A) regardless of the
        ## source pixel format -- Pillow consumes that natively.
        Image.frombytes(
            "RGBA", (shot.width, shot.height), bytes(shot.data)
        ).save(path)

    return _save


if __name__ == "__main__":
    sys.exit(main(sys.argv))
