#!/bin/sh
# Reuse one configured build for the inherited fast checks and Phase 10 PPU gates.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase10-fast.sh [--bin DIR]' >&2
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
  printf 'verify-phase10-fast: missing-configured-build: %s\n' "$bin_dir" >&2
  exit 1
fi
configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
if [ "$configured_root" != "$repo_root" ]; then
  printf 'verify-phase10-fast: wrong-source-root: %s\n' "$configured_root" >&2
  exit 1
fi

# The exact cheap set uses the three direct bus selectors and one gate script.
cmake --build "$bin_dir" --target bus_test
if [ ! -x "$bin_dir/bus_test" ]; then
  printf '%s\n' 'verify-phase10-fast: missing-focused-test-executable' >&2
  exit 1
fi

# Phase 9 invokes Phase 8, which in turn invokes Phase 7, using this same tree.
"$repo_root/scripts/verify-phase9-fast.sh" --bin "$bin_dir"

# Bind selection to this configured source/build before running any cases.
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=verify -P "$repo_root/tests/gates/phase10_selection.cmake"

expected_regex='^(ppu[.](boundary|registers|refusal)|phase10[.](policy([.]control)?|selection[.]control))$'
test_output=$(mktemp "$bin_dir/phase10-fast.XXXXXX")
trap 'rm -f "$test_output"' 0 HUP INT TERM
set +e
# The exact Phase 7/8/9 predecessor closures just passed for these inputs.
# Exclude only those fixtures here; normal CI/ASAN keeps the full graph.
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast)$' -R "$expected_regex" \
  >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  exit "$test_status"
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  printf '%s\n' 'verify-phase10-fast: skipped-or-unrun-test' >&2
  exit 1
fi
printf '%s\n' 'verify-phase10-fast: exact-six passed'
