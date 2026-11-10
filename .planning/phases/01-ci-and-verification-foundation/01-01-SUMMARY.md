---
phase: 01-ci-and-verification-foundation
plan: 01
subsystem: infra
tags: [cmake, ctest, presets, junit, inventory, c17, posix-sh]

requires: []
provides:
  - "One command, `cmake --workflow --preset ci`: configure, Release warnings-as-errors build, CTest with junit, verbose post step that checks the junit against the sorted inventory"
  - "gbl_add_test() registration helper with configure-time inventory compare (cmake_language DEFER)"
  - "scripts/check-junit.sh tracer subset with the stable summary line `inventory: expected N, ran M, skipped K (allowed J)`"
  - "gamebloke_core static library (gbl_version only) and gamebloke-run (--version, --help, exit codes 0 and 2)"
affects: [01-02, 01-03, 01-04, 01-05, 01-06, 01-07, 01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 3500
  tasks: 2
  commits: 2
plan_head_before: 315650a991850fd2b37620c57ed4549af8b16fde
plan_head_after: 837977015c6d99e9faf10c6d0e679c18011a3e50

tech-stack:
  added: []
  patterns:
    - "Tests register from top-level directory scope via include(), never add_subdirectory()"
    - "Inventory line format: `<name> <label,label>[ skip-ok]`, single spaces, byte-sorted, `#` comments"
    - "Stable reason lines: `inventory: <code>[: <name>]`, `gbl_add_test: <code>: <name>`"
    - "Post step is a label-`post` CTest test run by a verbose second test preset so its output reaches green logs"

key-files:
  created:
    - CMakeLists.txt
    - CMakePresets.json
    - include/gamebloke/gamebloke.h
    - src/version.c
    - runner/main.c
    - cmake/GblTests.cmake
    - cmake/InventoryCompare.cmake
    - scripts/check-junit.sh
    - tests/core.cmake
    - tests/inventory/ci.txt
  modified: []

key-decisions:
  - "ci-post test preset uses output.verbosity verbose (not outputOnFailure) so the inventory summary and SKIPPED lines appear in a passing run's log (B1)"
  - "Inventory compare is whole-line (name, labels, skip-ok), so a label or skip-ok change is a configure-mismatch"
  - "gbl_add_test accepts LABELS as separate or comma-joined arguments and normalizes them"
  - "InventoryCompare.cmake script mode only runs when it is the -P script (CMAKE_SCRIPT_MODE_FILE equals its own path), so include() from other scripts stays inert"

patterns-established:
  - "One verify command; CI must call it verbatim"
  - "Every gate prints a reason code a negative control can match on"

requirements-completed: [VERIF-01, VERIF-03]

duration: 3 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 01: Tracer Workflow and Inventory Spine Summary

**`cmake --workflow --preset ci` builds the C17 skeleton, runs CTest with junit, and a verbose post step proves every inventoried test ran (`inventory: expected 1, ran 1, skipped 0 (allowed 0)`), with configure-time refusal of malformed registrations and unsorted or mismatched inventories.**

## Performance

- **Duration:** 3 min
- **Started:** 2026-11-10T00:40:34Z
- **Completed:** 2026-11-10T00:43:17Z (before the SUMMARY and state commits)
- **Tasks:** 2
- **Files modified:** 10 created

## Accomplishments

- Tracer: one command from a clean `build/ci` goes green on macOS arm64 (AppleClang 21, CMake 4.4.3, Ninja 1.13.2) and prints the inventory verdict in the log.
- Registration spine: `gbl_add_test()` rejects `bad-name` and `no-labels`; the deferred compare fails configure with `inventory: unsorted` or `inventory: configure-mismatch`; a stale `build/ci/junit.xml` is deleted at configure time.
- Skeleton: `gamebloke_core` (static, only `gbl_version()`, no `#include` lines in the public header) and `gamebloke-run` with documented exit codes (0 success, 2 usage error).
- VERIF-01's `lints the manifest` clause has a named Phase 2 extension point: a `manifest.lint` test in `tests/gates.cmake` (created by plan 03), recorded in the CMakeLists.txt extension-point comment.

## Task Commits

1. **Task 1: Tracer (leading vertical slice)** - `f5d2b83` (feat)
2. **Task 2: Registration refusals, unsorted inventories, runner exit codes** - `8379770` (feat)

**Plan metadata:** committed separately with this SUMMARY.

## Verification Evidence

- Plan verification: `rm -rf build/ci && cmake --workflow --preset ci` exits 0; log contains `inventory: expected 1, ran 1, skipped 0 (allowed 0)`; `ctest --test-dir build/ci -N` lists 2 tests (`inventory.junit`, `runner.version`). PASS.
- Task 1 acceptance:
  - junit contains `<testcase name="runner.version" ... status="run">`: PASS.
  - Removing the `runner.version` line from `tests/inventory/ci.txt` makes `cmake --preset ci` fail with `inventory: configure-mismatch` (and `listed but not registered: runner.version runner,unit`); line restored: PASS.
  - Mutation `ci-post` output `{"outputOnFailure": true}`: **observed workflow exit 0 and zero `inventory: expected` lines in the log**, so the tracer verify fails as the plan predicts; `{"verbosity": "verbose"}` restored and the green log shows the line again. This confirms the B1 deviation on CMake 4.4.3 (the floor-version confirmation remains plan 09's job).
  - `cmake --list-presets=all` parses; no key starts with `$` (the only `"$` matches are `${sourceDir}` values): PASS.
- Task 2 acceptance:
  - `gamebloke-run --help` exits 0 with `exit codes: 0 success, 2 usage error`; `--bogus` and no arguments exit 2 with `usage: gamebloke-run [--version | --help]` on stderr: PASS.
  - Script mode `cmake -DREGISTERED_FILE=... -DINVENTORY=... -P cmake/InventoryCompare.cmake` with an out-of-order inventory exits non-zero with `inventory: unsorted`; a matching sorted pair exits 0; a skip-ok difference exits non-zero with `configure-mismatch`: PASS.
  - Temporarily appending `gbl_add_test(NAME Bad.Name LABELS unit COMMAND true)` fails configure with `gbl_add_test: bad-name: Bad.Name`; a call without LABELS fails with `no-labels`; a name containing a space fails with `bad-name`; `tests/core.cmake` restored: PASS.
- check-junit.sh direct shapes: nonexistent junit prints `inventory: no-junit: <path>`, `inventory: missing: runner.version`, the summary line, exit 1; a junit with a failed `runner.version` and an unlisted test prints `inventory: failed:` and `inventory: extra:`, exit 1; wrong argument count prints `check-junit: usage` and exits 2.

## Deviations from Plan

### Process notes (no code deviation)

**1. Pre-commit protected-branch assertion on `main`**
- **Found during:** Task 1 commit
- **Issue:** `gsd_run query git.base-branch --is-protected main` returns `true`, which the executor commit guard treats as a halt condition. The orchestrator pinned this run to `main` (`.planning/config.json` has `git.branching_strategy: none`; every prior phase commit is on `main`), so HEAD never drifted onto it.
- **Action:** Committed locally on `main` as instructed; changed no config, pushed nothing. Flagged here for the orchestrator. The phase research (Pitfall 8) already plans to move GSD off direct-to-main commits once the ruleset lands.

**2. TDD red phase for Task 2 by direct invocation**
- Task 2 is `tdd="true"` but its `<files>` list holds no test file and the plan states plan 02 pins these behaviors with registered tests. RED was demonstrated before implementation by direct invocation (`--help` exited 2 without the exit-code line; script mode returned 0 on an unsorted inventory; malformed names registered). No separate RED commit exists because there is no committed test artifact in this plan's file budget.

**Total deviations:** 0 auto-fixed code deviations; 2 process notes. **Impact:** none on deliverables.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model (T-01-03, T-01-34 mitigated as planned).

## Issues Encountered

- First configure failed because `set_tests_properties` was given a flattened list; fixed within Task 1 by setting each property in its own call (the labels list stays quoted).
- CMake wraps long `message(FATAL_ERROR)` text, so `inventory: unsorted:` may be followed by the path on the next line. The reason prefix is intact; plan 02 controls should match `inventory: unsorted`, not the full line.

## Next Phase Readiness

Ready for 01-02: the junit contract completion, `ExpectFail.cmake`, and the registered controls (including `inventory.nc.add_test_bad_name` and `inventory.nc.add_test_no_labels`) can build directly on `gbl_add_test`, `gbl_inventory_compare`, and `scripts/check-junit.sh`. `tests/gates.cmake` and `tests/inventory/{asan,hygiene}.txt` do not exist yet (plans 03 and 05).

## Self-Check: PASSED

- All ten created files exist on disk.
- Commits `f5d2b83` and `8379770` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 2 commits.
