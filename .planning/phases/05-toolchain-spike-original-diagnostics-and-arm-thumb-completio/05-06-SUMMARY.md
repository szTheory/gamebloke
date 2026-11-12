---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "06"
subsystem: cpu
tags: [armv4t, swp, swpb, ordered-transactions, c17]
requires:
  - phase: 05-05
    provides: admitted smoke fixture, canonical CPU and CI seams
provides:
  - One bounded ephemeral decode/preflight/execute path for word SWP and SWPB
  - Ordered bus transaction, result, register-effect, and status-effect slots
  - End-to-end swap and whole-unit refusal evidence
affects: [phase-05-cpu, armv4t, ci, asan]
tech-stack:
  added: []
  patterns:
    - Bounded per-unit transaction slots with bus-owned time
    - Captured operands and register banks applied after ordered bus operations
key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - Keep SWP and SWPB on the existing single decode, preflight, and execute path.
  - Align word reads and rotate the read result by the requested lane; byte swaps use the exact byte address.
  - Classify PC in SWP Rn, Rd, or Rm as unpredictable internally and preserve the existing unsupported-instruction stop result.
patterns-established:
  - A unit captures register operands and banks before effects, orders data slots, and commits final register/status effects after bus work.
  - Trace reservation derives from the unit descriptor; the declared maximum is one instruction, sixteen data operations, and two refill fetches.
requirements-completed: []
coverage:
  - id: D1
    description: Real ARM streams execute word SWP and SWPB through ordered read/write slots with captured operands and lane semantics.
    verification:
      - kind: unit
        ref: "build/ci/cpu_test slots"
        status: pass
      - kind: unit
        ref: "build/ci/cpu_test slots_bounds"
        status: pass
    human_judgment: false
  - id: D2
    description: Exact and one-short cycle, trace, sequence, and counter reservations preserve full machine state and caller trace storage on refusal.
    verification:
      - kind: unit
        ref: "build/ci/cpu_test slots and slots_bounds"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci; build/phase05-plan-ledger/final-ci-confirmation.log"
        status: pass
      - kind: unit
        ref: "build/phase05-plan-ledger/final-asan-tests.log"
        status: pass
    human_judgment: false
actuals:
  tokens: 16744
  tasks: 2
  commits: 4
commits: 4
plan_head_before: ac86b3ccb53cc96a7cbe277edb1316c70f536f0a
plan_head_after: 8d0a25701b6986f5462158f928b40f86ed95ea0d
duration: 30min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 06: Ordered SWP Transactions Summary

**Word SWP and SWPB now execute through bounded ordered bus slots with lane-correct results and atomic whole-unit refusal.**

## Performance

- **Duration:** 30 min 29 sec
- **Started:** 2026-11-12T01:14:58Z
- **Completed:** 2026-11-12T01:45:23Z
- **Tasks:** 2
- **Code and test files modified:** 5

## Accomplishments

- Replaced the single data-access fields with per-unit arrays bounded to 16 data slots/results/register effects, two fetches, and one status effect. The staged trace bound is derived as 19 records: one instruction, sixteen data records, and two refill fetches. Compile-time assertions pin these capacities.
- Routed the existing ARM/Thumb transfer effects and word SWP through the same decoded unit. Word SWP reads the aligned word, rotates the result by the original low address lane, writes the captured Rm value, then commits Rd.
- Added SWPB at the exact byte address, including odd addresses, and exercised Rd/Rm, Rd/Rn, and Rn/Rm aliases. PC in Rn, Rd, or Rm is explicitly classified before bus effects.
- Added whole-machine and full caller-buffer comparisons for refused cycles, trace capacity, trace sequence, master-cycle headroom, and retirement-counter headroom. Exact-fit tests include a nonempty trace prefix and a bus-only category mask.
- Registered `slots` and `slots_bounds` in CTest and the canonical CI and ASan inventories so full lanes execute both new test groups.

## Task Commits

1. **Task 1 RED:** `021b52d` — failing real-stream word SWP and refusal tests.
2. **Task 1 GREEN:** `db5a95b` — bounded ordered unit effects and word SWP execution.
3. **Task 2 RED:** `9b766b7` — SWPB, aliases, PC classification, and reservation boundaries.
4. **Task 2 GREEN:** `8d0a257` — SWPB execution and maximum-capacity assertions.

The planning metadata commit containing this summary and the canonical state/roadmap updates is separate from the four measured task commits.

## Files Created/Modified

- `src/cpu.c` — bounded unit slots, complete-unit cost checks, delayed register/status commits, and SWP/SWPB decode/execution.
- `tests/cpu_test.c` — real SWP/SWPB streams, aliases, PC controls, and exact/one-short refusal assertions.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — canonical registration for the two slot selectors.

## TDD Evidence

- **Task 1 RED:** `build/phase05-plan-ledger/task1-red.tap` and `task1-red-evidence.json`; the SWP behavior assertion failed because the decoder stopped on unsupported instruction while fixture setup passed. The classifier returned `RED_EVIDENCE_OK`.
- **Task 1 GREEN:** `cmake --preset ci && cmake --build --preset ci && build/ci/cpu_test slots` passed all five assertions.
- **Task 2 RED:** `build/phase05-plan-ledger/task2-red.tap` and `task2-red-evidence.json`; all nine assertions were discovered, and only the named SWPB behavior assertion failed because SWPB was unsupported. The classifier returned `RED_EVIDENCE_OK`.
- **Task 2 GREEN:** `cmake --preset ci && cmake --build --preset ci && build/ci/cpu_test slots_bounds` passed all nine assertions.

## Final Verification

- `cmake --workflow --preset ci` completed successfully: 331 tests discovered, 330 passed, and the one configured owner-local logo test skipped because `GBL_OWNER_LOGO_FILE` was unset. The post-inventory check passed with 331 expected, 330 run, and one allowed skip. Evidence: `build/phase05-plan-ledger/final-ci-confirmation.log`.
- Existing CPU lifecycle, budget, trace, trace-capacity, mailbox, `slots`, and `slots_bounds` checks passed in the final focused run (7/7). Evidence: `build/phase05-plan-ledger/final-cpu-regressions.log`.
- The existing macOS `build/asan` configuration used AppleClang `/usr/bin/clang`, Ninja, C17, and ASan+UBSan. Its build passed; 74/74 tests passed, including the new CPU groups and sanitizer controls. The post inventory passed with 74 expected, 74 run, and zero skips. Evidence: `build/phase05-plan-ledger/final-asan-build.log`, `final-asan-tests.log`, and `final-asan-post-confirmation.log`.
- The asan workflow preset itself requests `clang-21`, which is unavailable on this macOS host. After that configure-only failure, the ignored `build/asan` directory was restored with its existing AppleClang sanitizer settings; no project preset, compiler policy, or CI file was changed.

## Deviations from Plan

**1. Registered the new selectors in the canonical test lanes.**

- **Found during:** Task 2
- **Issue:** The plan names `slots` and `slots_bounds`, but without CTest and inventory entries the canonical CI and ASan workflows would not run those selectors.
- **Fix:** Added the two test registrations and corresponding inventory rows.
- **Files modified:** `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt`
- **Verification:** Both selectors passed in the full CI and ASan inventories; inventory checks passed.

**Total deviations:** 1 small verification-registration addition. No ISA or machine-state scope was added.

## Capacity and Scope Notes

The data/result/register-effect arrays are pinned at sixteen entries and the fetch array at two. This change does not implement a block-transfer instruction, so no future LDM/STM execution coverage is claimed; the runtime guest streams here exercise the actual two-operation SWP/SWPB descriptor. The current CPU subset and remaining diagnostic ledger rows stay phase-wide work.

## Issues Encountered

- The macOS ASan preset names unavailable `clang-21`. The preexisting native AppleClang build was restored using its existing sanitizer flags and completed all sanitizer tests.
- The first direct ASan CTest run wrote JUnit under a nested path because CTest resolved the relative filename against `--test-dir`. The unchanged report from the 74-test run was copied to the configured build path; the post-inventory then passed. The earlier path-only post-check failure is retained in `build/phase05-plan-ledger/final-asan-post.log`.
- The first CI log wrapper used zsh's reserved `status` name after the workflow had completed. The workflow was rerun with a non-reserved variable and exited successfully; the clean run is `final-ci-confirmation.log`.

## Requirements

This plan contributes real SWP/SWPB behavior to CPU-01, but does not mark CPU-01 complete. CPU-01/CPU-02 and DIAG-01/02/03 remain Pending for phase-wide diagnostic implementation and final verification.

## Threat Flags

None. The change adds no new trust boundary; decoded spans, counts, costs, counters, and selected trace capacity are checked before data bus effects.

## Next Phase Readiness

Plan 06 is complete. Plan 07 remains the next Phase 05 plan; it was not started. No hosted run, push, PR edit, merge, or Phase 6 work was performed. Root owns publication and the later phase boundary.

## Self-Check: PASSED

- Summary file exists at the canonical Phase 05 path.
- All four task commits are ancestors of the plan head.
- The commit ledger measures four task commits from `ac86b3ccb53cc96a7cbe277edb1316c70f536f0a` through `8d0a25701b6986f5462158f928b40f86ed95ea0d`.
- Changed source and test lines contain no placeholder, TODO, or FIXME stubs.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Code Review Fix Addendum

**Added:** 2026-11-12T02:30:13Z after review `05-06-REVIEW.md`.

- **WR-01 resolved:** Added a private test probe that constructs a synthetic
  maximum-capacity CPU unit using the production slot helpers, then calls the
  same cost, machine preflight, selected trace-count, and trace preflight
  functions used by `gbl_cpu_run`. It executes no bus operations. The tests
  cover 16 data/results/register effects, two fetches, one status effect, all
  19 selected trace records, exact and one-short cycle/master-cycle/retirement/
  trace-capacity/sequence headroom, and one-over rejection for every bounded
  descriptor count. Each attempt compares the full machine bytes and trace
  buffer contents to their snapshots. The synthetic descriptor is a reservation
  probe only; it does not implement or claim LDM/STM execution.
- **WR-02 resolved:** Parameterized the SWP fixture address. Independently
  pinned word-SWP results now cover lanes 0 through 3. SWPB now covers `Rd=Rm`,
  `Rd=Rn`, and `Rn=Rm`, with register and memory outcomes, the exact byte
  address and width, and ordered read-before-write trace records checked.
- **Fix commits:** `dc424d1` (WR-01), `cc352e3` (WR-02), `e95e6c6` and
  `712824d` (keep the private WR-01 seam compatible with copied mutation
  harnesses and remove its superseded header).

### Review-fix verification

All checks ran in the main checkout; `.planning/config.json` has
`workflow.use_worktrees=false`. Evidence is retained under the ignored
`build/phase05-review06/` directory.

- `build/phase05-review06/wr01-targeted.log`: the maximum-descriptor selector
  passed all 20 assertions.
- `build/phase05-review06/wr02-targeted.log` and
  `build/phase05-review06/wr01-seam-compat.log`: the expanded `slots_bounds`
  selector passed all 27 assertions; the copied-source `cpu.pos.clean`
  mutation control passed.
- `build/phase05-review06/cpu-regressions.log`: existing CPU lifecycle,
  budget, mailbox, trace, trace-capacity, `slots`, and `slots_bounds` CTest
  regressions passed 7/7.
- `build/phase05-review06/final-ci-retry.log`: canonical
  `cmake --workflow --preset ci` passed all 330 executed tests. The inventory
  post-check passed with 331 expected, 330 run, and the one configured owner-local
  signature test skipped because `GBL_OWNER_LOGO_FILE` was unset. CTest time
  was 269.95 seconds; the build test took 0.62 seconds, and the longest work
  was the distinct mutation controls.
- `build/phase05-review06/final-asan.log`: native AppleClang `/usr/bin/clang`,
  Ninja, ASan+UBSan build passed. All 74 non-post tests passed. A first direct
  CTest invocation included the post-inventory test before JUnit generation;
  after the report was written, the post check passed with 74 expected, 74 run,
  and zero skips.
- The initial CI attempt is retained at
  `build/phase05-review06/final-ci.log`; it exposed the copied-source harness
  not copying a newly added test header. The hook declaration was moved into
  the existing copied private helper header and the final canonical run passed.

CPU-01/02 and DIAG-01/02/03 remain phase-wide Pending. Plan 07 remains the next
Phase 05 plan; no guest block-transfer implementation, hosted CI, publication,
or Phase 6 work was performed.

## Code Review Fix Addendum — Iteration 2

**Added:** 2026-11-12T02:43:53Z after the independent re-review in
`05-06-REVIEW.md`.

- **WR-03 resolved:** Removed padding-sensitive `memcmp` checks for the machine
  and trace-buffer structs in the maximum-reservation test. Machine snapshots
  use the canonical equality, digest, and same-instance identity helpers; the
  trace pointer, capacity, count, and sequence are compared field by field.
  The initialized trace-record array still uses a raw byte comparison.
- **Commit:** `2548cb1` (`tests/cpu_test.c` only). No CPU implementation or
  helper behavior changed.
- **Fresh verification:** Normal and native AppleClang ASan/UBSan builds of
  `cpu_test` passed. In each build, `slots_bounds` passed 27/27 assertions,
  `slots` passed 5/5, and the existing CPU lifecycle/budget/mailbox/trace
  regressions passed 5/5 with no skips. Evidence: `build/phase05-review06/`
  `iteration2-normal.log` and `iteration2-asan.log`.
- Full CI and full sanitizer inventories were not rerun for this test-only
  correction. The broader isolated Linux GCC canonical CI receipt is owned by
  the parent reviewer; no later plan or state advancement occurred here.
