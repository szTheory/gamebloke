---
phase: 04-cpu-execution-core-and-headless-runner
plan: "11"
subsystem: testing
tags: [C17, CMake, POSIX, headless-runner, JSONL, ASAN]

# Dependency graph
requires:
  - phase: 04-10
    provides: Canonical bounded runner, fixture metadata, and mailbox result interfaces
provides:
  - Actual CLI coverage for bounded statuses, refusal, cancellation, trace, and precedence
  - Host fault-injection and CPU exception to production JSON classification coverage
  - Canonical CI and ASAN runner test registrations
affects: [04-12, 04-13, headless-runner, CI, ASAN]

# Measured at summary creation, before the documentation commit.
actuals:
  tokens: 17114
  tasks: 2
  commits: 6
plan_head_before: 93a1f50082577377f8e956efc157b451ef84b885
plan_head_after: f3eb587d4c4775f584db20bdbb5fa2f3a85fe7b8

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Signal handlers set only a volatile sig_atomic_t flag; the bounded runner observes cancellation at unit boundaries.
    - Synthetic control metadata and images remain build-only and separate from the empty production manifest.
    - CPU stop classification and JSON formatting use the same production path in execution and host tests.

key-files:
  created:
    - tests/gates/runner_case.cmake
  modified:
    - runner/main.c
    - runner/run.c
    - runner/run.h
    - tests/runner_test.c
    - CMakeLists.txt
    - cmake/GenerateFixtureIndex.cmake
    - tests/gates/runner_metadata.cmake
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - Do not add CLI flags, guest triggers, or modeled interrupt/abort sources to reach external exceptions absent from the current direct-start profile.
  - Exercise all eight CPU exception stop mappings through the existing exception API and the same production classifier and JSON writer.
  - Keep synthetic pass, fail, and BIOS-required cases in a generated control index; do not add rows or rights grants to the production manifest.

patterns-established:
  - Cancellation remains signal-safe and is consumed by the existing bounded execution loop.
  - Trace JSONL is written only to an explicit path, with trace failures retaining HARNESS_FAIL precedence.

requirements-completed: [] # Parent holds cross-plan roll-up for independent phase verification.
requirements-addressed: [RUN-01, RUN-02, RUN-03, PROF-03]
coverage:
  - id: D1
    description: Bounded CLI outcomes preserve status, semantic result, machine time, claim, and limits.
    requirement: RUN-01
    verification:
      - kind: integration
        ref: tests/gates/runner_case.cmake; cmake --workflow --preset ci
        status: pass
    human_judgment: false
  - id: D2
    description: Registered mailbox outcomes and unregistered raw results keep their distinct classifications.
    requirement: RUN-02
    verification:
      - kind: integration
        ref: tests/gates/runner_case.cmake; runner.cases
        status: pass
    human_judgment: false
  - id: D3
    description: Trace capacity, output-full, deterministic JSONL, and write failure behavior are bounded and explicit.
    requirement: RUN-03
    verification:
      - kind: unit
        ref: tests/runner_test.c; tests/gates/runner_case.cmake
        status: pass
    human_judgment: false
  - id: D4
    description: Every CPU exception stop maps to GUEST_STOP with its stable production JSON reason.
    requirement: PROF-03
    verification:
      - kind: unit
        ref: tests/runner_test.c#all CPU exception stops share the production GUEST_STOP JSON mapping
        status: pass
    human_judgment: false

# Metrics
duration: "28min from first task commit through final verification"
completed: 2026-11-11
status: complete
---

# Phase 04 Plan 11: Bounded runner failure matrix Summary

**The headless runner now has a real CLI failure matrix, bounded signal cancellation, explicit trace failure precedence, and CI/ASAN coverage.**

## Performance

- **Duration:** 28 minutes from first task commit through final verification; initial context loading preceded this measured window.
- **Started:** 2026-11-11T09:31:15Z (first plan commit)
- **Completed:** 2026-11-11T09:59:38Z
- **Tasks:** 2
- **Files modified:** 11 implementation and test files; this summary is a separate documentation file.

## Accomplishments

- Added a real CLI matrix covering PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, CANCELLED, MISSING, SKIPPED, REFUSED, and TIMEOUT with their protocol exit distinctions. It checks zero, exact, and one-short budgets; checked uint64 decimal/frame conversion; raw unregistered mailboxes; missing/read/refusal/digest/partial inputs; unknown selectors; and JSON path separation.
- Added signal-safe SIGINT handling. The handler only sets a `volatile sig_atomic_t` flag; the existing bounded runner callback observes it at CPU-unit boundaries. Trace capacity is refused above 65,536 records, selected JSONL goes only to an explicit trace path, and incomplete/full or failed trace output remains HARNESS_FAIL even after a guest PASS.
- Added host fault-injection for allocator, read, trace open/write/close, cancellation, and cleanup behavior. The host suite also sends each of the eight existing CPU exception kinds through `gbl_cpu_enter_exception`, then checks the production stop classifier and JSON writer.
- Registered `runner.tracer`, `runner.unit`, and `runner.cases` in the CI and ASAN inventories. Generated synthetic control pass/fail/BIOS-required metadata stays in a separate build-only index; the production fixture manifest remains empty.

## TDD Gate Compliance

- **RED:** Commit `44b28a5` added the actual CLI SIGINT assertion. Its target test failed because the old process terminated without writing cancellation JSON; the other 21 assertions passed. `gsd_run check tdd-red-evidence` accepted the saved report as `RED_EVIDENCE_OK target_test_failed`.
- **GREEN:** Commit `0350520` added the signal-safe cancellation flag and connected it to the canonical bounded runner path. The targeted SIGINT assertion then passed.
- **Refactor and extension:** Later commits expanded real CLI status/trace coverage, hardened the signal subprocess timing, and registered the suites. No fake RED was created for CPU exception classification because those mappings already existed; the new test exercises their actual lower-level API and shared production JSON boundary.

## Task Commits

1. **Task 1 RED:** `44b28a5` — add failing cancellation and runner host assertions.
2. **Task 1 GREEN:** `0350520` — handle bounded SIGINT cancellation.
3. **Task 1:** `eb870ae` — cover bounded real CLI outcomes and trace behavior.
4. **Task 1:** `88a007c` — cover trace write/close failure and cancellation precedence.
5. **Task 1 validation adjustment:** `f3eb587` — exercise all CPU exception stops through the existing API and production JSON classifier.
6. **Task 2:** `a98ddbb` — register runner suites in canonical CI and ASAN inventories.

## Files Created/Modified

- `runner/main.c` — installs POSIX SIGINT handling that sets only a signal-safe flag.
- `runner/run.c`, `runner/run.h` — share the production guest-stop classifier with host tests.
- `tests/runner_test.c` — host bounds, allocation/read/write faults, cancellation, cleanup, JSON, and all-eight exception mapping assertions.
- `tests/gates/runner_case.cmake` — invokes the real CLI for status, exit, input, budget, trace, and selector cases.
- `CMakeLists.txt` — links host tests to canonical runner modules and generates local-only control metadata.
- `cmake/GenerateFixtureIndex.cmake`, `tests/gates/runner_metadata.cmake` — keep synthetic control rows isolated and check production-index authority boundaries.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — register and inventory the runner suites.

## Verification

- `cmake --build --preset ci --target runner_test gamebloke-run-control gamebloke-run && build/ci/runner_test` — 23/23 TAP assertions passed twice after the final host-test changes.
- `cmake -DSRC=. -DBIN=build/ci -P tests/gates/runner_case.cmake` — actual CLI matrix passed, including unknown selector rejection.
- `cmake -DSRC=. -DBIN=build/ci -DHASH_TEST=build/ci/hash_test -P tests/gates/runner_metadata.cmake` — wrong index, malformed manifest, stale digest, and wrong hash controls were rejected as expected; production index remained empty.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang`, followed by `cmake --build --preset asan --target runner_test gamebloke-run-control gamebloke-run` and the registered runner subset — 9/9 tests passed, including host allocator faults and actual CLI cases.
- Final `cmake --workflow --preset ci` — all 326 registered CI tests passed; the existing owner-local signature test was the one allowed skip because `GBL_OWNER_LOGO_FILE` was unset. The post-workflow inventory check passed. Measured CTest time was 75.61 seconds.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing test control cases] Added isolated synthetic control metadata for real CLI guest-fail and BIOS-skip outcomes.**
- **Found during:** Task 1
- **Issue:** The real CLI matrix needed a registered guest-failure image and a BIOS-required metadata row to exercise GUEST_FAIL and SKIPPED, while the authoritative production manifest must remain empty.
- **Fix:** Extended only the generated build-tree control index with synthetic pass, fail, and BIOS-required cases, and added metadata gate assertions that reject them from the production index. These rows grant no redistribution or execution rights outside the synthetic control build.
- **Files modified:** `CMakeLists.txt`, `cmake/GenerateFixtureIndex.cmake`, `tests/gates/runner_metadata.cmake`
- **Verification:** The metadata gate and final CI workflow passed.
- **Committed in:** `44b28a5`

**2. [Validation-route adjustment] Verified five external no-BIOS stop mappings at the existing CPU API boundary.**
- **Found during:** Task 1 verification
- **Issue:** The direct-start profile has no external reset, interrupt, or abort source. The CLI can reach SWI, undefined, and coprocessor stops, but cannot honestly generate reset, prefetch-abort, data-abort, IRQ, or FIQ events from an owned guest image.
- **Fix:** Added a narrow internal seam and tested all eight real `gbl_cpu_enter_exception` observations through the same production runner classifier and JSON writer used by normal execution. No synthetic CLI flag, guest trigger, interrupt source, or abort model was added.
- **Files modified:** `runner/run.c`, `runner/run.h`, `tests/runner_test.c`
- **Verification:** The host exception-to-JSON matrix passed under CI and ASAN.
- **Committed in:** `f3eb587`

**3. [Rule 1 - Test reliability] Increased the bounded SIGINT test startup window after one intermittent early-signal failure.**
- **Found during:** Task 1 final host verification
- **Issue:** One initial post-change host run failed the SIGINT subprocess assertion; an immediate rerun passed. The test's 100 ms startup delay could signal before the child had installed its handler.
- **Fix:** Raised the bounded startup delay to 250 ms; retained the 3-second watchdog.
- **Files modified:** `tests/runner_test.c`
- **Verification:** Two consecutive host-suite runs, the focused ASAN suite, and the final CI workflow passed.
- **Committed in:** `f3eb587`

**Total deviations:** 3. All remained within bounded runner behavior and test validation; no new dependency, rights grant, or CPU event source was introduced.

## Issues Encountered

The final CLI gate covers only the three no-BIOS exception sources reachable from the current direct-start instruction path. The other five names remain validated at the real CPU API and production serialization boundary. This is a reachability limit of the current profile, not a claim that those events can be triggered by current guest execution.

## User Setup Required

None. The one owner-local signature test remains an allowed skip unless `GBL_OWNER_LOGO_FILE` is set; it is unrelated to runner behavior.

## Next Phase Readiness

The CLI, host-test, CI, and ASAN evidence is ready for the follow-up evidence and verification documentation plans. The generated control images are test-only; this work grants no rights, broad compatibility, physical hardware qualification, or BIOS-backed behavior.

## Self-Check: PASSED

- Summary file exists at the planned path.
- All six measured plan commits are ancestors of the current plan head.
- Recorded `plan_head_before`, `plan_head_after`, task count, commit count, and diff-size-based token actuals match the persisted plan ledger and current history.
