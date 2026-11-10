#!/bin/sh
# Static trap gate (CR-01). In POSIX sh a trap action for a signal runs and the
# script then RESUMES, so `trap '<cleanup>' EXIT HUP INT TERM` lets an
# interrupted scan continue with its temporary files already deleted and report
# a clean result. Every script that cleans up on EXIT must end on a signal:
#
#   trap '<cleanup>' EXIT
#   trap 'exit 129' HUP
#   trap 'exit 130' INT
#   trap 'exit 143' TERM
#
#   check-traps.sh [<root>]   scan scripts/ .githooks/ tests/ under <root> (default: the repository)
#
# Reason codes (each finding prints `traps: <code>: <path>[:<line>|: <SIGNAL>]`):
#   exit-with-signals   one trap line names EXIT together with HUP, INT, TERM or QUIT
#   no-signal-handler   the file traps EXIT but has no `trap 'exit <n>' <SIGNAL>` for HUP, INT, or TERM
#   no-files            nothing was scanned (a renamed directory must not pass silently)
#   usage
# Exit 0 clean, 1 on findings, 2 on a usage error.
set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -gt 1 ]; then
  echo 'traps: usage: check-traps.sh [<root>]' >&2
  exit 2
fi
if [ "$#" -eq 1 ]; then
  root=$1
else
  root=$(cd "$(dirname "$0")/.." && pwd -P)
fi
[ -d "$root" ] || { echo "traps: usage: not a directory: $root" >&2; exit 2; }

nl='
'
files=$(find "$root/scripts" "$root/.githooks" "$root/tests" -type f \
  \( -name '*.sh' -o -path '*/.githooks/*' \) -print 2>/dev/null | sort || true)
if [ -z "$files" ]; then
  echo "traps: no-files: no shell script found under $root"
  exit 1
fi

trap_line='^[[:space:]]*trap[[:space:]]'
exit_word='[[:space:]]EXIT([[:space:]]|$)'
sig_word='[[:space:]](HUP|INT|TERM|QUIT)([[:space:]]|$)'

findings=''
count=0
IFS=$nl
for f in $files; do
  count=$((count + 1))
  rel=${f#"$root"/}
  traps=$(grep -n -E -e "$trap_line" -- "$f" || true)
  [ -n "$traps" ] || continue

  both=$(printf '%s\n' "$traps" | grep -E -e "$exit_word" | grep -E -e "$sig_word" | cut -d: -f1 || true)
  for n in $both; do
    findings="${findings}traps: exit-with-signals: $rel:$n$nl"
  done

  if printf '%s\n' "$traps" | grep -Eq -e "$exit_word"; then
    for sig in HUP INT TERM; do
      handler="^[0-9]+:[[:space:]]*trap[[:space:]]+'[^']*exit[[:space:]]+[1-9][0-9]*[^']*'[[:space:]]+$sig([[:space:]]|\$)"
      if ! printf '%s\n' "$traps" | grep -Eq -e "$handler"; then
        findings="${findings}traps: no-signal-handler: $rel: $sig$nl"
      fi
    done
  fi
done

if [ -n "$findings" ]; then
  printf '%s' "$findings"
  exit 1
fi
echo "traps: ok ($count files)"
