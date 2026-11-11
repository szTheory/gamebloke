---
phase: 04-cpu-execution-core-and-headless-runner
plan: "02"
subsystem: cpu
tags: [c17, cmake, ctest, lifecycle, cycle-bounds]
requires:
  - phase: 04-01
    provides: [per-instance CPU state, bounded bus-backed ARM execution]
  - phase: 03-bus-api-memory-map-and-rom-loading
    provides: [canonical bus, copied-ROM loader, machine lifecycle]
provides:
  - Canonical CPU source in both configured cores and bus/loader scratch builds.
  - Registered CPU tracer, lifecycle and budget test selectors in ci and asan lanes.
  - Field-wise CPU lifecycle, refusal, instance-isolation and exact-budget evidence.
affects: [phase-04-cpu-execution-core-and-headless-runner, CPU-03, RUN-01]
actuals:
  tokens: 4643
  tasks: 2
  commits: 3
commits: 3
plan_head_before: 7ac31f797c2b683cb100beee305c90dabe6ef582
plan_head_after: a5112f5323cb9eb428ea0233289685524334d3dc
tech-stack:
  added: []
  patterns:
    - Both configured core variants and scratch controls use the same canonical source set.
    - Lifecycle tests seed every persistent CPU field before reset, load and refusal checks.
key-files:
  created: []
  modified:
    - CMakeLists.txt
    - tests/core.cmake
    - tests/gates/core_mutation.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - tests/cpu_test.c
    - tests/state_check.h
    - src/cpu.c
    - include/gamebloke/gamebloke.h
key-decisions:
  - "An initialized machine without a loaded ROM is an invalid CPU run context; use the existing INVALID_ARGUMENT status and preserve machine/result state."
patterns-established:
  - "Shared core source list: production and synthetic cores compile the same CPU implementation."
  - "Scratch source parity: bus and loader controls copy and compile the canonical CPU source."
requirements-completed: []
requirements-partial: [CPU-03, RUN-01]
coverage:
  - id: D1
    description: Both configured cores and clean bus/loader scratch controls compile the canonical CPU source.
    requirement: CPU-03
    verification:
      - kind: integration
        ref: "cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(core\\.version|cpu\\.tracer|bus\\.pos\\.clean|loader\\.pos\\.clean)$' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
  - id: D2
    description: CPU reset, ROM replacement/refusal, no-ROM handling, canonical fields and hard cycle caps have observable witnesses.
    requirement: RUN-01
    verification:
      - kind: unit
        ref: "tests/cpu_test.c#lifecycle"
        status: pass
      - kind: unit
        ref: "tests/cpu_test.c#budget"
        status: pass
      - kind: integration
        ref: "cmake --build --preset ci && ctest --test-dir build/ci -R '^(cpu\\.(lifecycle|budget)|machine\\.state|loader\\.(transaction|diagnostics))$' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
duration: not_measured
completed: "2026-11-10"
status: complete
---

# Phase 04 Plan 02: Canonical CPU Build and Lifecycle Evidence Summary

**Both configured cores and scratch controls now compile the same CPU source, with field-wise lifecycle and hard-budget refusal evidence.**

## Performance

- **Duration:** Not measured; the executor start timestamp was not captured.
- **Completed:** 2026-11-10 (America/New_York)
- **Tasks:** 2
- **Files modified:** 9 implementation/test files.
- **Full ci workflow:** 105.91 s wall clock; CTest took 105.44 s. The workflow configured once, found the build up to date, ran the suite and inventory post-check. No redundant full configure/build sequence was measured, so no build optimization was justified.

## Accomplishments

- Added `src/cpu.c` to the shared source list used by production and synthetic-logo cores. Registered `cpu.tracer`, `cpu.lifecycle` and `cpu.budget` once in the ci/asan lane definition and updated both sorted inventories.
- Added the CPU source to both bus and loader scratch copy/compile paths. Their clean controls passed with the configured compiler and warning policy.
- Seeded every CPU bank, SPSR, pipeline slot, sequential-fetch flag, retired counter and stop field before reset, replacement, invalid-run, allocation-failure and refused-load checks. Equality and digest perturbation checks detect each CPU field while allocator identity remains separately compared.
- Added explicit zero, one-cycle, exact-fit, one-short, `UINT64_MAX`-adjacent clock and instruction-counter cases, plus independent-instance isolation.
- Made a run without a loaded ROM return the existing `GBL_RUN_STATUS_INVALID_ARGUMENT` before any guest bus, time or CPU effects. Machine and output state remain unchanged.

## Task Commits

1. **Task 1: Register canonical CPU sources and selectors** — `5172c3d` (`feat(04-02)`).
2. **Task 2 RED: Add lifecycle and missing-ROM witnesses** — `c87d6bf` (`test(04-02)`).
3. **Task 2 GREEN: Refuse CPU execution without a loaded ROM** — `a5112f5` (`feat(04-02)`).

## Verification

- Task 1 command ran twice for the tracer feedback gate: `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(core\.version|cpu\.tracer|bus\.pos\.clean|loader\.pos\.clean)$' --no-tests=error --output-on-failure` — both runs passed 4/4.
- Task 2 command: `cmake --build --preset ci && ctest --test-dir build/ci -R '^(cpu\.(lifecycle|budget)|machine\.state|loader\.(transaction|diagnostics))$' --no-tests=error --output-on-failure` — passed 5/5.
- Canonical workflow: `cmake --workflow --preset ci` — passed. Inventory expected 299 entries, 298 ran and passed, and the existing `signature.owner_local` check was visibly skipped because `GBL_OWNER_LOGO_FILE` was unset; the inventory post-check accepted its declared skip.
- TDD RED: `build/ci/cpu_test lifecycle` failed only the new missing-ROM assertion (8/9 TAP assertions passed). `gsd-tools check tdd-red-evidence build/phase04-02-red.json --raw` returned `RED_EVIDENCE_OK`; the actual report and semantic assessment are retained at `build/phase04-02-red.json` (ignored build output).
- TDD GREEN: the Task 2 focused CTest command above passed after the early no-ROM refusal was added.
- `git diff --check` passed. The changed files contain no TODO/FIXME or UI placeholder stubs; the existing header phrase “placeholder master cycle” describes the documented timing policy.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Refuse CPU execution without a loaded ROM**
- **Found during:** Task 2
- **Issue:** An initialized machine without ROM data could consume refill cycles from ROM-tail/open-bus reads before stopping on instruction zero.
- **Fix:** Require a non-null ROM and nonzero ROM length in the run precondition; return the existing invalid-argument status before effects and document the unchanged-state/output postcondition.
- **Files modified:** `src/cpu.c`, `include/gamebloke/gamebloke.h`
- **Verification:** The new assertion failed before the guard, then `cpu.lifecycle` and the focused lifecycle/budget/machine/loader suite passed.
- **Committed in:** `a5112f5`.

**2. [Rule 3 - Blocking] Keep the shared direct-start assertion warning-clean**
- **Found during:** Task 2 GREEN build
- **Issue:** Clang’s warnings-as-errors rejected the new helper as unused in existing bus and loader test translation units.
- **Fix:** Marked the header helper `static inline`; the existing test sources remain unchanged.
- **Files modified:** `tests/state_check.h`
- **Verification:** Configured ci build and focused tests passed.
- **Committed in:** `a5112f5`.

**Total deviations:** 2 auto-fixed (one missing precondition, one strict-warning blocker). Both were required for the requested witnesses and configured build.

## Decisions Made

No new dependency, machine profile or hardware claim was introduced. Missing-ROM refusal uses the existing status and remains a local API precondition.

## Scope and Limits

The witnesses use project-authored in-memory instruction words. No BIOS or private ROM was executed. This plan proves configured source parity, the bounded ARM slice and refusal/lifecycle behavior; it does not establish full ARM/Thumb coverage, hardware timing accuracy or broad game compatibility. CPU-03 and RUN-01 remain partial for later Phase 4 plans.

## Next Phase Readiness

Plan 04-03 can expand the same CPU source with arithmetic and Thumb behavior. No user action is needed for this plan; phase state and requirement bookkeeping remain with the parent executor.

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Plan: 02*

## Self-Check: PASSED

- This summary exists at the planned path.
- Task commits `5172c3d`, `c87d6bf` and `a5112f5` are ancestors of the current branch head.
- Both task acceptance criteria and the plan's focused verification commands passed; the canonical ci workflow and inventory post-check passed.
