#!/bin/sh
# Run Phase 13's exact OBJ/composition selectors before the inherited closure.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase13-fast.sh [--bin DIR]' >&2
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

now() { date +%s; }
elapsed() { printf '%s' "$(( $2 - $1 ))"; }
fail() {
  printf 'verify-phase13-fast: %s\n' "$1" >&2
  exit 1
}

# Reject stale trees and unsupported lanes before running CMake or building.
if [ ! -f "$bin_dir/CMakeCache.txt" ]; then
  fail "missing-configured-build: $bin_dir"
fi
configured_roots=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
[ "$(printf '%s\n' "$configured_roots" | awk 'NF { n++ } END { print n+0 }')" -eq 1 ] ||
  fail 'build-source-missing-or-duplicate'
configured_root=$(printf '%s\n' "$configured_roots" | sed -n '1p')
if [ "$configured_root" != "$repo_root" ]; then
  fail "wrong-source-root: $configured_root"
fi
lanes=$(sed -n 's/^GBL_LANE:STRING=//p' "$bin_dir/CMakeCache.txt")
[ "$(printf '%s\n' "$lanes" | awk 'NF { n++ } END { print n+0 }')" -eq 1 ] ||
  fail 'lane-missing-or-duplicate'
lane=$(printf '%s\n' "$lanes" | sed -n '1p')
case $lane in
  ci|asan) ;;
  *) fail "unsupported-lane: $lane" ;;
esac

gate="$repo_root/tests/gates/phase13_selection.cmake"
preflight_start=$(now)
manifest=$(cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" \
  "-DBIN=$bin_dir" -DCASE=manifest -P "$gate" 2>&1) || {
  printf '%s\n' "$manifest" >&2
  fail 'manifest-command-failed'
}
expected_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase13-manifest-name=//p' \
  -e 's/^-- phase13-manifest-name=//p')
expected_regex=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase13-manifest-regex=//p' \
  -e 's/^-- phase13-manifest-regex=//p')
expected_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase13-manifest-count=//p' \
  -e 's/^-- phase13-manifest-count=//p')
expected_inherited_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase13-manifest-inherited-name=//p' \
  -e 's/^-- phase13-manifest-inherited-name=//p')
expected_inherited_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase13-manifest-inherited-count=//p' \
  -e 's/^-- phase13-manifest-inherited-count=//p')
manifest_name_count=$(printf '%s\n' "$expected_names" | awk 'NF { n++ } END { print n+0 }')
unique_name_count=$(printf '%s\n' "$expected_names" | LC_ALL=C sort -u | \
  awk 'NF { n++ } END { print n+0 }')
inherited_name_count=$(printf '%s\n' "$expected_inherited_names" | \
  awk 'NF { n++ } END { print n+0 }')
unique_inherited_count=$(printf '%s\n' "$expected_inherited_names" | LC_ALL=C sort -u | \
  awk 'NF { n++ } END { print n+0 }')
overlap=$(printf '%s\n%s\n' "$expected_names" "$expected_inherited_names" |
  LC_ALL=C sort | uniq -d | awk 'NF { print }')
case $expected_count in ''|*[!0-9]*) fail 'invalid-gate-manifest-count' ;; esac
case $expected_inherited_count in ''|*[!0-9]*) fail 'invalid-gate-manifest-inherited-count' ;; esac
if [ -z "$expected_regex" ] || [ "$manifest_name_count" -ne "$expected_count" ] ||
    [ "$unique_name_count" -ne "$expected_count" ] ||
    [ "$inherited_name_count" -ne "$expected_inherited_count" ] ||
    [ "$unique_inherited_count" -ne "$expected_inherited_count" ] ||
    [ -n "$overlap" ]; then
  printf '%s\n' "$manifest" >&2
  fail 'invalid-gate-manifest'
fi

# Exact CTest membership, command, inventory and fixture-graph validation is
# read-only and runs before focused builds or inherited verification.
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=verify -P "$gate"
cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" "-DBIN=$bin_dir" \
  -DCASE=order -P "$gate"
preflight_end=$(now)

build_start=$(now)
if { [ ! -x "$bin_dir/bus_test" ] && [ ! -x "$bin_dir/bus_test.exe" ]; } ||
    { [ ! -x "$bin_dir/determinism_test" ] && [ ! -x "$bin_dir/determinism_test.exe" ]; }; then
  cmake --build "$bin_dir" --target bus_test determinism_test
fi
if { [ ! -x "$bin_dir/bus_test" ] && [ ! -x "$bin_dir/bus_test.exe" ]; } ||
    { [ ! -x "$bin_dir/determinism_test" ] && [ ! -x "$bin_dir/determinism_test.exe" ]; }; then
  fail 'focused-test-executable-missing'
fi
build_end=$(now)

test_start=$(now)
test_output=$(mktemp "$bin_dir/phase13-fast.XXXXXX")
closure_output=$(mktemp "$bin_dir/phase13-inherited.XXXXXX")
inherited_names_file=$(mktemp "$bin_dir/phase13-inherited-names.XXXXXX")
printf '%s\n' "$expected_inherited_names" >"$inherited_names_file"
trap 'rm -f "$test_output" "$closure_output" "$inherited_names_file"' 0 HUP INT TERM
set +e
# Phase 14's outer helper runs the later display setup first. Exclude its
# fixture from this nested predecessor selector so it is not expanded twice.
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast|phase10_fast|phase11_fast|phase12_fast|phase14_fast)$' \
  -R "$expected_regex" >"$test_output" 2>&1
test_status=$?
set -e
cat "$test_output"
if [ "$test_status" -ne 0 ]; then
  fail 'phase13-selector-failure'
fi
if grep -E 'Skipped|Not Run' "$test_output" >/dev/null; then
  fail 'skipped-or-unrun-test'
fi
actual_names=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ { print $4 }' \
  "$test_output" | LC_ALL=C sort)
expected_names=$(printf '%s\n' "$expected_names" | LC_ALL=C sort)
if [ -z "$actual_names" ] || [ "$actual_names" != "$expected_names" ]; then
  printf 'expected:\n%s\nactual:\n%s\n' "$expected_names" "$actual_names" >&2
  fail 'exact-selection-mismatch'
fi
test_end=$(now)

# The inherited closure is intentionally last and is invoked exactly once on
# this same configured build after all cheap checks and semantic selectors pass.
closure_start=$(now)
set +e
sh "$repo_root/scripts/verify-phase12-fast.sh" --bin "$bin_dir" \
  >"$closure_output" 2>&1
closure_status=$?
set -e
cat "$closure_output"
if [ "$closure_status" -ne 0 ]; then
  exit "$closure_status"
fi
actual_inherited_names=$(awk '
  NR == FNR { if (NF) wanted[$1] = 1; next }
  $2 == "Test" && $3 ~ /^#[0-9]+:$/ && $4 in wanted { print $4 }
' "$inherited_names_file" "$closure_output" | LC_ALL=C sort)
expected_inherited_names=$(printf '%s\n' "$expected_inherited_names" | LC_ALL=C sort)
if [ "$actual_inherited_names" != "$expected_inherited_names" ]; then
  printf 'expected inherited shared selectors:\n%s\nactual inherited shared selectors:\n%s\n' \
    "$expected_inherited_names" "$actual_inherited_names" >&2
  fail 'inherited-shared-selection-mismatch'
fi
aggregate_actual_names=$(printf '%s\n%s\n' "$actual_names" \
  "$actual_inherited_names" | LC_ALL=C sort)
aggregate_expected_names=$(printf '%s\n%s\n' "$expected_names" \
  "$expected_inherited_names" | LC_ALL=C sort)
if [ "$aggregate_actual_names" != "$aggregate_expected_names" ]; then
  printf 'expected aggregate selectors:\n%s\nactual aggregate selectors:\n%s\n' \
    "$aggregate_expected_names" "$aggregate_actual_names" >&2
  fail 'aggregate-selection-mismatch'
fi
closure_end=$(now)

aggregate_count=$(( expected_count + expected_inherited_count ))
printf 'verify-phase13-fast: exact-%s Phase13 selectors and %s inherited shared selectors passed on lane %s (aggregate %s)\n' \
  "$expected_count" "$expected_inherited_count" "$lane" "$aggregate_count"
printf 'verify-phase13-fast timing: preflight=%ss; focused-build=%ss; Phase13-selectors=%ss; inherited-closure=%ss\n' \
  "$(elapsed "$preflight_start" "$preflight_end")" \
  "$(elapsed "$build_start" "$build_end")" \
  "$(elapsed "$test_start" "$test_end")" \
  "$(elapsed "$closure_start" "$closure_end")"
