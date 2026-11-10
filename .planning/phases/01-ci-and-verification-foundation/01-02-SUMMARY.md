---
phase: 01-ci-and-verification-foundation
plan: 02
subsystem: infra
tags: [cmake, ctest, junit, inventory, negative-controls, posix-sh, awk, c17]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "gbl_add_test, InventoryCompare.cmake, tracer scripts/check-junit.sh, gamebloke_core and gamebloke-run (plan 01)"
provides:
  - "scripts/check-junit.sh full contract: missing, extra, duplicate, failed, disabled, skipped-not-allowed, skipped-no-reason, empty-junit, no-junit, with SKIPPED <name>: <mechanism> | <reason> lines"
  - "cmake/ExpectFail.cmake: gbl_expect_fail / gbl_expect_pass and a -DCMD/-DARGS/-DREGEX script mode reused by every later control"
  - "16 inventory controls, positives, and the wrapper meta-control, driven by junit shapes synthesized at test time"
  - "core and runner contract: tests/check.h, core.version, runner.help/usage_error/no_args, core.no_host_includes with its planted-include control"
affects: [01-03, 01-04, 01-05, 01-06, 01-07, 01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 6000
  tasks: 2
  commits: 2
plan_head_before: faf65c484290ab0af47fa6baa04b211a69c31047
plan_head_after: 0ad967177a3f5e81ad61b3165f67ceaf4b9b16dc

tech-stack:
  added: []
  patterns:
    - "Negative control = gbl_expect_fail on the gate: non-zero exit AND a stable reason-code regex, never an exact sanitizer exit code"
    - "Every control has a positive companion or a real-tree companion, and the wrapper itself has a meta-control"
    - "Junit shapes live in bracket strings next to the case that uses them, written under build/ci/nc/<case>/ at test time"
    - "Script-mode entries are inert under include() (CMAKE_SCRIPT_MODE_FILE equals CMAKE_CURRENT_LIST_FILE)"
    - "Multi-argument script-mode calls pass the program as -DCMD= and extra arguments as -DARGS=a|b"

key-files:
  created:
    - cmake/ExpectFail.cmake
    - cmake/CheckCoreIncludes.cmake
    - tests/harness.cmake
    - tests/harness_case.cmake
    - tests/check.h
    - tests/version_test.c
  modified:
    - scripts/check-junit.sh
    - tests/core.cmake
    - tests/inventory/ci.txt
    - CMakeLists.txt

key-decisions:
  - "gbl_expect_pass takes an optional REGEX and every wrapper prints the gate output on success, so a permitted skip's reason line is visible in ctest -V and in the verbose post log"
  - "check-junit.sh buffers the junit and splits on <testcase in awk, so system-out text with semicolons or any content cannot corrupt parsing; findings print in inventory order, then testcase order"
  - "A notrun test that is not skip-ok, or a skip-ok test with no SKIPPED: line, fails; a skip-ok test that runs and passes counts as ran"
  - "CheckCoreIncludes.cmake prefixes each source line with x before list splitting so empty lines keep their position and the reported line number is exact"

patterns-established:
  - "Controls match on the reason prefix (for example inventory: unsorted), since CMake wraps long FATAL_ERROR text"
  - "Sandboxes live under build/ci/nc/<name> and are rebuilt from scratch on every run"

requirements-completed: [VERIF-02, VERIF-03, VERIF-01]

duration: 5 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 02: Inventory Contract, Negative-Control Wrapper, and Core Contract Summary

**The ci workflow now fails on every way a test can silently not run (missing, extra, duplicate, failed, disabled, skipped without permission or reason, empty or absent junit), each shape proven by a control that must exit non-zero with its stated reason code, plus a meta-control that proves the wrapper fails when a gate passes; the core and runner contract is pinned by 24 tests (`inventory: expected 23, ran 23, skipped 0 (allowed 0)`).**

## Performance

- **Duration:** 5 min
- **Started:** ~2026-11-10T00:44Z
- **Completed:** 2026-11-10T00:49Z (before the SUMMARY and state commits)
- **Tasks:** 2
- **Files:** 6 created, 4 modified

## Accomplishments

- `scripts/check-junit.sh` reports every finding before exiting 1 and lists each permitted skip as `SKIPPED <name>: <mechanism> | <reason>`; usage errors exit 2.
- `cmake/ExpectFail.cmake` makes controls trustworthy: it fails on exit 0 (`gate PASSED but must fail`), on a wrong documented exit (`wrong exit`), and on a wrong reason (`failed for the wrong reason`); script mode uses `-DCMD`, `-DARGS=a|b`, `-DREGEX`, optional `-DEXIT`.
- 13 `inventory.nc.*` controls, `inventory.pos.pass`, `inventory.pos.skip_reported`, and `nc.meta.passing_gate_fails_wrapper` run on junit shapes written at test time in the exact CTest shapes; the two registration refusals (`gbl_add_test: bad-name`, `no-labels`) are controlled by configuring a language-free sandbox project (about 0.05 s each).
- `tests/check.h` (37 lines), `core.version`, `runner.help`, `runner.usage_error` and `runner.no_args` (exactly exit 2), and `core.no_host_includes` with `core.nc.host_include` that plants a stdio include and requires `core: host-include`.

## Task Commits

1. **Task 1: Inventory gate catches every silent-omission shape through one negative-control wrapper** - `6e36c61` (feat)
2. **Task 2: Core and runner contract pinned by unit tests and a no-host-includes gate** - `0ad9671` (feat)

**Plan metadata:** committed separately with this SUMMARY.

## Verification Evidence

- Plan verification: `rm -rf build/ci && cmake --workflow --preset ci` exits 0; log contains `inventory: expected 23, ran 23, skipped 0 (allowed 0)`; `ctest --test-dir build/ci -N` lists 24 tests. PASS.
- Task 1 automated verify: configure and build clean; `ctest -R '^(inventory\.(nc|pos)|nc\.meta)\.'` passes 16 of 16; `-N` reports `Total Tests: 16`. PASS.
- Task 1 acceptance:
  - `inventory.pos.skip_reported -V` shows `SKIPPED syn.gamma: SKIP_REGULAR_EXPRESSION_MATCHED | owner file not set; skipping` (ctest prefixes each line with its test number): PASS.
  - `inventory.nc.disabled -V` shows `inventory: disabled: syn.beta` and `expect-fail: ok: /inventory: disabled: syn.beta/ (exit 1)`: PASS.
  - `sh scripts/check-junit.sh onlyone` exits 2 with `check-junit: usage`: PASS.
  - `add_test_bad_name` and `add_test_no_labels` show `gbl_add_test: bad-name: Bad.Name`, `gbl_add_test: no-labels: ok.name`, and `expect-fail: ok:` twice: PASS.
  - `git ls-files tests/fixtures/junit` prints nothing: PASS.
  - Mutation: making the `missing` case write the pass shape fails `inventory.nc.missing` with `expect-fail: gate PASSED but must fail`; reverted and the test passes again: PASS.
  - Real CTest shapes were captured from a scratch project (skip via `SKIP_REGULAR_EXPRESSION`, `DISABLED`, failure) and the synthesized shapes match them.
- Task 2 acceptance:
  - `ctest -R '^(core|runner)\.'` runs 7 tests, all pass (core.version, core.no_host_includes, core.nc.host_include, runner.version, runner.help, runner.usage_error, runner.no_args): PASS.
  - `wc -l < tests/check.h` is 37 (range 25 to 60): PASS.
  - `grep -c '#include' include/gamebloke/gamebloke.h` is 0: PASS.
  - `cmake -DDIRS=src -P cmake/CheckCoreIncludes.cmake` exits 0 and prints `core: ok`; a temporary `src/zz.c` with `#include <stdlib.h>` made it exit 1 with `core: host-include: <path>:1: <stdlib.h>` (file removed): PASS.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Controls print gate output on success; gbl_expect_pass takes an optional REGEX**
- **Found during:** Task 1
- **Issue:** The acceptance check for `inventory.pos.skip_reported` needs the `SKIPPED ...` line in `ctest -V` output, but the plan specified wrapper output only on failure. Without a REGEX on the positive control, the positive could also pass without the reported skip reason.
- **Fix:** Both wrappers print the combined gate output on success; `gbl_expect_pass` accepts an optional `REGEX` that the output must match. `inventory.pos.pass` pins the summary line and `inventory.pos.skip_reported` pins the `SKIPPED ... | owner file not set` line.
- **Files modified:** cmake/ExpectFail.cmake, tests/harness_case.cmake
- **Commit:** 6e36c61

**2. [Rule 1 - Bug] awk `ncases` could be -1 for a zero-length junit**
- **Found during:** Task 1 (direct run on an empty file before the controls existed)
- **Issue:** `split()` returns 0 for empty input, so `ncases - 1` was -1 and `empty-junit` was not reported for a zero-byte junit.
- **Fix:** Clamp to 0. A zero-byte junit now reports `inventory: empty-junit`.
- **Files modified:** scripts/check-junit.sh
- **Commit:** 6e36c61

### Process notes (no code deviation)

**3. TDD by direct invocation, no separate RED commit.** Both tasks are `tdd="true"` but have no separate test artifact outside the registered controls, which are part of the same task commit. The behaviors were exercised against the real scripts before commit (direct runs of check-junit.sh on empty and absent junit, the mutation of the `missing` case, a planted `src/zz.c`), matching the approach recorded for plan 01.

**4. Commit guard on `main`.** `main` is a protected branch per the guard; the orchestrator pinned this plan to local `main` (`branching_strategy: none`), so commits were made there as instructed. Nothing was pushed and no config changed.

**Total deviations:** 2 auto-fixed (1 Rule 2, 1 Rule 1), 2 process notes. **Impact:** none on scope; both fixes strengthen the controls.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model (T-01-01 and T-01-02 mitigated as planned; no registry packages installed).

## Issues Encountered

- The ctest `-V` log prefixes each output line with the test number (`17: SKIPPED ...`); the underlying script line starts with `SKIPPED `.
- `core.nc.host_include` output shows the CMake-wrapped reason (`core: host-include:` then the path on the next line); the controls match the prefix, as plan 01 recommended.

## Next Phase Readiness

Ready for 01-03: `cmake/ExpectFail.cmake` (`gbl_expect_fail`, `gbl_expect_pass`, script mode with `-DCMD`/`-DARGS`/`-DREGEX`/`-DEXIT`) is the reusable control wrapper for hygiene and signature gates. `tests/gates.cmake` and `tests/inventory/{asan,hygiene}.txt` do not exist yet (plans 03 and 05). The core and runner tests also run in the `asan` lane once plan 05 adds it.

## Self-Check: PASSED

- All ten files listed under key-files exist on disk.
- Commits `6e36c61` and `0ad9671` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 2 commits.
