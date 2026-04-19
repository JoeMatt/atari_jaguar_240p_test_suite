include	Makefile.config
include	Makefile.docker

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

ifeq ($(AUTO_DOCKER_NOTICE),1)
  $(info >> Native m68k-atari-mint-gcc not found; routing build through the Jaguar SDK Docker image.)
  $(info >> Set USE_DOCKER=0 to disable, or run `make help` to see every target.)
endif

PROJECT=jag_240p_test_suite
SRCC=	main.c\
	common_assets.c\
	text_engine.c\
	help.c\
	patterns.c\
	tests.c\
	controller_test.c
SRCS=
SRCH=
STRUCT_S=$(wildcard ./struct/*.s)
GFX_S=$(wildcard ./gfx/data/*.s) 
AUD_S=$(wildcard ./sound/data/*.s)
OBJS=$(SRCC:.c=.o) $(SRCS:.s=.o) $(GFX_S:.s=.o) $(STRUCT_S:.s=.o) $(AUD_S:.s=.o)
OTHEROBJS=
RMVLIBS=display.o interrupt.o sound.o rmvlib.a fb2d.o lz77.o gpudriver.o

include	Makefile.example
