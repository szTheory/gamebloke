#!/bin/sh
# Run Phase 15's cheap selectors before one exact access/production set.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase15-fast.sh [--bin DIR] [--preflight-only]' >&2
  exit 2
}

fail() {
  printf 'verify-phase15-fast: %s\n' "$1" >&2
  exit 1
}

now() { date +%s; }
elapsed() { printf '%s' "$(( $2 - $1 ))"; }

bin_dir='build/ci'
preflight_only=0
while [ "$#" -gt 0 ]; do
  case $1 in
    --bin)
      [ "$#" -ge 2 ] || usage
      bin_dir=$2
      [ -n "$bin_dir" ] || usage
      shift 2
      ;;
    --preflight-only)
      preflight_only=1
      shift
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
if [ ! -d "$bin_dir" ]; then
  fail "build-directory-missing: $bin_dir"
fi
bin_dir=$(CDPATH= cd "$bin_dir" && pwd -P)

total_start=$(now)
preflight_start=$total_start
if [ ! -f "$bin_dir/CMakeCache.txt" ]; then
  fail "missing-configured-build: $bin_dir"
fi
configured_roots=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' \
  "$bin_dir/CMakeCache.txt")
configured_root_count=$(printf '%s\n' "$configured_roots" | \
  awk 'NF { n++ } END { print n+0 }')
if [ "$configured_root_count" -ne 1 ]; then
  fail 'build-source-missing-or-duplicate'
fi
configured_root=$(printf '%s\n' "$configured_roots" | sed -n '1p')
if [ "$configured_root" != "$repo_root" ]; then
  fail "wrong-source-root: $configured_root"
fi
lanes=$(sed -n 's/^GBL_LANE:STRING=//p' "$bin_dir/CMakeCache.txt")
lane_count=$(printf '%s\n' "$lanes" | awk 'NF { n++ } END { print n+0 }')
if [ "$lane_count" -ne 1 ]; then
  fail 'lane-missing-or-duplicate'
fi
lane=$(printf '%s\n' "$lanes" | sed -n '1p')
case $lane in
  ci|asan) ;;
  *) fail "unsupported-lane: $lane" ;;
esac

gate="$repo_root/tests/gates/phase15_selection.cmake"
if [ ! -f "$gate" ]; then
  fail 'selection-gate-missing'
fi
sh "$repo_root/scripts/check-workflow.sh"
manifest=$(cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" \
  "-DBIN=$bin_dir" -DCASE=manifest -P "$gate" 2>&1) || {
  printf '%s\n' "$manifest" >&2
  fail 'manifest-command-failed'
}
printf '%s\n' "$manifest"
expected_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase15-manifest-name=//p' \
  -e 's/^-- phase15-manifest-name=//p')
expected_regex=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase15-manifest-regex=//p' \
  -e 's/^-- phase15-manifest-regex=//p')
expected_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase15-manifest-count=//p' \
  -e 's/^-- phase15-manifest-count=//p')
case $expected_count in ''|*[!0-9]*) fail 'invalid-manifest-count' ;; esac
manifest_name_count=$(printf '%s\n' "$expected_names" | \
  awk 'NF { n++ } END { print n+0 }')
unique_name_count=$(printf '%s\n' "$expected_names" | LC_ALL=C sort -u | \
  awk 'NF { n++ } END { print n+0 }')
if [ -z "$expected_regex" ] || [ "$manifest_name_count" -ne "$expected_count" ] ||
    [ "$unique_name_count" -ne "$expected_count" ]; then
  fail 'invalid-gate-manifest'
fi

run_gate() {
  gate_case=$1
  gate_output=$(cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" \
    "-DBIN=$bin_dir" "-DCASE=$gate_case" -P "$gate" 2>&1) || {
    printf '%s\n' "$gate_output" >&2
    fail "gate-$gate_case-failed"
  }
  printf '%s\n' "$gate_output"
}
run_gate verify
run_gate control
run_gate order
preflight_end=$(now)
printf 'verify-phase15-fast: source=%s lane=%s\n' "$repo_root" "$lane"
printf 'verify-phase15-fast timing: preflight=%ss\n' \
  "$(elapsed "$preflight_start" "$preflight_end")"
if [ "$preflight_only" -eq 1 ]; then
  printf '%s\n' 'verify-phase15-fast: preflight-only passed; build and focused selectors not run'
  exit 0
fi

failed_output=''
keep_failed_output=0
cleanup() {
  if [ -n "$failed_output" ] && [ "$keep_failed_output" -eq 0 ]; then
    rm -f "$failed_output"
  fi
}
trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

build_start=$(now)
if ! cmake --build "$bin_dir" --target runner_test cpu_test dma_test bus_test determinism_test; then
  fail 'incremental-target-build-failed'
fi
build_end=$(now)

failed_output=$(mktemp "$bin_dir/phase15-fast-ctest.XXXXXX")
focused_start=$(now)
set +e
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast|phase10_fast|phase11_fast|phase12_fast|phase13_fast|phase14_fast|phase15_fast)$' \
  -R "$expected_regex" >"$failed_output" 2>&1
focused_status=$?
set -e
cat "$failed_output"
if [ "$focused_status" -ne 0 ]; then
  keep_failed_output=1
  printf 'verify-phase15-fast: preserved failed CTest output at %s\n' \
    "$failed_output" >&2
  fail 'focused-selector-failure'
fi
if grep -E 'Skipped|Not Run' "$failed_output" >/dev/null; then
  keep_failed_output=1
  printf 'verify-phase15-fast: preserved skipped CTest output at %s\n' \
    "$failed_output" >&2
  fail 'skipped-or-unrun-test'
fi
actual_names=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ { print $4 }' \
  "$failed_output" | LC_ALL=C sort)
expected_names_sorted=$(printf '%s\n' "$expected_names" | LC_ALL=C sort)
if [ -z "$actual_names" ] || [ "$actual_names" != "$expected_names_sorted" ]; then
  keep_failed_output=1
  printf 'expected focused names:\n%s\nactual focused names:\n%s\n' \
    "$expected_names_sorted" "$actual_names" >&2
  printf 'verify-phase15-fast: preserved CTest output at %s\n' \
    "$failed_output" >&2
  fail 'exact-selection-mismatch'
fi
focused_end=$(now)
total_end=$(now)
printf 'verify-phase15-fast: exact-%s selectors passed on lane %s\n' \
  "$expected_count" "$lane"
printf 'verify-phase15-fast executed focused names:\n%s\n' "$actual_names"
printf 'verify-phase15-fast timing: preflight=%ss; incremental-build=%ss; focused=%ss; total=%ss\n' \
  "$(elapsed "$preflight_start" "$preflight_end")" \
  "$(elapsed "$build_start" "$build_end")" \
  "$(elapsed "$focused_start" "$focused_end")" \
  "$(elapsed "$total_start" "$total_end")"
