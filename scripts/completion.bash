# ---------------------------------------------------------------------------
# Project-local bash completion for the Jaguar 240p Test Suite Makefile.
#
# Sources targets from `make -qpRr` so it always matches the live makefile,
# including targets defined via `include`d sub-makefiles that bash-completion's
# stock _make handler can miss.
#
# Install (one of):
#   1. ad-hoc:    source scripts/completion.bash
#   2. .bashrc:   source /path/to/atari_jaguar_240p_test_suite/scripts/completion.bash
# ---------------------------------------------------------------------------

_jag_make_completion () {
  local cur prev makefile targets
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"

  for makefile in GNUmakefile Makefile makefile; do
    [[ -r $makefile ]] && break
  done
  [[ -r $makefile ]] || return 0

  targets=$(make -qpRr 2>/dev/null \
    | awk -F: '
        /^# Not a target:/ { skip = 1; next }
        skip { skip = 0; next }
        /^[a-zA-Z][a-zA-Z0-9._-]*:([^=]|$)/ {
          t = $1
          if (t ~ /^(Makefile|GNUmakefile|makefile)$/) next
          if (t ~ /\.(o|c|h|s|bin|cof|rom|tar\.gz|d)$/) next
          print t
        }
      ' \
    | sort -u)

  COMPREPLY=( $(compgen -W "${targets}" -- "${cur}") )
  return 0
}

complete -F _jag_make_completion make gmake
