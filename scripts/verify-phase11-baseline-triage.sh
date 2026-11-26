#!/bin/sh
# Compare the exact Phase 11 canonical failures against its detached start baseline.
set -eu

usage() {
  printf '%s\n' 'usage: scripts/verify-phase11-baseline-triage.sh [--bin DIR] [--baseline-ref SHA] [--report FILE] (--classify|--check|--self-test)' >&2
  exit 2
}

baseline_ref='5f2b0f0dbe6ee84d70998be375f13b44b8c229a9'
bin_dir='build/ci'
report_file='.planning/phases/11-sampled-rows-and-transactional-visibility/11-06-BASELINE-TRIAGE.md'
mode=''
while [ "$#" -gt 0 ]; do
  case $1 in
    --bin) [ "$#" -ge 2 ] || usage; bin_dir=$2; shift 2 ;;
    --baseline-ref) [ "$#" -ge 2 ] || usage; baseline_ref=$2; shift 2 ;;
    --report) [ "$#" -ge 2 ] || usage; report_file=$2; shift 2 ;;
    --classify|--check|--self-test) [ -z "$mode" ] || usage; mode=$1; shift ;;
    *) usage ;;
  esac
done
[ -n "$mode" ] || usage

repo_root=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
case $bin_dir in /*) ;; *) bin_dir="$repo_root/$bin_dir" ;; esac
case $report_file in /*) report_path=$report_file ;; *) report_path="$repo_root/$report_file" ;; esac
case $report_path in "$repo_root"/*) ;; *) printf '%s\n' 'phase11-triage: report-outside-source-root' >&2; exit 1 ;; esac

# The explicit set is the 31 executable selectors reported failed in the
# interrupted canonical log. The 17 not previously compared at baseline are
# the full CPU-instruction and CPU-mutation remainder in this set.
expected_names='bus.timing
bus.irq.registers
bus.keypad.registers
time.halt.refusal
timer.devices
cpu.budget
cpu.arm_block_bounds
cpu.arm_block_edges
cpu.arm_shifts_conditions
cpu.arm_subword_alignment
cpu.thumb_alu
cpu.thumb_bl
cpu.thumb_control
cpu.thumb_high
cpu.thumb_multiple
cpu.thumb_sp
cpu.thumb_stack
cpu.thumb_transfer
cpu.exceptions
cpu.multiply_signed_timing
cpu.slots_bounds
cpu.isa_classification
cpu.power.boundary
bus.nc.video_bytes
bus.nc.clock_overflow
cpu.nc.exception_lr
cpu.pos.clean
cpu.nc.maximum_trace
cpu.nc.bl_halves
core.no_host_includes
diagnostics.execute'
expected_final_failures="$expected_names
hygiene.tree"

fail() { printf 'phase11-triage: %s\n' "$*" >&2; exit 1; }

sha256_file() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    fail 'sha256-tool-unavailable'
  fi
}

sorted_names() { printf '%s\n' "$1" | sed '/^[[:space:]]*$/d' | LC_ALL=C sort; }

assert_cache_root() {
  cache=$1
  expected_root=$2
  [ -f "$cache/CMakeCache.txt" ] || fail "missing-configured-build:$cache"
  configured_root=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache/CMakeCache.txt")
  [ "$configured_root" = "$expected_root" ] || fail "wrong-source-root:$configured_root"
  lane=$(sed -n 's/^GBL_LANE:STRING=//p' "$cache/CMakeCache.txt")
  [ "$lane" = ci ] || fail "unsupported-build-lane:$lane"
}

resolve_baseline() {
  [ "$(git -C "$repo_root" rev-parse "$baseline_ref^{commit}" 2>/dev/null || true)" = "$baseline_ref" ] || fail "baseline-ref-unavailable-or-noncanonical:$baseline_ref"
  entries=$(git -C "$repo_root" worktree list --porcelain) || fail 'worktree-list-failed'
  roots=$(printf '%s\n' "$entries" | awk -v wanted="$baseline_ref" '
    /^worktree / { path = substr($0, 10) }
    /^HEAD / { head = substr($0, 6) }
    /^detached$/ { detached = 1 }
    /^$/ {
      if (head == wanted && detached) print path
      path = ""; head = ""; detached = 0
    }
    END { if (head == wanted && detached) print path }
  ')
  count=$(printf '%s\n' "$roots" | sed '/^[[:space:]]*$/d' | wc -l | tr -d ' ')
  [ "$count" = 1 ] || fail "detached-baseline-worktree-count:$count"
  baseline_root=$(printf '%s\n' "$roots" | sed -n '1p')
  [ "$baseline_root" != "$repo_root" ] || fail 'baseline-is-current-root'
  [ "$(git -C "$baseline_root" rev-parse HEAD 2>/dev/null || true)" = "$baseline_ref" ] || fail 'baseline-worktree-head-mismatch'
}

status_rows() {
  awk '
    $2 == "Test" && $3 ~ /^#[0-9]+:$/ {
      name = $4
      result = "unknown"
      if ($0 ~ /\*\*\*Failed/) result = "failed"
      else if ($0 ~ /\*\*\*Skipped/) result = "skipped"
      else if ($0 ~ /Not Run/) result = "not-run"
      else if ($0 ~ /Passed/) result = "passed"
      print name "\t" result
    }
  ' "$1"
}

selector_status() {
  rows=$1
  selector=$2
  matches=$(printf '%s\n' "$rows" | awk -F '\t' -v wanted="$selector" '$1 == wanted { print $2 }')
  count=$(printf '%s\n' "$matches" | sed '/^[[:space:]]*$/d' | wc -l | tr -d ' ')
  [ "$count" = 1 ] || return 1
  case $matches in passed|failed) printf '%s\n' "$matches" ;; *) return 1 ;; esac
}

validate_run() {
  log_file=$1
  junit_file=$2
  ctest_exit=$3
  rows=$(status_rows "$log_file")
  [ -f "$junit_file" ] || fail "missing-junit:$junit_file"
  duplicate=$(printf '%s\n' "$rows" | awk -F '\t' '{ n[$1]++; if (n[$1] > 1) bad = 1 } END { if (bad) print "duplicate" }')
  [ -z "$duplicate" ] || fail "duplicate-ctest-name:$(basename "$log_file")"
  expected_count=0
  while IFS= read -r selector; do
    [ -n "$selector" ] || continue
    expected_count=$((expected_count + 1))
    selector_status "$rows" "$selector" >/dev/null || fail "missing-duplicate-skipped-or-unrun-selector:$selector:$(basename "$log_file")"
    testcase_count=$(awk -v needle="name=\"$selector\"" 'index($0, needle) { count++ } END { print count + 0 }' "$junit_file")
    [ "$testcase_count" = 1 ] || fail "junit-selector-count:$selector:$testcase_count"
  done <<EOF
$expected_names
EOF
  case_count=$(printf '%s\n' "$rows" | sed '/^[[:space:]]*$/d' | wc -l | tr -d ' ')
  [ "$case_count" -ge "$expected_count" ] || fail "incomplete-ctest-log:$(basename "$log_file")"
  if printf '%s\n' "$rows" | awk -F '\t' '$2 != "passed" && $2 != "failed" { found = 1 } END { exit !found }'; then
    fail "skipped-or-unrun-result:$(basename "$log_file")"
  fi
  if printf '%s\n' "$rows" | awk -F '\t' '$2 == "failed" { found = 1 } END { exit !found }'; then
    [ "$ctest_exit" -ne 0 ] || fail "failure-with-zero-ctest-status:$(basename "$log_file")"
  else
    [ "$ctest_exit" -eq 0 ] || fail "nonzero-ctest-without-failing-case:$(basename "$log_file")"
  fi
  case_count=$(awk 'index($0, "<testcase ") { count++ } END { print count + 0 }' "$junit_file")
  [ "$case_count" -ge "$expected_count" ] || fail "incomplete-junit:$(basename "$junit_file")"
}

validate_original_log() {
  original_log="$repo_root/build/ci/phase11-canonical-ci-final.log"
  [ -f "$original_log" ] || fail 'missing-final-canonical-log'
  actual=$(awk '$2 == "Test" && $3 ~ /^#[0-9]+:$/ && $0 ~ /\*\*\*Failed/ { print $4 }' "$original_log" | LC_ALL=C sort)
  expected=$(sorted_names "$expected_final_failures")
  [ "$actual" = "$expected" ] || fail 'final-log-failure-set-mismatch'
  evidence="$repo_root/.planning/phases/11-sampled-rows-and-transactional-visibility/11-EVIDENCE.md"
  grep -Fq 'runner.relocation_admitted' "$evidence" || fail 'missing-relocation-skip-receipt'
  grep -Fq 'Not Run' "$evidence" || fail 'missing-relocation-not-run-receipt'
}

row_count() {
  awk -F '|' -v wanted="$1" 'function trim(x) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", x); return x } trim($2) == wanted { n++ } END { print n + 0 }' "$2"
}

row_value() {
  awk -F '|' -v wanted="$1" -v col="$2" 'function trim(x) { gsub(/^[[:space:]]+|[[:space:]]+$/, "", x); return x } trim($2) == wanted { print trim($col); n++ } END { if (n != 1) exit 1 }' "$3"
}

validate_report() {
  report=$1
  receipt_dir=$2
  expected_head=$3
  [ -f "$report" ] || fail 'missing-triage-report'
  grep -Fqx 'Current source binding: repository-root' "$report" || fail 'report-current-root-binding-mismatch'
  grep -Fqx "Current source revision at classification: $expected_head" "$report" || fail 'report-original-current-revision-mismatch'
  grep -Fqx "Baseline source revision: $baseline_ref" "$report" || fail 'report-baseline-revision-mismatch'
  for receipt in current.log current.junit.xml current.exit baseline.log baseline.junit.xml baseline.exit roots.txt current.root baseline.root current.bin baseline.bin current.head baseline.head; do
    [ -f "$receipt_dir/$receipt" ] || fail "missing-receipt:$receipt"
  done
  grep -Fqx "Current log SHA256: $(sha256_file "$receipt_dir/current.log")" "$report" || fail 'current-log-digest-mismatch'
  grep -Fqx "Current JUnit SHA256: $(sha256_file "$receipt_dir/current.junit.xml")" "$report" || fail 'current-junit-digest-mismatch'
  grep -Fqx "Baseline log SHA256: $(sha256_file "$receipt_dir/baseline.log")" "$report" || fail 'baseline-log-digest-mismatch'
  grep -Fqx "Baseline JUnit SHA256: $(sha256_file "$receipt_dir/baseline.junit.xml")" "$report" || fail 'baseline-junit-digest-mismatch'
  validate_run "$receipt_dir/current.log" "$receipt_dir/current.junit.xml" "$(cat "$receipt_dir/current.exit")"
  validate_run "$receipt_dir/baseline.log" "$receipt_dir/baseline.junit.xml" "$(cat "$receipt_dir/baseline.exit")"
  current_rows=$(status_rows "$receipt_dir/current.log")
  baseline_rows=$(status_rows "$receipt_dir/baseline.log")
  while IFS= read -r selector; do
    [ -n "$selector" ] || continue
    [ "$(row_count "$selector" "$report")" = 1 ] || fail "missing-or-duplicate-report-row:$selector"
    report_current=$(row_value "$selector" 4 "$report") || fail "missing-current-report-status:$selector"
    report_baseline=$(row_value "$selector" 5 "$report") || fail "missing-baseline-report-status:$selector"
    [ "$report_current" = "$(selector_status "$current_rows" "$selector")" ] || fail "current-status-report-mismatch:$selector"
    [ "$report_baseline" = "$(selector_status "$baseline_rows" "$selector")" ] || fail "baseline-status-report-mismatch:$selector"
  done <<EOF
$expected_names
EOF
  [ "$(row_count hygiene.tree "$report")" = 1 ] || fail 'missing-hygiene-tree-report-row'
  [ "$(row_count runner.relocation_admitted "$report")" = 1 ] || fail 'missing-relocation-report-row'
  grep -Fq '16 historical plan files / 32 path lines; source identity confirmed at baseline' "$report" || fail 'missing-hygiene-source-identity-classification'
  grep -Fq 'not run: diagnostic_execution prerequisite failed before this dependent case' "$report" || fail 'missing-relocation-dependency-classification'
}

live_bindings() {
  current_head=$(git -C "$repo_root" rev-parse HEAD) || fail 'current-head-unavailable'
  resolve_baseline
  assert_cache_root "$bin_dir" "$repo_root"
  baseline_bin="$baseline_root/build/ci"
  assert_cache_root "$baseline_bin" "$baseline_root"
}

write_report() {
  receipt_dir=$1
  temp_report="$report_path.tmp.$$"
  current_head=$(cat "$receipt_dir/current.head")
  current_rows=$(status_rows "$receipt_dir/current.log")
  baseline_rows=$(status_rows "$receipt_dir/baseline.log")
  {
    printf '%s\n' '# Phase 11-06 — Current/Detached-Baseline Triage' ''
    printf '%s\n' 'Classification was captured before any source or test repair. A matching failed exit alone does not establish the same cause.' ''
    printf 'Current source binding: repository-root\n'
    printf 'Current source revision at classification: %s\n' "$current_head"
    printf 'Baseline source revision: %s\n' "$baseline_ref"
    printf 'Final canonical log SHA256: %s\n' "$(sha256_file "$repo_root/build/ci/phase11-canonical-ci-final.log")"
    printf 'Current log SHA256: %s\n' "$(sha256_file "$receipt_dir/current.log")"
    printf 'Current JUnit SHA256: %s\n' "$(sha256_file "$receipt_dir/current.junit.xml")"
    printf 'Baseline log SHA256: %s\n' "$(sha256_file "$receipt_dir/baseline.log")"
    printf 'Baseline JUnit SHA256: %s\n' "$(sha256_file "$receipt_dir/baseline.junit.xml")"
    printf 'Receipt directory: build/ci/phase11-baseline-triage\n\n'
    printf '%s\n' 'Ignored receipts bind the actual roots, CMake source bindings, selected CTest output, and per-case JUnit reports. Each selector appears once.' ''
    printf '%s\n' '| Selector | Original final log | Current at classification | Detached baseline | Initial comparison | Cause / later GREEN receipt |'
    printf '%s\n' '|---|---|---|---|---|---|'
    while IFS= read -r selector; do
      [ -n "$selector" ] || continue
      current_status=$(selector_status "$current_rows" "$selector") || fail "invalid-current-status:$selector"
      baseline_status=$(selector_status "$baseline_rows" "$selector") || fail "invalid-baseline-status:$selector"
      if [ "$current_status" = "$baseline_status" ]; then comparison='same status; cause not inferred from status alone'; else comparison='status differs; requires cause review'; fi
      printf '| %s | failed | %s | %s | %s | pending focused cause review |\n' "$selector" "$current_status" "$baseline_status" "$comparison"
    done <<EOF
$expected_names
EOF
    printf '%s\n' '| hygiene.tree | failed | not run here | baseline source identity | 16 historical plan files / 32 path lines; source identity confirmed at baseline | path cleanup belongs to 11-07 |'
    printf '%s\n' '| runner.relocation_admitted | not run | not run | not run | not run: diagnostic_execution prerequisite failed before this dependent case | original dependency skip retained; no claim of a selector result |' ''
    printf '%s\n' '## Original failure set' ''
    printf '%s\n' 'The 31 executable selectors above were extracted from the interrupted final canonical log and reproduced as a complete exact selection on both revisions. The hygiene.tree row is a source-identity finding, not a baseline CTest run. runner.relocation_admitted remains an explicit dependency skip because diagnostics.execute failed before its fixture completed.' ''
    printf '%s\n' '## Focused repair receipts' ''
    printf '%s\n' 'No source or test repair had been applied when this dual-revision classification was captured. Task 2 and Task 3 append the proven cause and separate GREEN results to affected rows.'
  } > "$temp_report"
  mv "$temp_report" "$report_path"
}

classify() {
  live_bindings
  validate_original_log
  git -C "$repo_root" diff --quiet HEAD -- src tests || fail 'classify-after-source-or-test-change'
  receipt_dir="$repo_root/build/ci/phase11-baseline-triage"
  mkdir -p "$receipt_dir"
  reusable=true
  for receipt in current.log current.junit.xml current.exit baseline.log baseline.junit.xml baseline.exit; do
    [ -f "$receipt_dir/$receipt" ] || reusable=false
  done
  [ "$(cat "$receipt_dir/current.root" 2>/dev/null || true)" = "$repo_root" ] || reusable=false
  [ "$(cat "$receipt_dir/baseline.root" 2>/dev/null || true)" = "$baseline_root" ] || reusable=false
  [ "$(cat "$receipt_dir/current.bin" 2>/dev/null || true)" = "$bin_dir" ] || reusable=false
  [ "$(cat "$receipt_dir/baseline.bin" 2>/dev/null || true)" = "$baseline_root/build/ci" ] || reusable=false
  [ "$(cat "$receipt_dir/current.head" 2>/dev/null || true)" = "$current_head" ] || reusable=false
  [ "$(cat "$receipt_dir/baseline.head" 2>/dev/null || true)" = "$baseline_ref" ] || reusable=false
  if [ "$reusable" != true ]; then
    printf '%s\n' "$repo_root" > "$receipt_dir/current.root"
    printf '%s\n' "$baseline_root" > "$receipt_dir/baseline.root"
    printf '%s\n' "$bin_dir" > "$receipt_dir/current.bin"
    printf '%s\n' "$baseline_root/build/ci" > "$receipt_dir/baseline.bin"
    printf '%s\n' "$baseline_ref" > "$receipt_dir/baseline.ref"
    printf '%s\n' "$current_head" > "$receipt_dir/current.head"
    printf '%s\n' "$baseline_ref" > "$receipt_dir/baseline.head"
    {
      printf 'current_root=%s\n' "$repo_root"
      printf 'current_bin=%s\n' "$bin_dir"
      printf 'current_head=%s\n' "$current_head"
      printf 'baseline_root=%s\n' "$baseline_root"
      printf 'baseline_bin=%s\n' "$baseline_root/build/ci"
      printf 'baseline_ref=%s\n' "$baseline_ref"
    } > "$receipt_dir/roots.txt"
    # Build only the selected executables; up-to-date targets remain untouched.
    cmake --build "$bin_dir" --target cpu_test bus_test timer_test gamebloke-run > "$receipt_dir/current.build.log" 2>&1 || fail 'current-selected-target-build-failed'
    cmake --build "$baseline_root/build/ci" --target cpu_test bus_test timer_test gamebloke-run > "$receipt_dir/baseline.build.log" 2>&1 || fail 'baseline-selected-target-build-failed'
    selector_regex='^(diagnostics[.]execute|bus[.](timing|irq[.]registers|keypad[.]registers|nc[.](video_bytes|clock_overflow))|time[.]halt[.]refusal|timer[.]devices|cpu[.](budget|arm_block_bounds|arm_block_edges|arm_shifts_conditions|arm_subword_alignment|thumb_alu|thumb_bl|thumb_control|thumb_high|thumb_multiple|thumb_sp|thumb_stack|thumb_transfer|exceptions|multiply_signed_timing|slots_bounds|isa_classification|power[.]boundary)|cpu[.](nc[.](exception_lr|maximum_trace|bl_halves)|pos[.]clean)|core[.]no_host_includes)$'
    set +e
    ctest --test-dir "$bin_dir" --output-on-failure --no-tests=error --output-junit "$receipt_dir/current.junit.xml" -R "$selector_regex" > "$receipt_dir/current.log" 2>&1
    current_exit=$?
    set -e
    printf '%s\n' "$current_exit" > "$receipt_dir/current.exit"
    set +e
    ctest --test-dir "$baseline_root/build/ci" --output-on-failure --no-tests=error --output-junit "$receipt_dir/baseline.junit.xml" -R "$selector_regex" > "$receipt_dir/baseline.log" 2>&1
    baseline_exit=$?
    set -e
    printf '%s\n' "$baseline_exit" > "$receipt_dir/baseline.exit"
  else
    current_exit=$(cat "$receipt_dir/current.exit")
    baseline_exit=$(cat "$receipt_dir/baseline.exit")
  fi
  validate_run "$receipt_dir/current.log" "$receipt_dir/current.junit.xml" "$current_exit"
  validate_run "$receipt_dir/baseline.log" "$receipt_dir/baseline.junit.xml" "$baseline_exit"
  write_report "$receipt_dir"
  validate_report "$report_path" "$receipt_dir" "$current_head"
  printf '%s\n' 'phase11-triage: dual-revision classification complete; matching exit status does not imply matching cause'
}

check() {
  live_bindings
  receipt_dir="$repo_root/build/ci/phase11-baseline-triage"
  validate_report "$report_path" "$receipt_dir" "$(cat "$receipt_dir/current.head")"
  [ "$(cat "$receipt_dir/current.root")" = "$repo_root" ] || fail 'current-root-receipt-mismatch'
  [ "$(cat "$receipt_dir/baseline.root")" = "$baseline_root" ] || fail 'baseline-root-receipt-mismatch'
  [ "$(cat "$receipt_dir/current.bin")" = "$bin_dir" ] || fail 'current-build-receipt-mismatch'
  [ "$(cat "$receipt_dir/baseline.bin")" = "$baseline_root/build/ci" ] || fail 'baseline-build-receipt-mismatch'
  [ "$(cat "$receipt_dir/baseline.head")" = "$baseline_ref" ] || fail 'baseline-head-receipt-mismatch'
  printf '%s\n' 'phase11-triage: report rows and dual-revision results are complete'
}

self_test() {
  check
  receipt_dir="$repo_root/build/ci/phase11-baseline-triage"
  scratch=$(mktemp -d /tmp/phase11-triage.XXXXXX) || fail 'scratch-create-failed'
  trap 'rm -rf "$scratch"' 0 HUP INT TERM
  make_copy() {
    label=$1
    mkdir -p "$scratch/$label/receipts"
    cp "$report_path" "$scratch/$label/report.md"
    cp -R "$receipt_dir/." "$scratch/$label/receipts/"
  }
  validate_copy() {
    label=$1
    validate_report "$scratch/$label/report.md" "$scratch/$label/receipts" "$(cat "$receipt_dir/current.head")"
  }
  make_copy clean
  validate_copy clean
  make_copy wrong-baseline
  sed 's/^Baseline source revision: .*/Baseline source revision: 0000000000000000000000000000000000000000/' "$scratch/wrong-baseline/report.md" > "$scratch/wrong-baseline/report.tmp"
  mv "$scratch/wrong-baseline/report.tmp" "$scratch/wrong-baseline/report.md"
  if (validate_copy wrong-baseline) >/dev/null 2>&1; then fail 'self-test-admitted-wrong-baseline'; fi
  make_copy wrong-root
  sed 's/^Current source binding: repository-root$/Current source binding: wrong-root/' "$scratch/wrong-root/report.md" > "$scratch/wrong-root/report.tmp"
  mv "$scratch/wrong-root/report.tmp" "$scratch/wrong-root/report.md"
  if (validate_copy wrong-root) >/dev/null 2>&1; then fail 'self-test-admitted-wrong-root'; fi
  make_copy missing-result
  rm "$scratch/missing-result/receipts/current.junit.xml"
  if (validate_copy missing-result) >/dev/null 2>&1; then fail 'self-test-admitted-missing-result'; fi
  make_copy skipped-case
  first_selector=$(printf '%s\n' "$expected_names" | sed -n '1p')
  awk -v wanted="$first_selector" '$2 == "Test" && $3 ~ /^#[0-9]+:$/ && $4 == wanted { gsub(/Passed|\*\*\*Failed/, "***Skipped") } { print }' "$scratch/skipped-case/receipts/current.log" > "$scratch/skipped-case/receipts/current.log.tmp"
  mv "$scratch/skipped-case/receipts/current.log.tmp" "$scratch/skipped-case/receipts/current.log"
  skipped_sha=$(sha256_file "$scratch/skipped-case/receipts/current.log")
  sed "s/^Current log SHA256: .*/Current log SHA256: $skipped_sha/" "$scratch/skipped-case/report.md" > "$scratch/skipped-case/report.tmp"
  mv "$scratch/skipped-case/report.tmp" "$scratch/skipped-case/report.md"
  if (validate_copy skipped-case) >/dev/null 2>&1; then fail 'self-test-admitted-skipped-case'; fi
  make_copy missing-row
  first_selector=$(printf '%s\n' "$expected_names" | sed -n '1p')
  awk -F '|' -v wanted="$first_selector" '$2 !~ "^[[:space:]]*" wanted "[[:space:]]*$" { print }' "$scratch/missing-row/report.md" > "$scratch/missing-row/report.tmp"
  mv "$scratch/missing-row/report.tmp" "$scratch/missing-row/report.md"
  if (validate_copy missing-row) >/dev/null 2>&1; then fail 'self-test-admitted-missing-report-row'; fi
  printf '%s\n' 'phase11-triage: clean copy accepted; wrong root/baseline, missing result, skipped case, and missing row rejected'
}

case $mode in
  --classify) classify ;;
  --check) check ;;
  --self-test) self_test ;;
esac
