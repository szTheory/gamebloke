#!/bin/sh
# Case table for scripts/check-title.sh (D-14). Run from the source root.
#
# Line format: <ok|bad> <title>   (the title is everything after the first space)
set -u
LC_ALL=C
export LC_ALL

src=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
checker="$src/scripts/check-title.sh"

fail=0
n=0

# check <ok|bad> <expected reason or -> <title>
check() {
  want=$1
  reason=$2
  title=$3
  n=$((n + 1))
  out=$(sh "$checker" "$title" 2>&1)
  rc=$?
  if [ "$want" = ok ]; then
    if [ "$rc" -ne 0 ] || [ "$out" != "title: ok" ]; then
      echo "title-case: FAIL $n: wanted ok, got exit $rc '$out': $title"
      fail=1
    fi
  else
    if [ "$rc" -ne 1 ]; then
      echo "title-case: FAIL $n: wanted exit 1, got $rc: $title"
      fail=1
    fi
    case $out in
      "$reason"*) ;;
      *)
        echo "title-case: FAIL $n: wanted reason '$reason', got '$out': $title"
        fail=1
        ;;
    esac
  fi
}

while IFS= read -r line; do
  [ -n "$line" ] || continue
  verdict=${line%% *}
  title=${line#* }
  case $verdict in
    ok) check ok - "$title" ;;
    bad) check bad 'title: not-conventional' "$title" ;;
  esac
done <<'TABLE'
ok ci: add verification foundation
ok feat(core)!: change api
ok docs(01): record evidence
ok fix(runner/cli): exit 2 on a missing file
ok revert: undo the last change
bad Add stuff
bad feat:missing space
bad wip: thing
bad feat(): empty scope
bad feat(Core): uppercase scope
bad feat:  double space
bad feat: 
bad FEAT: shouting
TABLE

# 100 characters is allowed, 101 is not.
prefix='ci: '
pad=$(printf '%*s' 96 '' | tr ' ' 'a')
check ok - "$prefix$pad"
check bad 'title: too-long' "$prefix${pad}a"

# Empty title and usage.
check bad 'title: empty' ''
n=$((n + 1))
out=$(sh "$checker" 2>&1)
rc=$?
if [ "$rc" -ne 2 ] || [ "${out#title: usage}" = "$out" ]; then
  echo "title-case: FAIL $n: no argument wanted exit 2 'title: usage', got $rc '$out'"
  fail=1
fi
n=$((n + 1))
out=$(sh "$checker" one two 2>&1)
rc=$?
if [ "$rc" -ne 2 ]; then
  echo "title-case: FAIL $n: two arguments wanted exit 2, got $rc"
  fail=1
fi

# A title is data: shell metacharacters must not run. The marker is a short
# relative name inside a scratch directory so the title stays under 100 chars.
scratch=$(mktemp -d "${TMPDIR:-/tmp}/gbl-title.XXXXXX")
(
  cd "$scratch" || exit 1
  sh "$checker" 'ci: $(touch m) `touch m`; touch m' >/dev/null 2>&1
  sh "$checker" '$(touch m)' >/dev/null 2>&1
  [ ! -e m ]
) || {
  echo "title-case: FAIL title text was executed"
  fail=1
}
n=$((n + 1))
rm -rf "$scratch"

[ "$fail" -eq 0 ] || exit 1
echo "title-cases: $n ok"
