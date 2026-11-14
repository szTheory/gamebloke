---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "05"
subsystem: diagnostics
tags: [arm7tdmi, armv4t, thumb, cmake, fixture-manifest, xpack]

requires:
  - phase: 04
    provides: Production runner, mailbox results, and canonical CheckResult validation.
  - phase: 05-04
    provides: Manifest artifact digest support and independent source/tool/output identities.
provides:
  - Production manifest admission for the original smoke positive and real assertion-broken image.
  - A source and toolchain-bound ARM/Thumb coverage ledger with explicit pending cases.
  - Production index and metadata controls that follow canonical manifest authority and exclude synthetic fixtures.
affects: [phase-05-plans-06-25, arm-thumb-coverage, diagnostics-fixtures, production-runner]

actuals:
  tokens: 138661.25 # 554645 changed diff characters / 4; excludes this summary and planning bookkeeping.
  tasks: 2
  commits: 17 # measured from the persisted 05-05 plan ledger to the code head.
plan_head_before: 8d01f9adbd6273964d10fcbd649a0415d0e2d385
plan_head_after: 03cb2840e009dfbcb21029b5af905b98101cc7f7

tech-stack:
  added: []
  patterns:
    - Keep source revision, host-specific toolchain archive identity, and generated ROM SHA as separate facts.
    - Admit generated ROM bytes only after an immutable production index is rebuilt from the canonical manifest.
    - Keep synthetic runner controls isolated from production fixture authority.

key-files:
  created:
    - diagnostics/armv4t/cases.json
    - diagnostics/armv4t/inputs.txt
    - diagnostics/armv4t/NOTICE
    - cmake/CheckArmCoverage.cmake
    - cmake/BuildArmDiagnostics.cmake
    - tests/gates/coverage_case.cmake
    - tests/gates/diagnostics_case.cmake
  modified:
    - CMakeLists.txt
    - tests/fixtures/manifest.json
    - tests/gates/runner_metadata.cmake
    - tests/gates/manifest_case.cmake

key-decisions:
  - "Pin source.commit to 4857ddc9c6ecaef61644fa4cf6286e7951e3bff4, the hosted-green source-only producer revision."
  - "Keep build.toolchain.sha256 null because the manifest has one field but the locked archive SHA differs by host; name and record each locked host archive in the toolchain identity and lock file."
  - "Admit only the measured smoke pair; leave the remaining 356 ledger rows pending and make final/full-coverage mode refuse honestly."

requirements-completed: []
coverage:
  - id: D1
    description: The original smoke positive and assertion-broken ROMs are pinned to the reviewed producer revision, exact artifact hashes, host-specific toolchain archives, BIOS none, and the fixture-specific NOTICE; this contributes evidence toward DIAG-01.
    requirement: DIAG-01
    verification:
      - kind: integration
        ref: "cmake -DCASE=smoke-pair -DSRC=. -DBIN=build/ci -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake"
        status: pass
      - kind: other
        ref: "Literal cmp: build/phase05-spike/linux-arm64-4857ddc-refresh/roms/{smoke,smoke-broken}.gba against build/phase05-spike/native/roms/"
        status: pass
    human_judgment: false
  - id: D2
    description: The production runner and canonical result checker accept PASS/pass/20481 for smoke and GUEST_FAIL/fail/20481 for the real operand-broken variant, with exact loaded ROM SHA and mailbox-complete state; this contributes evidence toward DIAG-03.
    requirement: DIAG-03
    verification:
      - kind: integration
        ref: "build/phase05-spike/admission/diagnostics-case-final.log"
        status: pass
    human_judgment: false
  - id: D3
    description: The ARM/Thumb family-form ledger identifies all 19 Thumb formats, keeps unimplemented rows pending, and refuses full-mode admission while coverage is incomplete.
    verification:
      - kind: integration
        ref: "build/phase05-spike/admission/coverage-controls-final.log"
        status: pass
    human_judgment: false
  - id: D4
    description: The pinned producer emitted ARMv4T output and the measured smoke pair matched byte-for-byte between local macOS ARM64 and Linux ARM64; hosted Linux x64 and macOS ARM64 hashes also match. This is partial evidence for DIAG-02, not phase-wide completion.
    requirement: DIAG-02
    verification:
      - kind: other
        ref: "build/phase05-spike/linux-arm64-4857ddc-refresh/proof/build-receipt.json and literal cmp against build/phase05-spike/native/roms/"
        status: pass
      - kind: other
        ref: "build/phase05-spike/installer-optimization/hosted-final-run.json and host logs"
        status: pass
    human_judgment: false

duration: "multi-resume; admission continuation 25min"
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 05: Original Diagnostic Smoke Admission Summary

**The source-pinned ARM/Thumb smoke pair now runs through the real production index and canonical result checker, with exact mailbox identity and local cross-host byte parity.**

## Performance

- **Duration:** Multi-resume plan; the admission continuation began at 2026-11-12T00:22:29Z and completed in about 25 minutes.
- **Tasks:** 2.
- **Files modified:** 17 across the complete plan diff.
- **Plan commits:** 17 measured task commits; the summary and planning-state commit is separate.

## Accomplishments

- Added production manifest rows for `smoke` and `smoke-broken`, both pinned to producer source commit `4857ddc9c6ecaef61644fa4cf6286e7951e3bff4` and their measured ROM SHA-256 values. The rows identify distinct clean and assertion-broken recipes and use the exact fixture-specific SPDX identifier `LicenseRef-Gamebloke-ARMv4T-Diagnostic-Public-CI` from `diagnostics/armv4t/NOTICE`, with BIOS requirement `none` and the existing narrow public-CI scope.
- Recorded all three locked host archive identities (Darwin ARM64, Linux x64, and Linux ARM64) without mislabeling an archive digest as the ROM or as a single cross-host toolchain digest. `build.toolchain.sha256` is null because the manifest schema has only one field for three host-specific archive hashes.
- Refreshed the pinned native Linux ARM64 build at the bound source commit. Both Linux ARM64 outputs matched the native macOS ARM64 files by literal `cmp`; hosted Linux x64/macOS ARM64 evidence separately establishes hash equality on the green source-only run, not a hosted literal comparison.
- Ran the actual built `gamebloke-run --fixture` path with the production immutable index and canonical `CheckResult.cmake`: smoke produced `PASS`, semantic `pass`, case 20481; the same-case real operand-broken ROM produced `GUEST_FAIL`, semantic `fail`, case 20481. Both results checked mailbox completion, loaded bytes, and the exact expected ROM SHA.
- Kept the ledger explicit: 334 ARM/Thumb target forms across all 19 Thumb formats; one smoke pair is source-verified and 356 rows remain pending. Draft validation passes, while the full builder and final mode refuse incomplete coverage. This does not claim full ISA coverage.
- Generalized `runner.metadata` to require the actual canonical manifest row count and IDs while rejecting synthetic control rows. The empty-manifest positive test now creates its own zero-row fixture, and the missing-link coverage negative control now passes its one-row manifest mutation to the checker.

## Verification

- `cmake -DCASE=smoke-pair -DSRC=. -DBIN=build/ci -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake` — passed; real production runner and canonical checker reported both expected results.
- Native Linux ARM64 `BuildArmDiagnostics.cmake` case 20481 build — source inputs matched `4857ddc`; Linux ARM64 and macOS ARM64 output files matched byte-for-byte for both variants.
- Normal and macOS ASan/UBSan `runner.metadata` — passed 1/1 in `build/ci` and `build/asan` respectively.
- `coverage_case.cmake` ledger controls and `CheckArmCoverage.cmake` draft validation — passed; 356 pending cases remain visible and full-mode refusal is deliberate.
- `cmake --workflow --preset ci` — passed. It inventoried 329 checks, ran 328 successfully, and retained the single existing `signature.owner_local` skip because `GBL_OWNER_LOGO_FILE` was unset. CTest took 79.64 seconds and post-inventory took 0.08 seconds; configure took 0.1 seconds and Ninja reported no build work, so no duplicate setup/build path was measured.
- Hosted run 43126123712 attempt 1 remains green on exact source commit `4857ddc`; all jobs and `CI required` succeeded. Linux and macOS diagnostic jobs reported zero skips and the same output hashes. Root owns any future publication or hosted run.

Detailed local logs and receipts remain under ignored `build/phase05-spike/`, including `admission/diagnostics-case-final.log`, `admission/runner-metadata-normal-final.log`, `admission/runner-metadata-asan-final.log`, `admission/coverage-controls-final.log`, `admission/full-canonical-ci-final2.log`, and `linux-arm64-4857ddc-refresh/proof/build-receipt.json`. Hosted raw metadata and logs remain under `installer-optimization/hosted-*`.

## Task Commits

1. **Task 1: Admit the original smoke positive and genuine assertion-broken image** — `a8a0bfc`, `16fd808`, `53a31fe`, `0ba747a`, `7f169c5`, `6538476`, `ca0d51d`, `c20d606`, `8f0ffa1`, `4857ddc`, `b112318`.
2. **Task 2: Create the inspectable family/form/case crosswalk and completeness gate** — `ce82147`, `6cde0d3`, `6bc4b0a`, `a78a0dc`, `b120c6c`, `03cb284`.

## Decisions Made

- A single manifest toolchain digest cannot truthfully represent three host archives; archive hashes stay host-specific in `toolchain/arm-none-eabi.lock` and the manifest identity, with the scalar SHA field null.
- The production manifest admits only the measured smoke pair. Later Phase 05 plans must implement and verify the remaining ledger rows rather than map them to smoke.
- No new project license, BIOS, third-party ROM, binary upload, broad compatibility claim, or Phase 6 work was introduced.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The runner metadata consumer assumed the production manifest always had zero rows.**
- **Found during:** Task 1 admission.
- **Issue:** The actual canonical production index correctly contained the two newly admitted rows, so the consumer test's fixed zero-row assertion rejected valid production authority.
- **Fix:** Compare the generated production row count and each canonical manifest ID; explicitly exclude synthetic control IDs.
- **Files modified:** `tests/gates/runner_metadata.cmake`.
- **Verification:** Normal and ASan/UBSan `runner.metadata` passed; final canonical CI passed.
- **Committed in:** `b112318`.

**2. [Rule 1 - Bug] The positive empty-manifest test copied the live canonical manifest.**
- **Found during:** Full canonical CI after Task 1 admission.
- **Issue:** `manifest.pos.empty` expected zero rows and failed when the production manifest gained two legitimate fixtures.
- **Fix:** Generate an isolated zero-row test fixture independent of the live production manifest.
- **Files modified:** `tests/gates/manifest_case.cmake`.
- **Verification:** `manifest.pos.empty` passed; final canonical CI passed.
- **Committed in:** `b112318`.

**3. [Rule 1 - Bug] The missing-link coverage control ignored its one-row manifest mutant.**
- **Found during:** Task 2 direct ledger-control verification.
- **Issue:** The helper always passed the canonical manifest, so the mutated fixture link was never tested.
- **Fix:** Allow the helper to receive a manifest path and pass the one-row mutant to the checker.
- **Files modified:** `tests/gates/coverage_case.cmake`.
- **Verification:** Full ledger-control suite passed, including missing-link refusal and final-mode pending-case refusal.
- **Committed in:** `03cb284`.

**Total deviations:** 3 auto-fixed Rule 1 test-consumer/control bugs. No producer input changed after the hosted-green source commit.

## Issues Encountered

- The first local full-CI attempt showed `manifest.pos.empty` was coupled to the production manifest. The isolated fixture fix resolved it; the final canonical CI workflow passed without new skips.
- The first Linux refresh command used an output path deeper than the builder's permitted single child under `build/phase05-spike`; its safety check stopped before writing. The corrected single-level path produced the verified result.

## Known Stubs

None. Pending ledger rows are explicit coverage states, not placeholder implementations.

## Threat Flags

None. The changes add no new network endpoint, authorization path, external file access, or trust-boundary schema. Fixture rights and generated-byte scope remain constrained by the existing NOTICE and manifest validator.

## Next Phase Readiness

Plan 05 is complete. The production smoke pair and diagnostic ledger are ready for later Phase 05 ARM/Thumb case implementation and full-matrix measurement. Phase 05 remains in progress; do not start Phase 6 or treat this smoke pair as full ISA coverage. Root remains the actor for future publication and hosted monitoring.

## Phase-Wide Requirement Status

This plan contributes measured smoke, source, toolchain, cross-host identity, and result-checking evidence toward DIAG-01, DIAG-02, and DIAG-03. These Phase 05 requirements remain pending: the ledger still has 356 unverified cases, and the phase verifier must assess the complete acceptance criteria after later case plans. Plan 05 itself is complete; this summary does not mark any phase-wide requirement complete.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists and records all 17 measured task commits; all 17 are ancestors of the plan head.
- The 17-commit ledger count matches `git rev-list --count` from plan base `8d01f9adbd6273964d10fcbd649a0415d0e2d385` through plan head `03cb2840e009dfbcb21029b5af905b98101cc7f7`.
- All bound producer inputs remain byte-identical to source commit `4857ddc9c6ecaef61644fa4cf6286e7951e3bff4`.
- Final canonical CI, production smoke, normal and ASan/UBSan metadata, and coverage-control evidence files are present under ignored `build/phase05-spike/`.
