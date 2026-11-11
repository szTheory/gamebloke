---
phase: 04-cpu-execution-core-and-headless-runner
fixed_at: 2026-11-11T12:24:06Z
review_path: .planning/phases/04-cpu-execution-core-and-headless-runner/04-REVIEW.md
iteration: 1
findings_in_scope: 4
fixed: 4
skipped: 0
status: all_fixed
---

# Phase 04: Code Review Fix Report

**Fixed at:** 2026-11-11T12:24:06Z
**Source review:** `.planning/phases/04-cpu-execution-core-and-headless-runner/04-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 4
- Fixed: 4
- Skipped: 0

## Fixed Issues

### CR-01: A completed PASS mailbox can use case ID zero

**Files modified:** `src/inspect.c`, `tests/cpu_test.c`, `tests/fixtures/instruction_image.c`, `CMakeLists.txt`, `cmake/GenerateFixtureIndex.cmake`, `tests/gates/runner_case.cmake`, `tests/gates/runner_metadata.cmake`
**Commit:** `70e501f`
**Status:** fixed: requires human verification
**Applied fix:** Reject completed PASS and FAIL mailboxes with case ID zero. Added a parser regression for both states and a generated zero-ID PASS fixture with an independently verified digest and a registered row in the build-only control index. The actual control runner executes it and reports a malformed mailbox and timeout, not PASS; the existing result-validator negative control remains separate. The synthetic fixture metadata is artificial local test data and grants no redistribution rights.

### CR-02: Thumb immediate word loads do not rotate unaligned data

**Files modified:** `src/cpu.c`, `tests/cpu_test.c`
**Commit:** `4da36a0`
**Status:** fixed: requires human verification
**Applied fix:** Enable address-lane rotation for Thumb immediate word loads only. Added lane 1, 2, and 3 assertions, including an effective address formed from base plus offset, and checked that unaligned word stores and existing byte transfers retain their current behavior.

### CR-03: Conditional ARM SWI and coprocessor instructions ignore their condition

**Files modified:** `src/cpu.c`, `tests/cpu_test.c`
**Commit:** `4efebc4`
**Status:** fixed: requires human verification
**Applied fix:** Apply the ARM condition check before scheduling SWI or coprocessor exceptions. Tests verify that failed conditions retire one instruction and advance sequentially without changing exception mode, SVC LR/SPSR, stop state, or the last-exception record.

### WR-01: Verification guide omits the Phase 4 validator reason codes

**Files modified:** `docs/verify.md`
**Commit:** `5762d5b`
**Status:** fixed
**Applied fix:** Added the four CPU-policy profile reason codes and eight Phase 4 result reason codes to the verification guide.

## Verification

Verification ran in the main checkout on the existing branch because `.planning/config.json` sets `workflow.use_worktrees` to `false`. Build outputs were written under `build/ci`.

- Built `cpu_test`, `gamebloke-run-control`, `runner_test`, and `gamebloke-run` successfully after the mailbox and generated-fixture changes.
- `ctest --test-dir build/ci -R '^(cpu\.mailbox|runner\.(result_contract|metadata))$' --no-tests=error --output-on-failure`: all 3 tests passed. The producer regression ran the registered zero-ID image; the metadata check confirmed the zero-case row and digest.
- `cmake --build build/ci --target cpu_test` succeeded after each CPU change.
- `ctest --test-dir build/ci -R '^cpu\.alignment$' --no-tests=error --output-on-failure`: passed.
- `ctest --test-dir build/ci -R '^cpu\.decode$' --no-tests=error --output-on-failure`: passed.
- Final combined build of `cpu_test`, `gamebloke-run-control`, `runner_test`, and `gamebloke-run` succeeded; `ctest --test-dir build/ci -R '^(cpu\.(mailbox|alignment|decode)|runner\.(result_contract|metadata))$' --no-tests=error --output-on-failure` passed all 5 focused tests against the final tree.
- A local consistency check confirmed all four documented CPU-policy codes and all eight Phase 4 result codes also appear in their respective CMake validator catalogs.

## Independent Re-review

The iteration-2 standard review found all four findings resolved with no new issues. It independently checked source logic and regression assertions; the deterministic findings do not leave a separate human product or rights gate. The historical fixer labels above are superseded by this technical review.

## Remaining Phase Gates

The documented trace-write payload-zero limitation remains unchanged. The 11 unresolved policy/probe flags and the subjective guide-clarity human gate also remain open; these review fixes do not resolve them or establish phase acceptance.

---

_Fixed: 2026-11-11T12:24:06Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
