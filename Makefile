include Makefile.config
include Makefile.docker

# ---------------------------------------------------------------------------
# Top-level .PHONY hoisting.
#
# zsh's _make completion (and most other shells' make completion) parses
# `.PHONY:` lines from the Makefile that was passed on the command line.
# It only follows `include` directives when there's a literal SPACE after
# `include` (it does not recognise tabs), and even when it does, listing
# every public target up here makes target completion instant.
# ---------------------------------------------------------------------------
.PHONY: all rom rom-debug j64 j64-debug dist clean alpine bjl debug skunkram skunkrom \
        vjram vjrom vjj64 reset \
        sdk-pull sdk-build sdk-shell \
        docker-build docker-debug docker-rom docker-rom-debug \
        docker-j64 docker-j64-debug docker-all \
        docker-up colima-start colima-stop colima-status \
        ensure-build ensure-j64 fresh-build fresh-j64 run run-ui run-headless \
        libretro-venv libretro-test libretro-run libretro-frames \
        unquarantine-core verify-sig test test-core test-deps test-mame \
        doctor in-docker native-build help help-toolchain

# ---------------------------------------------------------------------------
# Default goal selection.
#
# Probe the host once at parse time:
#   HAS_NATIVE_CC = 1 if m68k-atari-mint-gcc is on PATH
#   HAS_DOCKER    = 1 if `docker` is on PATH
#
# Resolution order:
#   - native toolchain present  -> default to `all` (native build)
#   - no native, but Docker is  -> default to `docker-build` and print a notice
#   - neither                   -> default to `help-toolchain` (friendly error)
#
# The user can always force a specific target (e.g. `make docker-rom`,
# `make all`, `make help`) -- this only changes what bare `make` does.
# ---------------------------------------------------------------------------
HAS_NATIVE_CC	:= $(if $(shell command -v m68k-atari-mint-gcc 2>/dev/null),1,0)
HAS_DOCKER	:= $(if $(shell command -v $(DOCKER) 2>/dev/null),1,0)

# USE_DOCKER overrides auto-detection:
#   USE_DOCKER=1 -> always route through Docker (even if native cc exists)
#   USE_DOCKER=0 -> never auto-route; fail loudly if native cc is missing
ifeq ($(USE_DOCKER),1)
  .DEFAULT_GOAL := docker-build
else ifeq ($(USE_DOCKER),0)
  ifeq ($(HAS_NATIVE_CC),1)
    .DEFAULT_GOAL := all
  else
    .DEFAULT_GOAL := help-toolchain
  endif
else ifeq ($(HAS_NATIVE_CC),1)
  .DEFAULT_GOAL := all
else ifeq ($(HAS_DOCKER),1)
  .DEFAULT_GOAL := docker-build
  AUTO_DOCKER_NOTICE := 1
else
  .DEFAULT_GOAL := help-toolchain
endif

# Only print the auto-route notice when bare `make` (or an explicit native
# target) actually triggers the redirection -- not for `make help`, completion
# probes, or sdk-* / docker-* targets that the user typed deliberately.
NOTICE_GOALS := all rom rom-debug dist
ifeq ($(AUTO_DOCKER_NOTICE),1)
  ifeq ($(filter-out $(NOTICE_GOALS),$(or $(MAKECMDGOALS),all)),)
    $(info >> Native m68k-atari-mint-gcc not found; routing build through the Jaguar SDK container image.)
    ifeq ($(HAS_COLIMA),1)
      $(info >> docker daemon will be auto-started via colima if not already running.)
    endif
    $(info >> Set USE_DOCKER=0 to disable, or run `make help` to see every target.)
  endif
endif

# ---------------------------------------------------------------------------
# Auto-purge a stale .depend.
#
# The Docker SDK build writes .depend with container paths like
#   main.o: main.c main.h /opt/jagsdk/lib/include/jagdefs.h ...
# When the user later runs `make <anything>` on the host (no toolchain),
# `-include .depend` pulls those rules in and make tries to satisfy
# `/opt/jagsdk/...` as a prerequisite -> "No rule to make target ...". Stop.
#
# We detect "this .depend was written by a different filesystem" by checking
# whether ANY referenced header path still exists. If none of them do, the
# file is stale and useless on this host -- nuke it before the include below.
# (Cheap: a single `awk` pass at parse time, no external grep/find chains.)
# ---------------------------------------------------------------------------
ifneq ($(wildcard .depend),)
  DEPEND_LIVE_PATHS := $(shell awk 'NF{for(i=1;i<=NF;i++) if($$i ~ /^\//) print $$i}' .depend 2>/dev/null | sort -u | head -20)
  DEPEND_HAS_LIVE   := $(strip $(foreach p,$(DEPEND_LIVE_PATHS),$(wildcard $(p))))
  ifeq ($(DEPEND_HAS_LIVE),)
    $(info >> .depend references paths that don't exist on this host (likely a docker build); purging.)
    $(shell rm -f .depend)
  endif
endif

PROJECT=jag_240p_test_suite
SRCC=	main.c\
	common_assets.c\
	text_engine.c\
	help.c\
	patterns.c\
	tests.c\
	extra_tests.c\
	controller_test.c
SRCS=
SRCH=
STRUCT_S=$(wildcard ./struct/*.s)
GFX_S=$(wildcard ./gfx/data/*.s) 
AUD_S=$(wildcard ./sound/data/*.s)
OBJS=$(SRCC:.c=.o) $(SRCS:.s=.o) $(GFX_S:.s=.o) $(STRUCT_S:.s=.o) $(AUD_S:.s=.o)
OTHEROBJS=
RMVLIBS=display.o interrupt.o sound.o rmvlib.a fb2d.o lz77.o gpudriver.o

include Makefile.example
