#!/bin/sh
# Run the exact Phase 11 sampled-row selectors on one existing configured tree.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase11-fast.sh [--bin DIR]' >&2
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

# Reject missing/foreign caches before the predecessor helper can build.
if [ ! -f "$bin_dir/CMakeCache.txt" ]; then
  printf 'verify-phase11-fast: missing-configured-build: %s\n' "$bin_dir" >&2
  exit 1
fi
configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
if [ "$configured_root" != "$repo_root" ]; then
  printf 'verify-phase11-fast: wrong-source-root: %s\n' "$configured_root" >&2
  exit 1
fi
lane=$(sed -n 's/^GBL_LANE:STRING=//p' "$bin_dir/CMakeCache.txt")
case $lane in
  ci)
    expected_regex='^(ppu[.](latch|registers|refusal)|phase11[.]selection[.]control|profile[.](lint|nc[.]ppu_policy))$'
    expected_names='phase11.selection.control
ppu.latch
ppu.registers
ppu.refusal
profile.lint
profile.nc.ppu_policy'
    expected_count=6
    ;;
  asan)
    expected_regex='^(ppu[.](latch|registers|refusal)|phase11[.]selection[.]control)$'
    expected_names='phase11.selection.control
ppu.latch
ppu.registers
ppu.refusal'
    expected_count=4
    ;;
  *)
    printf 'verify-phase11-fast: unsupported-lane: %s\n' "$lane" >&2
    exit 1
    ;;
esac

# Phase 10 runs the existing focused target once and establishes the inherited
# Phase 7/8/9/10 closure on this same configured tree.
"$repo_root/scripts/verify-phase10-fast.sh" --bin "$bin_dir"

# Bind this invocation to the same source/build before running Phase 11 cases.
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=verify -P "$repo_root/tests/gates/phase11_selection.cmake"
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=order -P "$repo_root/tests/gates/phase11_selection.cmake"

test_output=$(mktemp "$bin_dir/phase11-fast.XXXXXX")
trap 'rm -f "$test_output"' 0 HUP INT TERM
# Phase 14's outer helper runs its setup before entering the inherited chain;
# exclude that later fixture to prevent nested CTest expansion.
set +e
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast|phase10_fast|phase14_fast)$' \
  -R "$expected_regex" >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  exit "$test_status"
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  printf '%s\n' 'verify-phase11-fast: skipped-or-unrun-test' >&2
  exit 1
fi
actual_names=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ { print $4 }' \
  "$test_output" | LC_ALL=C sort)
expected_names=$(printf '%s\n' "$expected_names" | LC_ALL=C sort)
if [ "$actual_names" != "$expected_names" ]; then
  printf '%s\n' 'verify-phase11-fast: exact-selection-mismatch' >&2
  printf 'expected:\n%s\nactual:\n%s\n' "$expected_names" "$actual_names" >&2
  exit 1
fi
printf 'verify-phase11-fast: exact-%s passed\n' "$expected_count"
