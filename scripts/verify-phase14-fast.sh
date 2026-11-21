#!/bin/sh
# Run the exact Phase 14 display-effects selectors before one inherited closure.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase14-fast.sh [--bin DIR]' >&2
  exit 2
}

fail() {
  printf 'verify-phase14-fast: %s\n' "$1" >&2
  exit 1
}

now() { date +%s; }
elapsed() { printf '%s' "$(( $2 - $1 ))"; }

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

gate="$repo_root/tests/gates/phase14_selection.cmake"
if [ ! -f "$gate" ]; then
  fail 'selection-gate-missing'
fi
manifest=$(cmake -E chdir "$bin_dir" cmake "-DSRC=$repo_root" \
  "-DBIN=$bin_dir" -DCASE=manifest -P "$gate" 2>&1) || {
  printf '%s\n' "$manifest" >&2
  fail 'manifest-command-failed'
}
printf '%s\n' "$manifest"
expected_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase14-manifest-name=//p' \
  -e 's/^-- phase14-manifest-name=//p')
expected_regex=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase14-manifest-regex=//p' \
  -e 's/^-- phase14-manifest-regex=//p')
expected_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase14-manifest-count=//p' \
  -e 's/^-- phase14-manifest-count=//p')
expected_inherited_names=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase14-manifest-inherited-name=//p' \
  -e 's/^-- phase14-manifest-inherited-name=//p')
expected_inherited_count=$(printf '%s\n' "$manifest" | sed -n \
  -e 's/^phase14-manifest-inherited-count=//p' \
  -e 's/^-- phase14-manifest-inherited-count=//p')
case $expected_count in ''|*[!0-9]*) fail 'invalid-manifest-count' ;; esac
case $expected_inherited_count in ''|*[!0-9]*) fail 'invalid-inherited-count' ;; esac
manifest_name_count=$(printf '%s\n' "$expected_names" | \
  awk 'NF { n++ } END { print n+0 }')
unique_name_count=$(printf '%s\n' "$expected_names" | LC_ALL=C sort -u | \
  awk 'NF { n++ } END { print n+0 }')
inherited_name_count=$(printf '%s\n' "$expected_inherited_names" | \
  awk 'NF { n++ } END { print n+0 }')
unique_inherited_count=$(printf '%s\n' "$expected_inherited_names" | LC_ALL=C sort -u | \
  awk 'NF { n++ } END { print n+0 }')
overlap=$(printf '%s\n%s\n' "$expected_names" "$expected_inherited_names" | \
  LC_ALL=C sort | uniq -d | awk 'NF { print }')
if [ -z "$expected_regex" ] || [ "$manifest_name_count" -ne "$expected_count" ] ||
    [ "$unique_name_count" -ne "$expected_count" ] ||
    [ "$inherited_name_count" -ne "$expected_inherited_count" ] ||
    [ "$unique_inherited_count" -ne "$expected_inherited_count" ] ||
    [ -n "$overlap" ]; then
  fail 'invalid-gate-manifest'
fi
if [ "$lane" = asan ] && [ "$expected_inherited_count" -ne 0 ]; then
  fail 'asan-inherited-profile-manifest'
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
run_gate order
preflight_end=$(now)

build_start=$(now)
set --
if [ ! -x "$bin_dir/bus_test" ] && [ ! -x "$bin_dir/bus_test.exe" ]; then
  set -- "$@" bus_test
fi
if [ ! -x "$bin_dir/determinism_test" ] &&
    [ ! -x "$bin_dir/determinism_test.exe" ]; then
  set -- "$@" determinism_test
fi
if [ "$#" -gt 0 ]; then
  cmake --build "$bin_dir" --target "$@"
fi
if { [ ! -x "$bin_dir/bus_test" ] && [ ! -x "$bin_dir/bus_test.exe" ]; } ||
    { [ ! -x "$bin_dir/determinism_test" ] &&
      [ ! -x "$bin_dir/determinism_test.exe" ]; }; then
  fail 'focused-test-executable-missing'
fi
build_end=$(now)

focused_output=''
closure_output=''
focused_names_file=''
inherited_names_file=''
cleanup() {
  [ -z "$focused_output" ] || rm -f "$focused_output"
  [ -z "$closure_output" ] || rm -f "$closure_output"
  [ -z "$focused_names_file" ] || rm -f "$focused_names_file"
  [ -z "$inherited_names_file" ] || rm -f "$inherited_names_file"
}
trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
focused_output=$(mktemp "$bin_dir/phase14-focused.XXXXXX")
closure_output=$(mktemp "$bin_dir/phase14-inherited.XXXXXX")
focused_names_file=$(mktemp "$bin_dir/phase14-names.XXXXXX")
inherited_names_file=$(mktemp "$bin_dir/phase14-inherited-names.XXXXXX")
printf '%s\n' "$expected_names" >"$focused_names_file"
printf '%s\n' "$expected_inherited_names" >"$inherited_names_file"

focused_start=$(now)
set +e
ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast|phase9_fast|phase10_fast|phase11_fast|phase12_fast|phase13_fast|phase14_fast)$' \
  -R "$expected_regex" >"$focused_output" 2>&1
focused_status=$?
set -e
cat "$focused_output"
if [ "$focused_status" -ne 0 ]; then
  fail 'phase14-selector-failure'
fi
if grep -E 'Skipped|Not Run' "$focused_output" >/dev/null; then
  fail 'skipped-or-unrun-test'
fi
actual_names=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ { print $4 }' \
  "$focused_output" | LC_ALL=C sort)
expected_names_sorted=$(printf '%s\n' "$expected_names" | LC_ALL=C sort)
if [ -z "$actual_names" ] || [ "$actual_names" != "$expected_names_sorted" ]; then
  printf 'expected focused names:\n%s\nactual focused names:\n%s\n' \
    "$expected_names_sorted" "$actual_names" >&2
  fail 'exact-selection-mismatch'
fi
focused_end=$(now)
printf 'verify-phase14-fast: exact-%s selectors passed on lane %s\n' \
  "$expected_count" "$lane"
printf 'verify-phase14-fast executed focused names:\n%s\n' "$actual_names"

inherited_start=$(now)
set +e
sh "$repo_root/scripts/verify-phase13-fast.sh" --bin "$bin_dir" \
  >"$closure_output" 2>&1
inherited_status=$?
set -e
cat "$closure_output"
if [ "$inherited_status" -ne 0 ]; then
  fail 'inherited-closure-failed'
fi
actual_phase14_inherited=$(awk '
  NR == FNR { if (NF) focused[$1] = 1; next }
  $2 == "Test" && $3 ~ /^#[0-9]+:$/ && $4 in focused { print $4 }
' "$focused_names_file" "$closure_output" | LC_ALL=C sort -u)
if [ -n "$actual_phase14_inherited" ]; then
  printf 'unexpected Phase 14 executions in inherited output:\n%s\n' \
    "$actual_phase14_inherited" >&2
  fail 'inherited-phase14-reexpanded'
fi
actual_inherited_names=$(awk '
  NR == FNR { if (NF) wanted[$1] = 1; next }
  $2 == "Test" && $3 ~ /^#[0-9]+:$/ && $4 in wanted { print $4 }
' "$inherited_names_file" "$closure_output" | LC_ALL=C sort)
expected_inherited_sorted=$(printf '%s\n' "$expected_inherited_names" | LC_ALL=C sort)
if [ "$actual_inherited_names" != "$expected_inherited_sorted" ]; then
  printf 'expected inherited names:\n%s\nactual inherited names:\n%s\n' \
    "$expected_inherited_sorted" "$actual_inherited_names" >&2
  fail 'inherited-profile-selection-mismatch'
fi
inherited_end=$(now)
printf 'verify-phase14-fast executed inherited profile names:\n%s\n' \
  "$actual_inherited_names"

total_end=$(now)
aggregate_count=$(( expected_count + expected_inherited_count ))
printf 'verify-phase14-fast: exact-%s Phase14 selectors and %s inherited profile selectors passed (aggregate %s)\n' \
  "$expected_count" "$expected_inherited_count" "$aggregate_count"
printf 'verify-phase14-fast timing: preflight=%ss; focused-build=%ss; focused=%ss; inherited-closure=%ss; total=%ss\n' \
  "$(elapsed "$preflight_start" "$preflight_end")" \
  "$(elapsed "$build_start" "$build_end")" \
  "$(elapsed "$focused_start" "$focused_end")" \
  "$(elapsed "$inherited_start" "$inherited_end")" \
  "$(elapsed "$total_start" "$total_end")"
