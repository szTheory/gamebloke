#!/bin/sh
# Action pin gate (D-10).
#
#   check-action-pins.sh [workflow-file...]
#
# Every `uses:` line that is not a local `./` reference must read
#   uses: <owner>/<repo>[/<path>]@<40 lowercase hex> # v<digit>...
# so a retagged or moved action cannot change what the runner executes, and a
# reader can see which release the SHA stands for. With no argument every
# .yml and .yaml file under .github/workflows (repo root) is checked.
#
# Reason lines (all findings are printed; exit 1 if there is any):
#   pins: unpinned: <file>:<line>           ref is not a 40-hex SHA
#   pins: no-version-comment: <file>:<line> no `# v<digit>` comment after the ref
#   pins: ok (<n> uses)                     nothing found
# Exit 2: usage (a named file is missing, or no workflow file exists).
set -eu
LC_ALL=C
export LC_ALL

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$#" -eq 0 ]; then
  set --
  for f in "$root"/.github/workflows/*.yml "$root"/.github/workflows/*.yaml; do
    [ -f "$f" ] && set -- "$@" "$f"
  done
  if [ "$#" -eq 0 ]; then
    echo "pins: usage: no workflow files under .github/workflows" >&2
    exit 2
  fi
fi

for f in "$@"; do
  if [ ! -f "$f" ]; then
    echo "pins: usage: not a file: $f" >&2
    exit 2
  fi
done

rc=0
total=0
for f in "$@"; do
  out=$(awk -v file="$f" '
    function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
    {
      line = $0
      if (line !~ /^[ \t]*(-[ \t]+)?uses:[ \t]*/) next
      sub(/^[ \t]*(-[ \t]+)?uses:[ \t]*/, "", line)
      line = trim(line)
      # First whitespace-separated token is the reference; the rest may hold
      # the version comment. Surrounding quotes are not part of the ref.
      ref = line
      sub(/[ \t].*$/, "", ref)
      rest = substr(line, length(ref) + 1)
      if (ref ~ /^"/ || ref ~ /^\047/) ref = substr(ref, 2)
      if (ref ~ /"$/ || ref ~ /\047$/) ref = substr(ref, 1, length(ref) - 1)
      if (ref ~ /^\.\//) next          # local action or workflow
      uses++
      at = index(ref, "@")
      sha = (at > 0) ? substr(ref, at + 1) : ""
      if (length(sha) != 40 || sha !~ /^[0-9a-f]+$/) {
        print "pins: unpinned: " file ":" NR
        bad = 1
      }
      if (rest !~ /^[ \t]*#[ \t]*v[0-9]/) {
        print "pins: no-version-comment: " file ":" NR
        bad = 1
      }
    }
    END { print "USES " uses + 0; exit bad ? 1 : 0 }
  ' "$f") || rc=1
  count=$(printf '%s\n' "$out" | sed -n 's/^USES //p')
  total=$((total + ${count:-0}))
  printf '%s\n' "$out" | grep -v '^USES '  || true
done

if [ "$rc" -ne 0 ]; then
  exit 1
fi
echo "pins: ok ($total uses)"
