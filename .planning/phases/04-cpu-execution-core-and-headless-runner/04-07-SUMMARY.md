---
phase: 04-cpu-execution-core-and-headless-runner
plan: "07"
subsystem: CPU/trace
tags: [C17, CPU, trace, CMake, CTest, ASan, UBSan]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: "Canonical decoded CPU units, bus-backed execution, and no-BIOS exception stops from plan 04-06."
provides:
  - "Caller-owned bounded instruction, bus, and exception traces with selected-category output."
  - "Whole-unit trace capacity and sequence preflight with explicit trace-output-full termination."
affects: [headless runner, CPU tests, Phase 4 runner integration]

# Actuals (#2632): chars/4 over the realized implementation/test diff.
actuals:
  tokens: 17689
  tasks: 2
  commits: 3
commits: 3
plan_head_before: 87ac17fd67de7c82c8b07a5e6731d051219dedee
plan_head_after: f14aebdfecf77e32aef6b9fddb394fa51a340de6

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Trace records are caller-owned observations; no trace pointer or new persistent field enters canonical machine state."
    - "Capacity and sequence are preflighted for the complete decoded unit before its effects."

key-files:
  created:
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-07-SUMMARY.md
  modified:
    - include/gamebloke/gamebloke.h
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/gates/core_mutation.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Mask zero ignores trace storage; nonzero masks validate caller spans and aliases before execution."
  - "Initial refill bus records use opcode zero because no instruction is executing yet."
  - "Bus records report the actual bus result value; the current write-result contract reports zero for writes."
metrics:
  duration: "about 45 minutes; exact plan start time was not captured"
  completed: 2026-11-11
status: complete
requirements-completed: [] # Evidence is partial; parent owns requirement roll-up.
---

# Phase 4 Plan 7: Bounded CPU Trace Summary

**The canonical CPU path now emits selected guest-time trace records and refuses a whole unit explicitly when its caller-owned trace output cannot fit.**

## Performance

- **Duration:** About 45 minutes; the plan start time was not captured across the execution continuation.
- **Tasks:** 2/2.
- **Implementation and test files:** 7 modified files; no new persistent machine field, dependency, or state-comparison field was needed.
- **Canonical CI critical path:** 75.67 seconds for 321 tests; 320 passed and the permitted `signature.owner_local` test skipped. Configure took about 0.1 seconds and the build had no work. The post-run `inventory.junit` check passed with 321 expected, 320 run, and one allowed skip.

## Accomplishments

- Added `gbl_trace_record`, caller-owned `gbl_trace_buffer`, category masks, and `GBL_RUN_TRACE_OUTPUT_FULL` / `GBL_RUN_REASON_TRACE_OUTPUT_FULL`. Records are initialized deterministically and identify instruction start cycles, bus completion cycles, and exception completion cycles.
- Added checked mask, count, extent, pointer-wrap, and alias validation. For each decoded refill, instruction, or exception unit, the runner counts selected events and checks the complete record capacity and sequence space before execution. Refusal leaves that unit's machine state and prior trace records unchanged. A zero mask ignores the trace buffer and follows the same canonical machine path as untraced execution.
- Recorded actual bus results from the existing CPU bus calls. Current bus writes return a result value of zero, which the trace reports as-is. Initial refill fetches use opcode zero and their actual refill addresses.
- Added behavioral checks for all category masks, exact and one-short instruction/data/refill capacity, initial refill, sequential and branch/BX fetches, real LDR values, BIOS-none exception ordering without a vector fetch, sequence exhaustion, invalid masks/spans/aliases, untraced state parity, and trace exhaustion after a completed mailbox PASS.
- Added `cpu.nc.trace_loss` and `cpu.nc.budget_overshoot` semantic mutants, registered the selectors in CI and ASan inventories, and retained the shared clean-source companion coverage.

## Verification

- **TDD RED:** `build/ci/cpu_test trace` failed the two new named exact-capacity/one-short assertions against the missing trace behavior. `gsd_run check tdd-red-evidence build/phase04-tracer/04-07-red.json --raw` returned `RED_EVIDENCE_OK`.
- **TDD GREEN and tracer feedback:** After `96b1547`, `cmake --build --preset ci && build/ci/cpu_test trace` passed both task-1 trace assertions.
- `cmake --build --preset ci && build/ci/cpu_test trace && build/ci/cpu_test trace_capacity && ctest --test-dir build/ci -R '^cpu\.(trace|trace_capacity|nc\.(trace_loss|budget_overshoot))$' --no-tests=error --output-on-failure` — passed; 29 trace assertions, 4 capacity assertions, and all 4 focused CTest entries passed. Both mutants were caught by their named behavioral assertions.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --test-dir build/asan -R '^cpu\.(trace|trace_capacity|nc\.(trace_loss|budget_overshoot))$' --no-tests=error --output-on-failure` — 4/4 passed with AppleClang 21 ASan/UBSan.
- `cmake --workflow --preset ci` — all 321 tests accounted for; 320 passed and the existing allowed `signature.owner_local` test skipped because `GBL_OWNER_LOGO_FILE` was unset. The `ci-post` inventory check passed.
- Filtered inventory ordering checks and `git diff --check` passed.

## Task Commits

1. **Task 1 RED:** `934c3bf` — failing bounded trace witnesses.
2. **Task 1 GREEN:** `96b1547` — bounded CPU trace output.
3. **Task 2:** `f14aebd` — trace boundary behavior, semantic gates, and inventory registration.

The summary metadata commit is separate from the three measured task commits.

## Supported Scope and Limits

The trace API observes the existing canonical interpreter and bus. It does not introduce a second execution engine, instruction memoization, mid-unit preemption, BIOS/HLE, a new IRQ controller, private ROM execution, fixture rights, or hardware qualification. Existing bus timing remains the project placeholder policy; these results establish deterministic trace behavior under that policy, not hardware timing accuracy. The plan assumption that `RUN-03` lacks a classified edge-probe taxonomy remains flagged and unresolved.

## Known Stubs

- `include/gamebloke/gamebloke.h:399` retains the existing placeholder cost of one master cycle per bus access. It predates this plan and is already recorded as open in `.planning/WINDOWS.md`; this plan did not change timing policy or add a second ledger entry.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Keep the budget mutant compilable under warnings-as-errors**
- **Found during:** Task 2 mutation-gate verification.
- **Issue:** The first mutant replacement stopped reading `completed_units`, so its scratch build failed before reaching the semantic assertion.
- **Fix:** Changed the mutation to bypass the unit limit only when `completed_units == UINT64_MAX`, preserving a reference to the counter while still exposing budget overshoot to the real one-unit witness.
- **Files modified:** `tests/gates/core_mutation.cmake`.
- **Verification:** `cpu.nc.budget_overshoot` compiled and failed the intended mutant behavior in focused CI and ASan runs; `cpu.pos.clean` coverage passed in the canonical workflow.
- **Commit:** `f14aebd`.

## Outstanding Planning Flags

- `RUN-03` remains unresolved as a requirement taxonomy question; this plan adds concrete trace evidence but does not claim to resolve the probe classification.
- No hardware timing, accuracy, broad compatibility, rights, or release claim is made.

## Self-Check: PASSED

- The summary and all seven modified implementation/test files exist.
- Task commits `934c3bf`, `96b1547`, and `f14aebd` are ancestors of `HEAD`.
- Focused CI, focused ASan/UBSan, canonical CI, and the CI inventory post-check passed as recorded above.
