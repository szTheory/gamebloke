---
phase: 04-cpu-execution-core-and-headless-runner
plan: "10"
subsystem: runner
tags: [C17, CMake, ARM7TDMI, JSON, SHA-256]

# Dependency graph
requires:
  - phase: 04-09
    provides: Canonical CPU run, trace, mailbox, and SHA-256 interfaces
provides:
  - Bounded fresh-machine CLI execution with exact cycle and frame budgets
  - Metadata-bound gamebloke.result/1 output and diagnostic-only raw execution
  - Bounded JSON Lines trace output with checked result status
affects: [04-11-runner-tests, 04-12-producer-contract, 04-13-install-relocation]

# Actuals measured from the committed implementation diff (chars/4).
actuals:
  tokens: 13954
  tasks: 1
  commits: 1
commits: 1
plan_head_before: 8467607566f88fd57c4b12f37446b8b07c1dfe5b
plan_head_after: 188a741719838028a6ba9348d5c41b5cfa607471

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Fresh machine and bounded copied ROM per invocation
    - Exact byte-digest fixture authority; raw input remains diagnostic
    - One-instruction execution boundaries for cancellation and mailbox observation

key-files:
  created: [runner/run.h, runner/run.c, runner/result.c]
  modified: [runner/main.c, CMakeLists.txt, tests/gates/runner_metadata.cmake]

key-decisions:
  - "Only a digest-matching fixture in the generated immutable index can produce evidence PASS."
  - "A complete mailbox from unregistered raw input remains GUEST_STOP with its semantic observation and diagnostic limits."
  - "All machine time and semantic case identifiers are emitted as decimal strings to preserve uint64 precision."

requirements-completed: []

# Metrics
duration: 31min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 10: Bounded CLI Runner and JSON Producer Summary

**The headless CLI now runs a fresh canonical machine with bounded input and time, and emits a metadata-bound JSON result or an explicitly diagnostic raw-input result.**

## Performance

- **Duration:** 31 min
- **Started:** 2026-11-11T08:31:09Z
- **Completed:** 2026-11-11T09:02:35Z
- **Tasks:** 1
- **Files modified:** 6

## Accomplishments

- Added `--rom`, `--cycles` / `--frames`, optional fixture selection, bounded trace selection, and trace output. Invalid or duplicate options fail before execution; valid non-PASS runs return exit code 1, while usage errors return 2.
- Each invocation reads at most 32 MiB plus a sentinel byte, hashes the exact loaded span, checks copied machine bytes, and loads a fresh machine. The runner checks cancellation and the neutral mailbox after each single CPU unit and uses checked cycle accounting.
- Added one `gamebloke.result/1` JSON object per run. It separates evidence status, semantic mailbox observation, actual and expected digest, exact machine time strings, claim metadata, disputes, and limitations. Optional trace output is bounded JSON Lines and reports output failures without claiming completion.
- A registered digest plus a complete mailbox can produce PASS in the synthetic control target. Production raw input still executes for diagnosis but cannot PASS; it reports `unregistered-content`, the observed semantics and time, and empty evidence.

## Scope and Limits

The canonical production fixture index remains empty. The generated synthetic control manifest is only a local validator input and grants no rights. No private ROM, BIOS, or binary fixture was committed or run. This result uses profile-version-4 direct-start project policy, a partial ARM7TDMI instruction subset, placeholder timing, and no BIOS, timers, DMA, active display, audio, physical input, or host presentation. It establishes neither hardware accuracy, broad game compatibility, nor content rights. Existing fixture dispute identifiers and the `disputed` label are preserved; matching SHA-256 values establish byte identity only.

## Verification

- **RED:** The first direct CLI behavioral assertion failed against the prior help/version-only executable: a real guest invocation was rejected with usage status 2. `gsd_run check tdd-red-evidence` accepted the captured failure as `RED_EVIDENCE_OK target_test_failed`.
- **GREEN:** Five generated focused CLI assertions passed after implementation, covering registered control execution, raw non-PASS output, checked frame/uint64 parsing, fixture digest mismatch, trace output/full behavior, and unknown fixture/path handling. These generated scripts and outputs remain under ignored `build/` and are not committed.
- `cmake --preset ci && cmake --build --preset ci && build/ci/gamebloke-run-control --rom build/ci/generated/fixtures/owned-arm-pass.gba --fixture synthetic-control-pass --cycles 1000` passed after the task commit. The emitted JSON had status `PASS`, the loaded SHA-256, semantic case ID `36865`, and exact cycle/instruction/budget strings.
- The production runner on those same unregistered bytes emitted one JSON result with `GUEST_STOP` / `unregistered-content`, retained the guest mailbox observation, emitted no evidence claim, and returned exit code 1. `cmake/CheckResult.cmake` accepted the control JSON as `PASS` against the generated control manifest.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang` and the ASan/UBSan build and CLI runs passed. A trace capacity of one produced the expected `HARNESS_FAIL` / `trace-output-full` result with `trace.complete=false`; no sanitizer report occurred. LeakSanitizer was unavailable in this macOS runtime, so the successful sanitizer runs used `ASAN_OPTIONS=halt_on_error=1`.
- The final `cmake --workflow --preset ci` run passed the full local workflow: 322 checks passed and one owner-local signature check was skipped because its optional local file was unset; the inventory postcheck reported 323 expected, 322 run, and one allowed skip. Measured CTest time was 74.94 seconds.

## Task Commits

1. **Task 1: Run one generated image through the real CLI and JSON writer** - `188a741` (`feat`)

**Plan metadata:** captured in a separate summary commit after this file is written; shared planning state is updated by the parent executor.

## Files Created/Modified

- `runner/run.h` - Internal runner options, result model, limits, and cancellation seam.
- `runner/run.c` - Bounded file ownership, metadata validation, fresh machine execution, mailbox/cancellation observation, and bounded trace handling.
- `runner/result.c` - Escaped JSON result and bounded JSON Lines trace writer.
- `runner/main.c` - Checked CLI parser, retained help/version behavior, and documented result exits.
- `CMakeLists.txt` - Builds both runner targets from the same producer/core sources with only the generated metadata index differing.
- `tests/gates/runner_metadata.cmake` - Updated the runner source-list expectation to include the new producer files.

## Decisions Made

Only the immutable generated fixture index can authorize an evidence PASS. A raw run that reaches a complete guest mailbox stays diagnostic and reports its semantic result without claiming fixture evidence. Runtime results contain no host paths. Frame budgets are converted with checked arithmetic, and all uint64 values are serialized as decimal strings.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Compatibility bug] Preserved the existing help exit-code text while documenting run-result exits**
- **Found during:** Task 1 verification
- **Issue:** The pre-existing help gate expected the established `exit codes: 0 success, 2 usage error` text.
- **Fix:** Retained that exact line and documented valid non-PASS run exit code 1 separately.
- **Files modified:** `runner/main.c`
- **Verification:** Help gate and final full workflow passed.
- **Committed in:** `188a741`

**2. [Rule 3 - Blocking gate expectation] Updated the metadata gate for the expanded producer source list**
- **Found during:** Task 1 full workflow
- **Issue:** The existing metadata gate still expected only `runner/main.c` and `runner/sha256.c`, so it rejected the planned shared run and result sources.
- **Fix:** Updated the gate expectation to match the canonical four-source list used by both runner targets.
- **Files modified:** `tests/gates/runner_metadata.cmake`
- **Verification:** Metadata gate and final full workflow passed.
- **Committed in:** `188a741`

**Total deviations:** 2 auto-fixed (1 compatibility bug, 1 blocking gate expectation). **Impact on plan:** Both fixes were required to preserve the existing CLI contract and keep the planned producer source registration verifiable.

## Issues Encountered

The first full workflow run exposed the two expectations documented above; both were corrected and the final workflow passed. The optional LeakSanitizer mode is unsupported by the available macOS runtime, so ASan/UBSan verification ran with the supported runtime options.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 04-11 can add the runner unit/refusal/cancellation/trace test matrix and CTest registration against this producer. Plan 04-12 can harden the producer contract, and Plan 04-13 can validate installation and relocation. The production index is empty, so this slice does not create a real-fixture evidence PASS.

## Self-Check: PASSED

- Task commit `188a741` exists and is an ancestor of `HEAD`.
- All six implementation files listed above exist.
- Task commit measurement is one commit from the persisted plan base; the recorded implementation diff is 55,814 characters (13,954 tokens, rounded from chars/4).

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Completed: 2026-11-11*
