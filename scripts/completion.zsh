#compdef make gmake
# ---------------------------------------------------------------------------
# Project-local zsh completion for the Jaguar 240p Test Suite Makefile.
#
# Why ship our own?  zsh's stock `_make` parses Makefiles directly and silently
# bails on a few real-world constructs (tab between `include` and the file,
# extended-glob-only patterns, conditional `ifeq` blocks, etc).  This wrapper
# instead asks GNU make itself for the live target list via `make -qp`, which
# always matches what you'd actually be allowed to invoke.
#
# Install (one of):
#   1. fpath:        cp scripts/completion.zsh ~/.zsh/_make && compinit
#   2. ad-hoc:       source scripts/completion.zsh
#   3. .zshrc one-liner:
#        source /path/to/atari_jaguar_240p_test_suite/scripts/completion.zsh
# ---------------------------------------------------------------------------

_jag_make_targets () {
  local -a targets
  local makefile
  for makefile in GNUmakefile Makefile makefile; do
    [[ -r $makefile ]] && break
  done
  [[ -r $makefile ]] || return 1

  # `make -qpRr` dumps the entire database without running anything; we filter
  # for explicit rule lines (`name:` or `name: deps`) and drop pattern rules,
  # implicit suffixes, file targets, and make's internal bookkeeping.
  targets=( ${(f)"$(make -qpRr 2>/dev/null \
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
      | sort -u)"} )

  _describe -t targets 'make target' targets
}

_jag_make () {
  local context state line
  _arguments -C \
    '(-C --directory)'{-C,--directory=}'[change directory]:directory:_files -/' \
    '(-f --file --makefile)'{-f,--file=,--makefile=}'[read FILE as makefile]:makefile:_files' \
    '(-j --jobs)'{-j+,--jobs=}'[parallel jobs]: :_guard "[0-9]#" "number"' \
    '(-n --just-print --dry-run)'{-n,--just-print,--dry-run}"[don't run, just print]" \
    '(-B --always-make)'{-B,--always-make}'[unconditionally make all targets]' \
    '(-k --keep-going)'{-k,--keep-going}"[keep going on errors]" \
    '(-s --silent)'{-s,--silent,--quiet}'[silent mode]' \
    '*: :->target_or_var'

  case $state in
    target_or_var)
      if [[ $PREFIX == *=* ]]; then
        _message 'variable assignment'
      else
        _jag_make_targets
      fi
      ;;
  esac
}

compdef _jag_make make gmake
