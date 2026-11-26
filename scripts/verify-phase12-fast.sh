#!/bin/sh
# Run Phase 11's same-build predecessor closure and the exact current
# Phase 12 background selectors on one configured source/build pair.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase12-fast.sh [--bin DIR]' >&2
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
if [ -d "$bin_dir" ]; then
  bin_dir=$(CDPATH= cd "$bin_dir" && pwd -P)
fi

# Reject stale trees and unsupported lanes before running any build command.
if [ ! -f "$bin_dir/CMakeCache.txt" ]; then
  printf 'verify-phase12-fast: missing-configured-build: %s\n' "$bin_dir" >&2
  exit 1
fi
configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
if [ "$configured_root" != "$repo_root" ]; then
  printf 'verify-phase12-fast: wrong-source-root: %s\n' "$configured_root" >&2
  exit 1
fi
lane=$(sed -n 's/^GBL_LANE:STRING=//p' "$bin_dir/CMakeCache.txt")
case $lane in
  ci|asan) ;;
  *)
    printf 'verify-phase12-fast: unsupported-lane: %s\n' "$lane" >&2
    exit 1
    ;;
esac

gate="$repo_root/tests/gates/phase12_selection.cmake"
now() { date +%s; }
elapsed() { printf '%s' "$(( $2 - $1 ))"; }

# Gate and profile consistency checks are read-only and deliberately precede
# the inherited build/test closure.
setup_start=$(now)
manifest=$(cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" \
  "-DBIN=$bin_dir" -DCASE=manifest -P "$gate" 2>&1)
expected_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase12-manifest-name=//p' \
  -e 's/^-- phase12-manifest-name=//p')
expected_regex=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase12-manifest-regex=//p' \
  -e 's/^-- phase12-manifest-regex=//p')
expected_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase12-manifest-count=//p' \
  -e 's/^-- phase12-manifest-count=//p')
manifest_name_count=$(printf '%s\n' "$expected_names" | awk 'NF { n++ } END { print n+0 }')
if [ -z "$expected_regex" ] || [ -z "$expected_count" ] ||
    [ "$manifest_name_count" -ne "$expected_count" ]; then
  printf '%s\n' 'verify-phase12-fast: invalid-gate-manifest' >&2
  exit 1
fi

cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=verify -P "$gate"
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=order -P "$gate"
setup_end=$(now)

# Phase 11 delegates through Phase 10, whose one focused bus_test build also
# contains the Phase 12 selectors. Reuse that same-build build instead of
# paying for a duplicate post-closure target invocation.
closure_start=$(now)
closure_output=$(mktemp "$bin_dir/phase12-inherited.XXXXXX")
trap 'rm -f "$closure_output" "${test_output:-}"' 0 HUP INT TERM
set +e
sh "$repo_root/scripts/verify-phase11-fast.sh" --bin "$bin_dir" \
  >"$closure_output" 2>&1
closure_status=$?
set -e
cat "$closure_output"
if [ "$closure_status" -ne 0 ]; then
  exit "$closure_status"
fi
closure_end=$(now)

# This is only a recovery path: the Phase 11 closure normally built bus_test.
build_start=$(now)
if [ ! -x "$bin_dir/bus_test" ] && [ ! -x "$bin_dir/bus_test.exe" ]; then
  cmake --build "$bin_dir" --target bus_test
  build_end=$(now)
else
  build_end=$build_start
fi

test_start=$(now)
test_output=$(mktemp "$bin_dir/phase12-fast.XXXXXX")
set +e
# Phase 14's outer helper executes its display selectors before this inherited
# chain; excluding phase14_fast avoids expanding that later setup a second time.
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast|phase10_fast|phase11_fast|phase14_fast)$' \
  -R "$expected_regex" >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  exit "$test_status"
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  printf '%s\n' 'verify-phase12-fast: skipped-or-unrun-test' >&2
  exit 1
fi
actual_names=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ { print $4 }' \
  "$test_output" | LC_ALL=C sort)
expected_names=$(printf '%s\n' "$expected_names" | LC_ALL=C sort)
if [ "$actual_names" != "$expected_names" ]; then
  printf '%s\n' 'verify-phase12-fast: exact-selection-mismatch' >&2
  printf 'expected:\n%s\nactual:\n%s\n' "$expected_names" "$actual_names" >&2
  exit 1
fi
test_end=$(now)

printf 'verify-phase12-fast: exact-%s passed on lane %s\n' "$expected_count" "$lane"
printf 'verify-phase12-fast timing: setup=%ss; Phase11 closure=%ss; focused-build=%ss; tests=%ss\n' \
  "$(elapsed "$setup_start" "$setup_end")" \
  "$(elapsed "$closure_start" "$closure_end")" \
  "$(elapsed "$build_start" "$build_end")" \
  "$(elapsed "$test_start" "$test_end")"
