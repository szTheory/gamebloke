---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "03"
subsystem: toolchain-ci
tags: [cmake, github-actions, armv4t, diagnostics, linux-x64, darwin-arm64]

requires:
  - phase: 05-02
    provides: Original ARM/Thumb diagnostics, strict local proof controls, and macOS/Linux arm64 byte parity.
provides:
  - Canonical pinned diagnostics workflow with required Linux x64 and macOS arm64 hosted witnesses.
  - Fail-closed CI needs/static controls and an exact-row, source-head-bound hosted proof receipt.
affects: [phase-05-diagnostics, toolchain, ci, verification]

actuals:
  tokens: 14755 # 59,020 changed diff characters / 4; excludes this summary and state/roadmap bookkeeping.
  tasks: 2
  commits: 9 # measured from the persisted 05-03 plan ledger to the tested source head.
plan_head_before: 3bb6871765b717733e5b447e009bdb19de8de209
plan_head_after: 2a18d8e1b2f7d24b11b40627e697cfd42b60c48b

tech-stack:
  added: []
  patterns:
    - CMake workflow preset with required test inventory and a post-test JUnit inventory check.
    - Script-mode host detection using checked uname results and an explicit supported-platform mapping.
    - Hosted proof tied to the full source head and exact unique output-name rows.

key-files:
  created:
    - tests/toolchain.cmake
    - tests/inventory/diagnostics.txt
  modified:
    - CMakeLists.txt
    - CMakePresets.json
    - .github/workflows/ci.yml
    - scripts/check-workflow.sh
    - tests/ci/ci_cases.cmake
    - tests/ci/workflow_cases.cmake
    - tests/gates/toolchain_case.cmake
    - tests/gates/toolchain_spike.cmake
    - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-TOOLCHAIN-SPIKE.md
    - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-03-CI-HANDOFF.md

key-decisions:
  - "Keep required jobs within their existing 180-second limit; measured corrected diagnostics jobs were 92s on Linux x64 and 145s on macOS arm64."
  - "Use SHA-256 identity against explicitly local macOS arm64 counterparts for hosted Linux x64; literal cmp evidence applies only to the local macOS arm64/Linux arm64 pair."
  - "Resolve the original hosted stop only after preserving the owner's explicit authorization and capturing a corrected all-green run at the exact source head."
  - "Do not admit a ROM digest or mark the phase-wide DIAG-01, DIAG-02, or VERIF-04 requirements complete from this bounded plan."

patterns-established:
  - "A registered offline-driver test runs host-detection controls, the complete installer refusal suite, and a controlled failure that must propagate."
  - "Hosted output receipts require exact unique names before hash comparison and identify the provenance of local comparison bytes."

requirements-completed: []

coverage:
  - id: D1
    description: "One canonical diagnostics workflow builds and checks the original ARM/Thumb positive and broken images on Linux x64 and macOS arm64."
    verification:
      - kind: integration
        ref: "build/phase05-spike/05-03-corrected-linux-x64.log and 05-03-corrected-macos-arm64.log (4/4 tests, post-inventory 1/1, zero skips on each host)"
        status: pass
      - kind: integration
        ref: "build/phase05-spike/05-03-host-fix-darwin-diagnostics.log and 05-03-host-fix-linux-diagnostics.log (local Darwin and pinned Linux arm64 workflows)"
        status: pass
    human_judgment: false
  - id: D2
    description: "A missing or failed diagnostics job cannot disappear from CI routing or the required result."
    verification:
      - kind: other
        ref: "sh scripts/check-workflow.sh"
        status: pass
      - kind: other
        ref: "cmake -DCASE=needs -DSRC=. -DBIN=build/phase05-spike -P tests/ci/ci_cases.cmake (17 cases)"
        status: pass
      - kind: integration
        ref: "GitHub Actions run 43089440697: every underlying job and CI required succeeded at the tested source head"
        status: pass
    human_judgment: false
  - id: D3
    description: "The Linux x64 receipt is source-head-bound and validates the exact unique output rows against independently built local counterparts."
    verification:
      - kind: integration
        ref: "cmake -DCASE=hosted-proof -DSRC=. -DBIN=build/phase05-spike -P tests/gates/toolchain_spike.cmake"
        status: pass
      - kind: other
        ref: "build/phase05-spike/hosted-linux-x64.json and local-counterparts/LOCAL-PROVENANCE.json"
        status: pass
    human_judgment: false

duration: "not measured (multi-resume execution)"
completed: 2026-11-11
status: complete
---

# Phase 05 Plan 03: Required Diagnostics CI Summary

**The bounded hosted witness now passes on Linux x64 and macOS arm64 within the unchanged 180-second jobs, with hosted SHA identity kept distinct from local literal byte comparisons.**

## Performance

- **Duration:** Not measured across the resumed execution and owner-authorized stop.
- **Started:** The plan-specific start marker was unavailable after continuation; first plan test commit was `2026-11-11T13:31:24-05:00`.
- **Completed:** 2026-11-11, after the corrected hosted run and local closeout checks.
- **Tasks:** 2.
- **Files modified:** 12 plan/source and closeout artifacts (10 source/config/test files, the spike report, and the CI handoff).

## Accomplishments

- Added the diagnostics workflow and exact sorted inventory, with required pinned-tool checks, the existing CMake build/test path, and a post-test JUnit inventory check.
- Added a required Linux x64 and macOS arm64 diagnostics matrix, plus static and needs controls so a missing, failed, or skipped diagnostics job cannot pass through `CI required`.
- Bound the hosted receipt to the checked-out source head and validated the exact unique set of ROM output rows before comparing their hashes with documented local counterparts.
- Corrected CMake script-mode host detection after reproducing the hosted Linux offline-control failure. The helper now validates `uname` status, output, and supported host/architecture pairs; its regression controls cover empty CMake processor metadata, unsupported hosts, command failure, and empty output.
- Added the offline installer `offline-all` suite to the registered recurring test and proved that a controlled installer refusal is propagated by the driver.
- Captured actual hosted job durations and distinguished whole fetch-step time from the narrower archive transfer/extraction receipt fields.

## Task Commits

1. **05-03-01: Expose the measured spike as a small canonical workflow** — `9263a31` (RED preset contract), `8443dc4` (workflow implementation).
2. **05-03-02: Use the phase's one draft PR for the missing hosted x64 witness** — `d896084` (RED routing controls), `69416c9` (required diagnostics witness), `129d835` (bind receipt to source head), `75fee89` (expose hosted architecture attributes), `7512398` (output-row controls), `444f034` (complete hosted evidence validation), `2a18d8e` (script-mode host detection correction).

The source plan ledger measures nine commits from `3bb6871` through tested source head `2a18d8e`. Planning metadata is committed separately after this summary's self-check.

## Test and Hosted Evidence

- Native macOS arm64 diagnostics workflow: 4/4 required tests passed, then the post-test inventory passed 1/1; zero skips. Log: `build/phase05-spike/05-03-host-fix-darwin-diagnostics.log`.
- Pinned Linux arm64 container diagnostics workflow with the existing read-only xPack cache: 4/4 required tests passed, then the post-test inventory passed 1/1; zero skips. Log: `build/phase05-spike/05-03-host-fix-linux-diagnostics.log`.
- Corrected hosted Linux x64 and macOS arm64 diagnostics jobs each ran 4/4 tests and passed the 1/1 post-test inventory with zero skips. Every captured underlying job and `CI required` passed in [run 43089440697](https://github.com/szTheory/gamebloke/actions/runs/43089440697), checked out at full source head `2a18d8e1b2f7d24b11b40627e697cfd42b60c48b`.
- `sh scripts/check-workflow.sh` passed. The CMake needs suite passed all 17 cases. The real `hosted-proof` selector passed against the captured Linux x64 job and the local counterpart provenance.
- The hosted Linux x64 job completed in 92s (81s fetch step, 5s diagnostics workflow), leaving 88s under the fixed 180s cap. The hosted macOS arm64 job completed in 145s (130s fetch step, 7s workflow), leaving 35s. The other jobs passed: Ubuntu GCC build 113s, macOS build 140s, GNU regression 113s, ASan/UBSan 100s, hygiene 61s, and `CI required` 6s.
- The installed toolchain is xPack GNU Arm Embedded GCC `15.2.1-1.1`. On both hosted diagnostic jobs, all six object/final-ELF records report ARMv4T, ARM ISA support, and Thumb-1. The positive/broken mailbox outcomes are semantic pass/fail at case `20481`; unregistered execution remains the raw `GUEST_STOP` / `unregistered-content`, exit 1.
- Hosted hashes match the independently built local counterpart files: `smoke.gba` `424a460469dbdbbf98b1eb1991e26a45ae9324a2127936f81572308ba38510d9`; `smoke-broken.gba` `76d20735f76c2bb8c67117c98b6488f0ab324039b8b19d768f14aecb64caa9e6`. Local macOS arm64 versus native Linux arm64 literal `cmp` returned 0 for both files. No literal hosted/local `cmp` is claimed.
- No full 329-test local CI rerun was made for the isolated helper correction. The corrected hosted run passed all underlying required jobs at the tested head, and the complete diagnostics workflow was rerun locally on native macOS and pinned Linux arm64.

Raw hosted metadata and logs are preserved in ignored `build/phase05-spike/05-03-corrected-hosted-run.json`, `05-03-corrected-hosted-jobs.json`, `05-03-corrected-linux-x64.log`, and `05-03-corrected-macos-arm64.log`. The hosted receipt and local provenance are `build/phase05-spike/hosted-linux-x64.json` and `build/phase05-spike/local-counterparts/LOCAL-PROVENANCE.json`.

## Decisions Made

- Keep the existing 180-second job cap, pinned checkout, permissions, and toolchain source identity. The measured toolchain fetch step is the dominant diagnostics critical-path cost; narrow transfer/extraction sub-times do not represent setup end-to-end. The jobs do not repeat the same host installation within a lane, so no speculative caching or dependency change was justified.
- Treat the hosted Linux x64 output as SHA-256 identity checked against an explicitly local macOS arm64 counterpart. The literal byte comparison remains only between local macOS arm64 and native Linux arm64 outputs.
- Retain the historical red CI record and the owner's explicit authorization. The corrected green run resolves that stop for Plan 03; any future red hosted run still requires a stop before correction or retry.
- Keep digest admission and full Phase 05 requirement completion outside this bounded plan. `DIAG-01`, `DIAG-02`, and `VERIF-04` remain pending phase-wide verification; this plan's hosted witness is not the full diagnostic matrix.

## Deviations from Plan

### Auto-fixed issues

**1. [Rule 2 - Missing critical control] Kept the missing-job negative control pointed at the new diagnostics dependency**

- **Found during:** Task 2 static workflow routing.
- **Issue:** The existing `needs_missing` mutation removed the old `asan` dependency and would not demonstrate refusal if the newly required diagnostics job disappeared.
- **Fix:** Updated `tests/ci/workflow_cases.cmake` so the mutation removes `diagnostics` from the current needs list and requires the expected missing-diagnostics refusal.
- **Verification:** `sh scripts/check-workflow.sh` and the 17-case needs suite passed; the hosted `CI required` job passed at the corrected head.
- **Committed in:** `69416c9`.

**2. [Rule 1 - Bug] Fixed offline host detection in CMake script mode after the first hosted run stopped**

- **Found during:** Task 2 corrected hosted gate.
- **Issue:** CMake script mode returned an empty `CMAKE_HOST_SYSTEM_PROCESSOR`, causing the Linux offline installer helper to refuse the valid host. The plan's red-run stop was held until the owner replied yes to the bounded helper fix and local/hosted retest.
- **Fix:** Query and validate `uname -s`/`uname -m`, map only supported host pairs, and add actual script-mode regression controls. The registered offline driver now also runs all installer refusal cases and checks controlled-failure propagation.
- **Verification:** Reproduced `unsupported-native-host` in the pinned Linux arm64 environment before the fix; after it, host controls, all 21 installer cases, and the full diagnostics workflow passed on macOS and pinned Linux arm64. Corrected hosted Linux x64 and macOS arm64 jobs plus `CI required` then passed.
- **Files modified:** `tests/gates/toolchain_case.cmake`, `tests/toolchain.cmake`.
- **Committed in:** `2a18d8e`.

**3. [Rule 2 - Missing critical validation] Require exact unique hosted output rows before hash checks**

- **Found during:** Internal review of hosted receipt controls.
- **Issue:** Comparing the declared output names alone could accept duplicate or mismatched rows in the actual output array.
- **Fix:** Validate the output row-name set exactly and keep the local counterpart directory/provenance explicit; exercise duplicate, missing, and extra-row failures.
- **Verification:** Offline controls passed on local macOS, pinned Linux arm64, and both hosted diagnostics jobs; the real hosted-proof selector passed against the Linux x64 receipt.
- **Files modified:** `tests/gates/toolchain_spike.cmake`.
- **Committed in:** `7512398`, `444f034`.

**Total deviations:** 3 correctness/validation adjustments, all tied to the required CI proof. No timeout, dependency, cache, permissions, or artifact-upload expansion was made.

## TDD Gate Compliance

- **Task 1 RED/GREEN:** The failing diagnostics-preset contract was committed in `9263a31` before the workflow implementation in `8443dc4`; the native and hosted diagnostics workflows pass the complete four-test inventory and post-test check.
- **Task 2 RED/GREEN:** Routing controls were committed in `d896084` before the required diagnostics routing in `69416c9`; the expected missing-job control, 17-case needs suite, real hosted receipt verifier, and corrected hosted required result all pass.
- **Authorized correction RED/GREEN:** The Linux arm64 script-mode helper reproduced `unsupported-native-host` before `2a18d8e`; the explicit host detection controls and bounded local/hosted retests pass after the fix.

## Known Stubs

None found in the files changed for this plan.

## Threat Flags

None. The hosted workflow uses the planned pinned actions and toolchain path with existing permissions and limits; it uploads no ROMs or toolchain binaries and records no credentials or signed download URLs.

## Issues Encountered

- The first hosted run was red. Work stopped under the established rule; the owner then explicitly authorized the narrow helper correction and bounded retest. Its failed job remains historical, while the corrected run is the current green proof.
- The Linux x64 hosted fetch step took 81s and macOS arm64 took 130s. Receipts expose only 4s/3s and 3s/3s transfer/extraction portions respectively. No unmeasured cache or timeout change was justified.
- The canonical `state.update-progress` command declined to rewrite the aggregate progress bar because milestone phase scope resolved as `unscoped`; it left that field unchanged. The Roadmap plan row was updated to 3/25 and STATE records the next plan pointer and 41 completed-plan count. The current 67% phase-level bar remains 4/6 completed phases.
- No authentication gates occurred. No local BIOS, private ROM, or ROM artifact upload was used.

## User Setup Required

None. No additional credentials or external service configuration are required for this plan closeout.

## Next Phase Readiness

Plan 03's hosted gate and current stop are complete. The parent orchestrator owns phase-level validation and the existing draft PR; Plan 04 was not started here. The phase-wide requirements and any digest-admission or CPU-expansion gate remain pending.

## Self-Check: PASSED

- The summary, updated spike report, and resolved CI handoff exist; obsolete `.continue-here.md` is absent.
- All nine plan source commits are ancestors of tested source head `2a18d8e1b2f7d24b11b40627e697cfd42b60c48b`.
- The cited native and hosted workflow logs, raw hosted run/job snapshots, hosted receipt, and local counterpart provenance files exist.
