#!/bin/sh
# Run the exact Phase 7 prerequisites, then the five Phase 8 boundary checks.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase8-fast.sh [--bin DIR]' >&2
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
  printf 'verify-phase8-fast: missing-configured-build: %s\n' "$bin_dir" >&2
  exit 1
fi
configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
if [ "$configured_root" != "$repo_root" ]; then
  printf 'verify-phase8-fast: wrong-source-root: %s\n' "$configured_root" >&2
  exit 1
fi

# The existing Phase 7 command builds timer_test and proves its exact fixture
# on these same inputs. Build only the other Phase 8 executable here.
cmake --build "$bin_dir" --target cpu_test
if [ ! -x "$bin_dir/cpu_test" ] || [ ! -x "$bin_dir/timer_test" ]; then
  printf '%s\n' 'verify-phase8-fast: missing-focused-test-executable' >&2
  exit 1
fi
"$repo_root/scripts/verify-phase7-fast.sh" --bin "$bin_dir"

# Inventory and command bindings are checked before any Phase 8 test runs.
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" -DCASE=verify \
  -P "$repo_root/tests/gates/phase8_selection.cmake"

expected_regex='^(cpu[.]irq[.]boundary|keypad[.](timeline|refusal)|time[.]halt[.]boundary|phase8[.]selection[.]control)$'
test_output=$(mktemp "$bin_dir/phase8-fast.XXXXXX")
trap 'rm -f "$test_output"' 0 HUP INT TERM
set +e
# Phase 7 just passed for these exact inputs. Exclude its fixture only here so
# it is not auto-added invisibly; ordinary CI/ASAN CTest runs keep both graphs.
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^phase7_fast$' -R "$expected_regex" >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  exit "$test_status"
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  printf '%s\n' 'verify-phase8-fast: skipped-or-unrun-test' >&2
  exit 1
fi
printf '%s\n' 'verify-phase8-fast: exact-five passed'
