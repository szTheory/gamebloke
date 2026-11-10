#!/bin/sh
# Case table for scripts/classify-changes.sh (D-11). Run from the source root.
#
# Line format: <expected heavy> <changed-file count> <entry|entry|...>
# `-` stands for an empty listing. Entries are split on `|`; one entry is one
# API file entry, as the workflow marks it:
#   <path>      a plain entry, fed to the classifier as `F <path>`
#   <old>><new> a rename entry, fed as `F <new>` and `R <old>`
#   raw:<line>  fed verbatim (no marker), for the unmarked and failed-listing lines
set -u
LC_ALL=C
export LC_ALL

src=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
classifier="$src/scripts/classify-changes.sh"

# stderr of the short-listing case is checked too: a wrong reason would hide a
# classifier that fails closed for the wrong cause.
cases=$(cat <<'TABLE'
false 1 .planning/STATE.md
false 2 .planning/a.md|.planning/phases/x/y.md
false 1 .planning/old.md>.planning/new.md
false 2 .planning/old.md>.planning/new.md|.planning/b.md
true 2 .planning/a.md|src/version.c
true 1 .planning/old.md>src/new.c
true 1 src/old.c>.planning/new.md
true 1 .planningx/a.md
true 1 .planning-evil/x
true 1 .planning
true 1 -
true 1 .planning/a.md|raw:(listing failed)
true 3001 .planning/a.md|.planning/b.md|.planning/c.md
true 5 .planning/a.md
true 1 .planning/
true 3 .planning/a.md|.planning/b.md
true 3 .planning/o>.planning/n|.planning/b
true 4 .planning/o>.planning/n|.planning/p>.planning/q
true 1 raw:.planning/a.md
true 1 raw:F
true 1 raw:R .planning/a.md
TABLE
)

# to_markers: table entries on stdin, marker lines on stdout.
to_markers() {
  while IFS= read -r entry; do
    case $entry in
      raw:*) printf '%s\n' "${entry#raw:}" ;;
      *'>'*) printf 'F %s\nR %s\n' "${entry#*>}" "${entry%%>*}" ;;
      *) printf 'F %s\n' "$entry" ;;
    esac
  done
}

fail=0
n=0
while IFS= read -r line; do
  [ -n "$line" ] || continue
  n=$((n + 1))
  want=${line%% *}
  rest=${line#* }
  count=${rest%% *}
  paths=${rest#* }
  if [ "$paths" = "-" ]; then
    listing=''
  else
    listing=$(printf '%s\n' "$paths" | tr '|' '\n' | to_markers)
  fi
  if [ -n "$listing" ]; then
    got=$(printf '%s\n' "$listing" | sh "$classifier" "$count" 2>/dev/null)
  else
    got=$(sh "$classifier" "$count" </dev/null 2>/dev/null)
  fi
  if [ "$got" != "heavy=$want" ]; then
    echo "classify-case: FAIL $n: wanted heavy=$want got ${got:-<nothing>}: $line"
    fail=1
  fi
done <<EOF2
$cases
EOF2

# The short, non-erroring listing must fail closed for its own reason.
err=$(printf '%s\n' 'F .planning/a.md' | sh "$classifier" 5 2>&1 >/dev/null)
case $err in
  'classify: short listing (1 entries for 5 files)') ;;
  *)
    echo "classify-case: FAIL short-listing reason: got '$err'"
    fail=1
    ;;
esac

# Rename padding: three names but two entries must not satisfy a count of three.
err=$(printf '%s\n' 'F .planning/n' 'R .planning/o' 'F .planning/b' | sh "$classifier" 3 2>&1 >/dev/null)
case $err in
  'classify: short listing (2 entries for 3 files)') ;;
  *)
    echo "classify-case: FAIL rename-padding reason: got '$err'"
    fail=1
    ;;
esac

# The workflow must emit the marker lines this classifier counts.
for frag in '"F " + .filename' '"R " + .previous_filename'; do
  if ! grep -F -q -e "$frag" "$src/.github/workflows/ci.yml"; then
    echo "classify-case: FAIL ci.yml no longer emits $frag"
    fail=1
  fi
done

# A leading-zero count is a number, not octal.
got=$(printf '%s\n' 'F .planning/a.md' | sh "$classifier" 0001 2>/dev/null)
if [ "$got" != "heavy=false" ]; then
  echo "classify-case: FAIL leading-zero count: got ${got:-<nothing>}"
  fail=1
fi

# Usage: a non-numeric or missing count exits 2 with the reason code.
for arg in abc ''; do
  if [ -n "$arg" ]; then
    err=$(sh "$classifier" "$arg" </dev/null 2>&1 >/dev/null)
    rc=$?
  else
    err=$(sh "$classifier" </dev/null 2>&1 >/dev/null)
    rc=$?
  fi
  case $err in
    'classify: usage'*) ;;
    *) echo "classify-case: FAIL usage text for '$arg': got '$err'"; fail=1 ;;
  esac
  if [ "$rc" -ne 2 ]; then
    echo "classify-case: FAIL usage exit for '$arg': wanted 2 got $rc"
    fail=1
  fi
done

[ "$fail" -eq 0 ] || exit 1
echo "classify-cases: $n ok"
