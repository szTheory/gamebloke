---
phase: 04-cpu-execution-core-and-headless-runner
plan: "12"
subsystem: testing
tags: [CMake, JSON, headless-runner, semantic-results, CI, ASAN]

# Dependency graph
requires:
  - phase: 04-11
    provides: Bounded runner CLI, synthetic control fixtures, and result JSON producer
provides:
  - Semantic, mailbox, machine-time, termination, and trace-completeness validation for result/1
  - Actual producer-to-validator integration control preserving dispute identifiers and labels
  - CI, ASAN, and hygiene registrations plus evidence-contract documentation
affects: [04-13, evidence-contract, headless-runner, CI, ASAN]

# Measured at summary creation, before the summary metadata commit.
actuals:
  tokens: 10865
  tasks: 2
  commits: 3
plan_head_before: 051db3d46fea40c068e5e87f3761a348a924724b
plan_head_after: 894417a9221e9af660d7a0f2ed3d556b6b2759be

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Feed actual CLI stdout directly to the existing result checker using generated local control metadata.
    - Compare canonical decimal uint64 strings by length and lexical order, not signed CMake arithmetic.
    - Keep synthetic validator rows and generated control images separate from the empty production manifest.

key-files:
  created:
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-12-SUMMARY.md
  modified:
    - cmake/CheckResult.cmake
    - tests/gates/runner_case.cmake
    - contracts/examples/result-pass.json
    - tests/gates/result_case.cmake
    - tests/core.cmake
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - tests/inventory/hygiene.txt
    - docs/evidence-contract.md

key-decisions:
  - Keep gamebloke.result/1 and the existing status and label vocabulary; require semantic, time, termination, and trace observations within that contract.
  - Allow mailbox completion at the exact budget, while accepting that bounded execution can stop short when another whole instruction or refill unit cannot fit.
  - Keep production fixture authority tied to the canonical empty manifest; synthetic control metadata grants no fixture rights or production approval.
  - Preserve exact dispute ids and the disputed label from validated metadata, and keep runtime manifest/index overrides unsupported.

requirements-completed: [] # Parent owns the phase-level requirements roll-up.
requirements-addressed: [RUN-01, RUN-02, RUN-03, PROF-03]
coverage:
  - id: D1
    description: Result validation binds status to mailbox semantics, exact machine-time bounds, termination, and trace completeness.
    requirement: RUN-01
    verification:
      - kind: unit
        ref: tests/gates/result_case.cmake#semantic_time; ctest result.*
        status: pass
      - kind: integration
        ref: tests/gates/runner_case.cmake; cmake --workflow --preset ci
        status: pass
    human_judgment: false
  - id: D2
    description: Actual producer output validates through the same checker and preserves registered dispute disclosure.
    requirement: RUN-02
    verification:
      - kind: integration
        ref: tests/gates/runner_case.cmake#producer-to-validator controls; runner.result_contract
        status: pass
      - kind: integration
        ref: ctest --test-dir build/asan -R '^runner\.result_contract$'
        status: pass
    human_judgment: false
  - id: D3
    description: Evidence documentation describes the real CLI, mailbox, metadata authority, and limits.
    requirement: RUN-03
    verification: []
    human_judgment: true
    rationale: Automated tests cover executable contracts, but a human must judge the clarity of the explanatory guide and its rights boundaries.

# Metrics
duration: "30min from first task commit through verification"
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 12: Semantic and time result contract Summary

**The result checker now validates actual runner output against mailbox semantics, exact cycle bounds, termination, and trace completeness while preserving synthetic metadata limits.**

## Performance

- **Duration:** 30 minutes from first task commit through verification; initial context loading preceded this measured window.
- **Started:** 2026-11-11T10:11:39Z
- **Completed:** 2026-11-11T10:41:00Z
- **Tasks:** 2
- **Files modified:** 10 implementation, test, inventory, and documentation files; the summary is a separate output.

## Accomplishments

- Required semantic state and case id, canonical decimal-string uint64 counters, cycle budget, named termination, and trace completeness in the existing `gamebloke.result/1` contract. PASS now requires a completed PASS mailbox, nonzero case id, guest progress, and `mailbox-complete`; GUEST_FAIL, no-BIOS GUEST_STOP, trace-output-full, and other bounded states retain explicit consistency checks.
- Connected actual `gamebloke-run-control` stdout to `CheckResult.cmake`. Actual PASS and FAIL images validate against their generated control manifest; the disputed PASS retains `disp-gbatek-errata` and the exact `disputed` label. Mutated copies fail with specific reasons, including pending PASS, omitted fields, invalid or overflowing counters, over-budget cycles, removed dispute disclosure, and incomplete traces.
- Registered the producer contract in CI and ASAN, kept semantic/time controls in CI and hygiene, and documented CLI flags, mailbox wire layout and completion order, exact time representation, metadata authority, diagnostics, and unsupported scope.

## TDD Gate Compliance

- **RED:** Commit `de7428b` added actual-output producer/validator assertions. Replaying the pending-mailbox mutation against the pre-implementation checker returned `result: ok (PASS)` instead of rejecting it. The unchanged CTest JUnit report at `build/phase04-tracer/plan12-red.xml` and record at `build/phase04-tracer/plan12-red-evidence.json` passed `gsd_run check tdd-red-evidence ... --raw` as `RED_EVIDENCE_OK target_test_failed`; the failure was on the named producer contract assertion, not setup or fixture loading.
- **GREEN:** Commit `75e592b` added the canonical semantic, time, and termination checks and updated the example and mutation controls. The actual producer integration and direct semantic/time controls passed.
- **REFACTOR:** Task 2 commit `894417a` made trace-output-full and incomplete-trace validation independent of status-branch ordering, added wrong-status and non-HARNESS_FAIL trace controls, and wired the complete controls into the required lanes.

## Verification

- `cmake -DSRC=. -DBIN=build/ci -DVALIDATE_PRODUCER=ON -P tests/gates/runner_case.cmake` — actual producer acceptance, dispute preservation, semantic/time mutations, and production-manifest refusal passed.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(runner\.result_contract|result\.)' --no-tests=error --output-on-failure` — 30/30 selected tests passed.
- `cmake --workflow --preset ci` — 326 passed, one permitted `signature.owner_local` check skipped because `GBL_OWNER_LOGO_FILE` was unset; inventory validation passed with 327 expected checks. CTest took 75.05 seconds. The workflow built no targets after configuration, so there was no duplicate build setup to remove.
- `cmake --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --test-dir build/asan -R '^runner\.result_contract$' --no-tests=error --output-on-failure` — producer contract passed 1/1.
- `cmake --preset hygiene && cmake --build --preset hygiene && ctest --test-dir build/hygiene -R '^(result\.|hygiene\.tree$)' --no-tests=error --output-on-failure` — 30/30 selected tests passed.

No packages were installed. The canonical production manifest remains empty. No private ROM or BIOS was executed or committed, and no fixture rights, hardware qualification, or broad compatibility claim is inferred from the synthetic controls. The generated control rows and images are local validator inputs only. No runtime index or source-path override was introduced.

## Task Commits

1. **Task 1 RED:** `de7428b` (`test(04-12): add actual producer result contract controls`)
2. **Task 1 GREEN:** `75e592b` (`feat(04-12): enforce producer semantic and time result contract`)
3. **Task 2:** `894417a` (`fix(04-12): enforce trace completeness across result statuses`)

The final documentation commit contains this summary. Plan task commits measured from the saved base: 3 (`051db3d46fea40c068e5e87f3761a348a924724b` to `894417a9221e9af660d7a0f2ed3d556b6b2759be`).

## Files Created/Modified

- `cmake/CheckResult.cmake` — validates semantic state, canonical case id, uint64 time values, cycle bounds, termination, and trace completeness.
- `tests/gates/runner_case.cmake` — sends real producer JSON through the checker and applies direct mutations while checking exact dispute metadata and production-manifest rejection.
- `contracts/examples/result-pass.json` — current illustrative result/1 example with semantic, time, termination, and trace fields.
- `tests/gates/result_case.cmake` — adds exact-boundary, uint32/uint64 edge, missing-field, and trace/status mutation controls.
- `tests/core.cmake`, `tests/gates.cmake`, and CI/ASAN/hygiene inventories — register the real producer contract and semantic/time controls in the intended lanes.
- `docs/evidence-contract.md` — describes the result contract, actual CLI and mailbox behavior, synthetic authority limits, and unsupported machine scope.

## Decisions Made

- Kept schema `gamebloke.result/1`, the existing status vocabulary, and the sole `disputed` label unchanged.
- Used decimal strings for uint64 time values and lexical range comparisons to avoid CMake signed-integer overflow and JSON consumer rounding.
- Treated mailbox observations, guest status, machine time, and host validation as distinct evidence; raw diagnostic output does not become validated PASS by status mutation.
- Preserved no runtime index override and the empty production manifest. The generated synthetic manifest is not a rights grant.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Enforced incomplete-trace checks across all status branches**
- **Found during:** Task 2 verification.
- **Issue:** The initial trace checks were chained to the status classification, so a non-HARNESS_FAIL result could bypass the trace-output-full or incomplete-trace consistency rule.
- **Fix:** Validated trace-output-full and incomplete-trace conditions independently of status dispatch and added direct controls for wrong-status and incomplete non-HARNESS_FAIL output.
- **Files modified:** `cmake/CheckResult.cmake`, `tests/gates/result_case.cmake`.
- **Verification:** `result.nc.semantic_time`, the focused result/runner CTest suite, and the full CI workflow passed.
- **Committed in:** `894417a`.

**Total deviations:** 1 auto-fixed (Rule 1). **Impact on plan:** Closed a correctness gap in the new contract; no scope expansion.

## Known Stubs

| File | Line | Reason |
| --- | ---: | --- |
| `docs/evidence-contract.md` | 209 | This guide records the existing placeholder bus costs and unverified absent-BIOS, unsupported-I/O, and ROM-tail current-latch behavior. These are pre-existing limitations already tracked in `.planning/WINDOWS.md`; this plan does not claim hardware timing or accuracy. |

The existing spec-less probe classifications and descriptor-less rights/compatibility restrictions remain open for phase verification. The inherited timing and current-latch items are already present in the cross-phase ledger, so this plan adds no duplicate entries. They do not prevent completion of the result-contract goal.

## Threat Surface Scan

No new network endpoints, authorization paths, file access patterns, or schema versions were added. The changed validation remains within the plan's declared host-file/metadata-to-evidence trust boundary.

## Issues Encountered

The first isolated replay used for machine-validating the RED report lacked a symlink to an existing CMake helper and failed before reaching the target assertion. After linking the existing helper files into the temporary harness, the same target failed on the intended pending-mailbox assertion and the evidence checker accepted the report.

## User Setup Required

None.

## Next Phase Readiness

Plan 04-13 can rely on a registered producer-to-validator path and an explicit result contract. Parent phase verification still owns the broad requirements roll-up. This plan establishes no hardware qualification, BIOS-backed behavior, rights determination, or compatibility claim.

## Self-Check: PASSED

- The summary file exists at the required phase path.
- All three task commits are ancestors of the current plan branch HEAD.
- The saved plan base and measured task commit count match the recorded metadata.
- `git diff --check` and the summary trailing-whitespace scan passed.

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Completed: 2026-11-11*
