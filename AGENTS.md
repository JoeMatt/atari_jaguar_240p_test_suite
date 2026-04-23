# Agent guidelines — Atari Jaguar 240p Test Suite (JoeMatt fork)

This file is the canonical onboarding doc for AI coding agents (Cursor,
Claude Code, Codex, etc.) working in this repository. Read it before
making changes; it captures hard-won context that is not obvious from the
source alone.

A `CLAUDE.md` symlink at the repo root points here for Claude Code
compatibility — keep both names in sync by editing this file only.

---

## What this fork is

Upstream is the abandoned [BitJag/atari_jaguar_240p_test_suite][upstream].
This fork (JoeMatt) keeps the original test-suite source intact and adds:

- A reproducible Docker / Colima-based SDK so macOS, Linux, and CI all
  build with identical bytes (no more "works on my machine" toolchain
  drift).
- A self-routing `Makefile` that picks native vs container automatically.
- GitHub Actions CI: builds release + debug ROMs on every PR, attaches
  them as artifacts, ships GitHub Releases on tag push, and runs a
  libretro boot smoke test on every push.
- A pure-Python port of Tursilion's `makefastboot` so we don't depend on
  a Windows-only Visual Studio project for cart header generation.
- New on-screen tests carried over from the Genesis / Dreamcast / SNES
  240p suites — see [Test additions](#test-additions-vs-upstream) below.
- Local smoke-test harnesses: `make test` (libretro), `make test-mame`
  (independent emulator), `make doctor` (toolchain probe).
- An automated screenshot gallery embedded in `README.md`
  (`make screenshots && make screenshots-readme`).

[upstream]: https://github.com/BitJag/atari_jaguar_240p_test_suite

### Test additions vs upstream

Documented in `README.md` under *What's included*; bolded items are new
or substantially rewritten by this fork:

| Menu | New / rewritten tests |
|---|---|
| Test Patterns | More Patterns sub-menu (Color Bars w/ Gray, Linearity, Phase, Brightness, Contrast) |
| Video Tests | Striped Sprite, Manual Lag Test, Alternate 240p/480i |
| Audio Tests | L/R Balance + 1 kHz Tone, MDFourier Sweep |
| Hardware Tools | Pro Controller Test, Rotary Controller Test, System Info, Jaguar CD Probe, Video Mode Test (Resolution Switching), EEPROM Read & Write Test (93C46) |
| Screen Savers | Color Cycle, Bouncing Square, Scrolling Bars |

Procedurally generated wherever possible (see `extra_tests.c`,
`controller_test.c`) so we don't expand the asset pipeline. Every new
test ships with a Help screen entry in `help.c` — keep that invariant.

### License

GPL-2.0-or-later (verbatim grant from the upstream BitJag README).
`LICENSE` at the repo root contains the full GPL v2 text and `README.md`
asserts the "or any later version" clause via
`SPDX-License-Identifier: GPL-2.0-or-later`. GitHub's `licensee`
detector currently labels the repo `GPL-2.0` because it ignores prose
"or later" grants ([licensee/licensee#444][licensee-444]); we keep the
SPDX line + README clarification as the authoritative declaration.

[licensee-444]: https://github.com/licensee/licensee/issues/444

---

## Build system topology

```
make (top-level)
├── Makefile              ← entry point, default-goal selection, .PHONY hoisting
├── Makefile.config       ← MACFLAGS, ALNFLAGS, JAGPATH, DEBUG knobs
├── Makefile.docker       ← Docker / Colima wrappers, SDK_OWNER autodetect
└── Makefile.example      ← actual build rules (objs, .cof, .bin, .rom, .j64, dist)
```

### Default-goal logic (Makefile)

The top `Makefile` probes the host:

| `m68k-atari-mint-gcc` | `docker` | Default goal | Notes |
|-----------------------|----------|--------------|-------|
| present               | any      | `all`        | native build |
| missing               | present  | `docker-build` | prints a one-line `>> routing through SDK container` notice |
| missing               | missing  | `help-toolchain` | actionable error: install crossmint, Docker, or `brew install colima docker` |

Override with `USE_DOCKER=0` (force native) or `USE_DOCKER=1` (force
container).

### Colima auto-start (Makefile.docker)

Every docker-using target carries a `| docker-up` order-only prereq.
`docker-up` is a fast no-op if `docker info` succeeds; otherwise, if
`colima` is on PATH, it boots the VM (`colima start --profile default
--cpu 4 --memory 6 --disk 30`) before the build runs. Tunables:

| Variable | Default | Purpose |
|---|---|---|
| `COLIMA_PROFILE` | `default` | profile name |
| `COLIMA_CPU` | `4` | vCPUs at auto-start |
| `COLIMA_MEMORY` | `6` | GiB RAM at auto-start |
| `COLIMA_DISK` | `30` | GiB disk at auto-start |
| `COLIMA_VM_TYPE` | (unset) | set to `vz` on Apple Silicon for Rosetta-backed amd64 |
| `COLIMA_EXTRA` | (unset) | pass-through extra flags |

### SDK_OWNER autodetect (Makefile.docker)

`SDK_OWNER` derives the GHCR namespace from `git remote get-url origin`
so forks transparently pull/push from their own image. Falls back to
`joematt` when no GitHub remote exists. Never hard-code a different
owner — let the autodetect handle it, override on the command line if
absolutely needed (`make SDK_OWNER=foo` or `make SDK_IMAGE=ghcr.io/foo/bar:tag`).

---

## Make parser landmines (READ BEFORE EDITING `Makefile.docker`)

GNU make's `$(shell ...)` function has two non-obvious parsing rules
that have already cost us debugging time:

1. **Parens are counted even inside quoted strings.** A sed expression
   like `'s#(foo)#bar#'` is enough to unbalance the call. Use sed
   pre/suffix-strip rules instead of grouping/backrefs.
2. **`#` always starts a comment, even mid-line inside `$(shell ...)`.**
   This will silently chop the rest of the line at the first `#`, which
   destroys `s#...#...#` sed delimiters across line continuations. **Use
   `,` (or another delimiter) when writing sed inside `$(shell ...)`.**

Both rules are documented inline above the `SDK_OWNER_DETECTED` block.
Don't remove those comments.

---

## Output formats: `.rom` vs `.j64`

`.rom` and `.j64` are **byte-identical**. The Jaguar cart memory image
format has no canonical extension; emulators and flash-cart front-ends
have settled on `.j64` (raw cart image) and `.jag` (cart image with a
410-byte JagDOS metadata header). Most modern tooling accepts either,
but some launcher UIs filter the file picker by extension.

**Important — libretro core extension filter.** The Virtual Jaguar
libretro core (`virtualjaguar_libretro.{dylib,so,dll}`) hard-codes
`valid_extensions = "j64|jag|cue|cdi|iso"` in its `retro_get_system_info`
implementation. **`.rom` is not in that list** — RetroArch (or any
libretro frontend) will refuse to load the file with
`ContentError: Content extension 'rom' is not supported by the system`,
even though the bytes are valid. Always tell users to load the `.j64`
when targeting RetroArch / any libretro core.

The `j64` make target is therefore a `cp $(ROM) $(J64)` and nothing
more — never regenerate the bytes a different way, and never let `.rom`
and `.j64` diverge for the same source. `make docker-all` and
`release.yml` both produce both files; CI uploads them as separate
artifacts so no rename is needed downstream.

We do **not** pad the cart image to a power-of-2 size (1/2/4/6 MB).
Software emulators zero-fill on load; the few flash carts that require
padding are out of scope for this fork.

### Libretro smoke-testing harness

`scripts/libretro-load-test.py` + `make libretro-test` / `make
libretro-run` / `make libretro-frames` use
[JesseTG/libretro.py][libretro-py] to spin up a real libretro core
in-process and call `retro_init` → `retro_load_game` → optional
`retro_run` loop. Use this when an emulator "just doesn't load" a
build — the core's actual rejection (extension filter, BIOS missing,
content too big, checksum mismatch, etc.) surfaces as a clean Python
exception instead of being swallowed.

`make libretro-frames` additionally dumps every Nth rendered frame as
a PNG to `./frames/`. This is the right tool when the core *does* load
the cart but renders nothing visible — you can see exactly what the
Object Processor is painting frame-by-frame instead of staring at a
black emulator window. The `--summary` flag also prints
`non_black=X.X%` per frame, which surfaces "stuck on a single splash
line" symptoms instantly (sub-2 % across hundreds of frames means the
68k almost certainly never reached `main`).

The makefile bootstraps `.venv-libretro/` automatically on first run.
We deliberately do **not** invoke libretro.py's bundled
`python -m libretro.py.test.loads_content` because typer 0.12 has an
annotation bug under Python 3.14 (`tuple[Path, ...]` blows up Click).
The standalone script bypasses typer.

> Caveat — most libretro Jaguar cores (Virtual Jaguar included) **bypass
> BIOS verification entirely**. They jump straight to `$802000` without
> validating the fastboot signature or the cart-header CRC. That makes
> them excellent for "does my code even start?" but useless for
> validating header correctness — for that, run on real hardware,
> BigPEmu, or anything else that actually executes the BIOS. The
> `verify-sig.py` byte-equality check (see below) is what guards
> header correctness in CI.

[libretro-py]: https://github.com/JesseTG/libretro.py

### macOS dylib code-signing self-heal

The libretro core is a `.dylib` on macOS. Two failure modes are baked
into the platform that bite any harness using `dlopen` (libretro.py,
custom `dlopen_test.c`, etc.) but **not** RetroArch (which is unhardened
and skips signature checks):

1. **`com.apple.quarantine`** — if the dylib was downloaded via a
   browser, Gatekeeper marks it quarantined and refuses to load it from
   a hardened-runtime process.
2. **Stale ad-hoc signature** — when the linker emits a dylib it
   ad-hoc signs the file. Any subsequent `strip` / `install_name_tool`
   / post-link patch modifies bytes *after* the signature was taken,
   so the embedded signature only covers part of the file.
   Hardened-runtime processes (anything Python-3.12+ from Homebrew,
   anything you build with default clang on macOS 10.15+) refuse to
   `dlopen` such a dylib and `SIGKILL` the host before `dyld` can
   return a useful errno. Symptom: `Killed: 9` (exit code 137) right
   after `>> about to call load_game...`, no Python traceback, lldb
   shows `code signature does not cover entire file up to signature
   in <path>`.

`make resign-core` regenerates an ad-hoc signature covering the whole
file (`codesign --force --sign -`). `make unquarantine-core` strips
quarantine xattr **and** delegates to `resign-core` if the embedded
signature is stale; both `libretro-test` (and therefore `make test`,
`make run-ui`, etc.) and the screenshot tour declare
`unquarantine-core` as a prerequisite, so the heal happens
automatically the first time a freshly downloaded core is touched.

When investigating a SIGKILL in any non-RetroArch host on macOS, try
`make unquarantine-core` *first* before reaching for lldb — 90 % of
the time it's the signature, not actual emulator code.

### MAME smoke testing (independent of libretro)

`make test-mame` runs MAME's `jaguar` driver in `-bench` mode with the
just-built `.j64` for `MAME_SECONDS` (default `5`) seconds. It is
purely a "boots without crashing on a second emulator" check — MAME's
output is captured to `mame-test.log`, stdout/stderr is asserted clean
of `Fatal error`. Notes:

- **BIOS dependency.** Requires `jagboot.rom` + `jagwave.rom` reachable
  via MAME's standard ROM search (`~/Documents/MAME/roms/jaguar/` or
  `~/.mame/roms/jaguar/`). The Makefile auto-detects the first existing
  hash dir and exports `-rompath` accordingly; override with
  `MAME_ROMPATH=...` if you keep BIOS files elsewhere. We deliberately
  do **not** pull BIOS files in CI (license).
- **Local-only.** MAME isn't on the GitHub Actions runners by default
  and the BIOS issue above means no CI smoke job exists for it. Run
  manually before tagging a release.
- **Use as a tie-breaker** when the libretro smoke job goes green but a
  user reports a black screen. MAME validates BIOS-side cart-header
  checks that Virtual Jaguar libretro skips.

### Screenshot tour for the README gallery

`scripts/screenshot-tour.py` drives the libretro core through a scripted
joypad sequence and dumps PNGs + a `manifest.json` to `screenshots/`.
Two consumers turn that manifest into something humans can browse:

- `scripts/screenshot-readme.py` rewrites the
  `<!-- screenshots:start -->` / `<!-- screenshots:end -->` block in
  `README.md` with a collapsed `<details>` gallery (renders on GitHub).
- `scripts/screenshot-html.py` writes `screenshots/index.html` — a
  single self-contained, dep-free viewer (sticky group nav,
  click-to-zoom) that opens directly in any browser. Emitted
  automatically as the tail step of `make screenshots`; regen
  standalone with `make screenshots-html`. Pure stdlib so it
  intentionally does **not** depend on the libretro venv.

Two non-obvious design choices are load-bearing:

1. **Per-`TourGroup` fresh boot.** Each tour group is its own
   independent libretro session — we boot from cold, navigate to the
   target screen, capture, exit. Trying to "back out" with
   `JOYPAD_OPTION` between groups was unreliable because the cart's
   `controllerLock` state was carried across the menu→test→menu
   transition. Restarting costs ~0.5 s per group; correctness > speed.
2. **`TAP_PRESS_FRAMES = 4`.** Single-frame button presses are missed
   by the in-cart input debouncer (see *Cart input timing* below).
   `_flatten` asserts each button press for the first 4 frames of a
   step, then idles for the rest. **Don't lower this floor without
   re-running the full tour and diffing every PNG.**
3. **Main-menu preflight (`make screenshots-preflight`).** Full
   `make screenshots` depends on it: one boot and `main-menu` capture
   to a temp directory. If the read framebuffer has fewer than 2000
   non-black RGB pixels (a broken or headless-incomplete libretro build
   may show only a thin top band), the tour exits 4 before `rm -rf
   screenshots/`, so a good tracked gallery is not deleted in vain.
   Run `make screenshots-preflight` alone when swapping
   `virtualjaguar_libretro` builds.

`make screenshots-check` (plumbed into CI for the cleanup PR series)
fails if regenerating the gallery would change `README.md`.

### Profiling: `__muldi3` and 64×64 multiply (host / libretro builds)

`__muldi3` is a **compiler runtime** symbol (GCC libgcc / LLVM compiler-rt):
signed 64×64→64 multiply. The compiler emits a call when a full 64-bit
multiply is needed and the target ABI does not lower it to one or a few
machine instructions. It is **not** Jaguar- or 240p-specific; seeing
time under `__muldi3` means some hot path is doing 64-bit multiplies
often (typical on a **32-bit** ARM slice, e.g. armv7), not that the
**emulation model** of video or timing is wrong. On **arm64** iOS/tvOS
device builds, many projects never show `__muldi3` meaningfully; `nm` on
an arm64 `virtualjaguar_libretro.dylib` may show **no** `muldi` symbols
— so always **confirm which binary and arch** were profiled (simulator
vs device, Rosetta, thin binary slice).

| Symptom | Typical cause |
| --- | --- |
| Undefined / missing `__muldi3` at **link** time | Link line missing compiler runtime (e.g. `-lgcc` / compiler-rt) or an unusual LTO/strip path. |
| **Slow** / lots of time **in** `__muldi3` at **run** time | 64-bit multiplies in a hot loop on a 32-bit target, or accidental widening. |

Neither case is, by itself, evidence of a **240p test correctness** bug
(geometry, levels, sync). Heavy time in a libgcc helper is a **ABI /
performance** signal; you still need **wrong output, wrong state, or a
failing test** to justify calling it a logic bug. Debug path: expand the
stack **above** `__muldi3` to the **call sites in project code**; if the
goal is speed on 32-bit, reduce 64-bit work in those loops (where the
Jaguar-side model allows staying in 32-bit). This repository does **not**
treat `__muldi3` in profiles as a smoking gun for headless-screenshot or
OP behavior — use framebuffer / register evidence (see *Screenshot
tour* preflight) separately.

### Text rendering: textBox sizing rules

The font + textBox engine in `text_engine.c` has a couple of sharp
edges that bite anyone writing a new on-screen test. Lessons paid for
in PR #10 (the EEPROM Test render-corruption fix):

1. **`mainFont` is fixed-width.** `main.c` initialises every glyph to
   `char_width = 5` and `kerning = 1`, so each character advances
   the cursor exactly 6 px. Use that to size every textBox: a string
   of *N* visible characters needs at least `N * 6` pixels, plus a
   safety margin for the trailing pixel (and for whatever you might
   later write into the same box via `updateLine`).
2. **Sprite framebuffer is sized at `newTextBox()` time, not at
   `updateTextBox()` time.** The sprite's pixel buffer is allocated
   from the *initial* string's `line_count` after word-wrap. If a
   later `updateTextBox()` writes a string long enough to wrap an
   extra line, drawing overflows the framebuffer and *corrupts the
   adjacent sprite's memory*. The visible symptom is a cascade of
   garbled rows in seemingly unrelated boxes — the kind of thing
   that looks like a font bug or layer collision but is actually a
   buffer overflow. Always initialise a textBox with the **worst-
   case** string it'll ever hold, even if that means a placeholder
   like `"WARN: restore failed at 0xFF -- check save data"`.
3. **`box_width` must be a multiple of 8 (one phrase).** The Object
   Processor expects `DEPTH8` sprite widths to be phrase-aligned.
   Misaligned widths (300, 268, etc.) render with subtle pixel
   smearing or stride glitches. Every existing test uses 64, 80,
   128, 192, 224, 256, 320 — stick to that ladder. 320 is the full
   NTSC screen width and is a safe default for any box that has to
   display variable-length text.
4. **`updateLine(tb, text, ...)` reallocates `tb->text`** (free +
   malloc + strncpy). That's fine for occasional updates but the
   per-frame call rate of a redraw loop will fragment the heap.
   For per-frame text changes prefer the BounceSquareTest pattern:
   write directly into `tb->text[i]` (which was sized by
   `newTextBox`) and call `updateTextBox(tb)`. Use `updateLine`
   only when the displayed string changes infrequently or when the
   color also needs to change.
5. **`resetTextScreen()` clears the framebuffer between updates.**
   You don't need trailing-space padding on shorter strings to
   "erase" pixels from a previous longer string — the engine
   already does that. Padding cargo-culted from ancient code just
   inflates `char_count` and risks pushing you into accidental
   wrap.

If a screen looks corrupted with a freshly written test, the cause
is almost always (2) or (3) — check those before suspecting
`fb2d_copy_straight` or sprite layer ordering.

### Cart input timing (relevant to any scripted harness)

Every menu and test in `main.c` / `patterns.c` / `controller_test.c`
uses the same gate before acting on a joypad bit:

```c
if ((settings->joy1 & JOYPAD_X) && settings->controllerLock == 0) {
    settings->controllerLock = 1;
    /* react */
}
```

`controllerLock` is cleared on the next vblank where `settings->joy1
== 0`. The practical implication for scripted input (libretro.py, MAME
auto-keypress, a custom `dlopen` host) is **a button press must be
asserted across at least one full poll-vblank-poll cycle to register**.
Empirically that's 4 frames at NTSC. Ports of the screenshot tour to
other emulators must respect this — the cart is correct, the harness
is what needs to wait. Don't "fix" it in the cart.

## ROM generation invariants (`scripts/make-rom.py`)

This script ports `makefastboot.cpp` to Python. Correctness rules
that *must* be preserved:

1. **The full input body is written to the ROM verbatim.** The original
   silently truncated 1–3 trailing bytes when the linker output wasn't
   32-bit aligned. We round *only the checksum range* down to a 4-byte
   boundary; the body length in the output ROM equals the body length in
   the input `.bin`. A `note:` is emitted on stderr if the input is
   misaligned.
2. **`JAG_MAGIC` is the seed for the running checksum**, computed over
   big-endian 32-bit words. The header layout matches makefastboot.cpp
   byte-for-byte; if you "clean it up" you will brick fastboot.
3. **`FASTBOOT_SIGNATURE` must be byte-identical to upstream.** It is
   an RSA-encrypted boot stub the Jaguar BIOS verifies before jumping
   to `$802000`. A single bit flip is enough to halt the boot on real
   hardware / BigPEmu / any BIOS-strict emulator — but it is *invisible*
   under most libretro and standalone emulators (which skip BIOS
   verification entirely). The historic regression here was byte
   `0x69` flipping `0x76 → 0x52`, which "looked fine" in libretro
   smoke tests for weeks. `scripts/verify-sig.py` (run automatically
   from `.github/workflows/build.yml` and via `make verify-sig`)
   diff-checks every byte against tursilion/makefastboot upstream and
   fails CI on any divergence. Re-run it whenever the signature
   constant is touched.
4. **Pad cart output to a 1 MiB boundary with `0xFF`.** Virtual
   Jaguar/libretro file-type detection only executes cart images when
   the file size matches its ROM heuristics (multiples of 1 MiB for
   cart images). Non-padded images may be accepted by the frontend but
   never executed by the core (classic symptom: cyan top line + black
   screen forever). Padding bytes are treated as erased flash and are
   safe on real hardware.

A round-trip smoke test lives at the bottom of any agent transcript that
modified this script — run it after any change here:

```python
data_aligned = b'\x12\x34\x56\x78' * 5
data_unaligned = data_aligned + b'\xAA\xBB\xCC'
# pipe each through scripts/make-rom.py and assert that
# rom[8192:] == original_input for both inputs.
```

---

## CI / CD

| Workflow | Trigger | Purpose |
|---|---|---|
| `.github/workflows/sdk-image.yml` | push to any branch, PRs, manual | Build SDK Docker image, push to `ghcr.io/<owner>/jaguar-sdk:{latest,sha-<short>}`. PRs build but don't push. Manual `push=false` builds locally and smoke-tests the **just-built image** (not `:latest`). |
| `.github/workflows/build.yml` | push, PR | Build release + debug `.cof`/`.bin`/`.rom`, upload as artifacts. Includes a **libretro boot smoke job** that downloads the just-built `.j64` and the libretro Linux core artifact, runs `libretro-load-test.py` for `LIBRETRO_FRAMES` frames, and asserts non-zero pixel coverage. PR-only sticky comment via `marocchino/sticky-pull-request-comment` — **guarded against fork PRs** (where `GITHUB_TOKEN` is read-only) and `continue-on-error: true` so a comment failure can't redden a green build. |
| `.github/workflows/release.yml` | tag push | Same build matrix, publishes to GitHub Releases. (Smoke test is intentionally `build.yml`-only — releases are cut off the same SHA that the smoke job already validated.) |

Permissions follow least-privilege: only the `pr-comment` job has
`pull-requests: write`; the build job is `contents: read` + `packages: read`.

### Pull-with-fallback pattern

`build.yml` and `release.yml` first try `docker pull` of the SDK image
from GHCR. If that fails (e.g. the `sdk-image.yml` workflow hasn't
finished publishing yet, or this is a fresh fork), they fall back to
`docker build` from `docker/Dockerfile` locally. Don't remove this — it
prevents race conditions when multiple workflows run in parallel.

---

## Toolchain pinning policy (`docker/Dockerfile`)

All four upstream sources are pinned to **immutable commit SHAs**, not
branch names:

| Tool | Repo | Why pinned |
|---|---|---|
| RMAC | `tiddly.mooo.com:5000/rmac/rmac.git` | upstream is `master`-only, no tags |
| RLN | `tiddly.mooo.com:5000/rln/rln.git` | same |
| jlibc | `github.com/theRemovers/jlibc` | reproducibility |
| rmvlib | `github.com/theRemovers/rmvlib` | reproducibility |

`tiddly.mooo.com` only serves git over **plain HTTP** (no HTTPS endpoint
as of 2026-04). The SHA pins are what give us integrity here — `git
checkout <sha>` will refuse to proceed if the fetched objects don't hash
to the requested SHA. **Do not switch back to branch refs.**

To bump:

```sh
git ls-remote http://tiddly.mooo.com:5000/rmac/rmac.git HEAD
git ls-remote http://tiddly.mooo.com:5000/rln/rln.git HEAD
git ls-remote https://github.com/theRemovers/jlibc.git HEAD
git ls-remote https://github.com/theRemovers/rmvlib.git HEAD
```

Update the corresponding `ARG *_REF=` lines, rebuild, smoke-test.

The clone pattern is `git clone <repo> && git -C <repo> checkout
--detach <sha>` rather than `git clone --depth 1 --branch <ref>`,
because `--branch` only accepts branch/tag names, not raw SHAs. The
repos are tiny (<2 MiB each), so the dropped `--depth 1` is irrelevant.

---

## Shell completion

Two paths, documented in README:

1. **Bundled (recommended):** source `scripts/completion.zsh` (or
   `.bash`) from `~/.zshrc` / `~/.bashrc`. These introspect `make
   -qpRr` directly so they bypass stock-completion quirks with included
   Makefiles.
2. **Stock:** `compinit` / `bash-completion` works *if* the user has a
   fresh `~/.zcompdump`. The `Makefile` is structured to cooperate with
   it: `.PHONY` is hoisted to the top, `include` directives use a
   literal space (not tab), and the target list is dense up front.

When editing the `Makefile`, never revert `include ` (space) to
`include\t` (tab) — zsh's `_make` parser silently ignores tab-included
files.

---

## Known caveats / gotchas

- **`mac -d` is broken.** The original Makefile had `MACFLAGS += -d` in
  debug builds; this causes RMAC to fail with "empty symbol" errors.
  Removed. The `-d` flag is for symbol *definition*, not debug info.
- **PPA install in slim containers** — Vincent Rivière's PPA must be
  added via manual keyring (`gpg --dearmor` to
  `/etc/apt/keyrings/vriviere.gpg`), not `add-apt-repository`. The
  latter pulls in launchpadlib and hangs in slim Ubuntu images.
- **jlibc/rmvlib `make all` builds doxygen docs.** Either install
  `doxygen` (we do) or pass `MAKEFLAGS=--no-print-directory` (we also do
  — without it, `make -C subdir`'s "Entering/Leaving directory" lines
  break the install step's `find`).
- **Colima `--vm-type vz`** on Apple Silicon enables Rosetta amd64
  emulation, which is dramatically faster than `qemu` for our amd64
  image. Document this for macOS users; don't enforce it (some users on
  older macOS lack Virtualization.framework).

---

## Conventions for changes

- **Don't add code comments that just narrate the code.** Only document
  *why* / non-obvious intent / constraints / parser landmines.
- **Don't delete existing comments or logic** without an explicit reason
  in the commit message.
- **Use `Makefile.config` for tunables**, `Makefile.example` for build
  rules, `Makefile.docker` for container wrappers, `Makefile` for entry
  logic. Don't cross the streams.
- **Per-file commits** when concerns are separable; one commit when
  changes are interleaved (like `Makefile.docker` Colima + SDK_OWNER).
  Always write commit messages that explain *why*, not just *what*.
- **Run `make help` after editing any Makefile** to verify the
  self-documenting target list still parses.
- **PR review feedback from Copilot / Qodo** should be triaged into a
  TODO list and addressed in a single follow-up commit titled
  `ci/build: address Copilot PR review feedback` (or similar).
- **Version bumps** live in exactly one place: `main.c` `drawCredits()`
  (`Ver. X.Y.Z - MM/DD/YYYY`). Tag with `vX.Y.Z` to trigger
  `release.yml`. Don't bump the version on every PR — bump on the
  commit that immediately precedes the tag.
- **README screenshots** are the source of truth for the gallery, but
  only regenerate them on user-visible UI changes (new menu entries,
  new tests, layout shifts). Re-running the tour for cosmetic-only
  edits churns the diff for no reason. Run `make screenshots-check`
  in CI to catch the cases that *do* matter.

---

## Quick reference: most common targets

```sh
make                    # auto-routes to native or docker-build
make help               # full target list
make docker-rom         # build .rom in container (release)
make docker-rom-debug   # build .rom in container (-O0 -g)
make docker-j64         # build .rom + .j64 (same bytes, different ext)
make docker-j64-debug   # build debug .rom + .j64
make docker-all         # both ROMs in one shot
make sdk-shell          # interactive shell in SDK container
make colima-start       # boot the colima VM
make help-toolchain     # actionable error when nothing is installed

# Local validation
make doctor             # probe toolchain + emulators + venv state
make test               # libretro boot smoke test (alias for libretro-test)
make test-mame          # boot the .j64 in MAME -bench (BIOS required)
make verify-sig         # diff fastboot signature vs upstream

# macOS dylib hygiene
make unquarantine-core  # strip quarantine xattr (auto re-signs if stale)
make resign-core        # force ad-hoc re-sign of the core dylib

# Screenshot gallery
make screenshots-preflight  # main menu only: fail if read framebuffer is nearly empty (no rm -rf)
make screenshots        # same as preflight, then full tour (also rebuilds index.html)
make screenshots-readme # rewrite README gallery between markers
make screenshots-html   # rewrite screenshots/index.html viewer (dep-free, no venv)
make screenshots-check  # CI-friendly: fails if README or index.html would change
```
