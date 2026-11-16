#!/bin/sh
# Fast model-time gate: validate the exact five CTest names before execution.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase7-fast.sh [--bin DIR]' >&2
  exit 2
}

bin_dir='build/ci'
while [ "$#" -gt 0 ]; do
  case $1 in
    --bin)
      [ "$#" -ge 2 ] || usage
      bin_dir=$2
      [ -n "$bin_dir" ] || usage
      shift 2
      ;;
    -*) usage ;;
    *) usage ;;
  esac
done

repo_root=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
case $bin_dir in
  /*) ;;
  *) bin_dir="$repo_root/$bin_dir" ;;
esac

if [ ! -f "$bin_dir/CMakeCache.txt" ]; then
  printf 'verify-phase7-fast: missing-configured-build: %s\n' "$bin_dir" >&2
  exit 1
fi
configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
if [ "$configured_root" != "$repo_root" ]; then
  printf 'verify-phase7-fast: wrong-source-root: %s\n' "$configured_root" >&2
  exit 1
fi

cmake --build "$bin_dir" --target timer_test
if [ ! -x "$bin_dir/timer_test" ]; then
  printf 'verify-phase7-fast: missing-timer-test-executable: %s\n' \
    "$bin_dir/timer_test" >&2
  exit 1
fi

expected_regex='^(time[.]events[.](order|boundary|refusal)|timer0[.]model[.]boundary|phase7[.]selection[.]control)$'
cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" -DCASE=verify \
  -P "$repo_root/tests/gates/phase7_selection.cmake"

test_output=$(mktemp "$bin_dir/phase7-fast.XXXXXX")
trap 'rm -f "$test_output"' 0 HUP INT TERM
set +e
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -R "$expected_regex" >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  exit "$test_status"
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  printf '%s\n' 'verify-phase7-fast: skipped-or-unrun-test' >&2
  exit 1
fi
printf '%s\n' 'verify-phase7-fast: exact-five passed'
