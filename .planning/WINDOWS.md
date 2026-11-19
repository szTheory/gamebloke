---
schema_version: 1
open_count: 5
waived_count: 0
fixed_count: 25
total_count: 30
last_updated: 2026-11-19T04:52:36.486Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 04 | stub | include/gamebloke/gamebloke.h | 232 | Bus accesses retain an explicit placeholder master-cycle cost; plan 04-03 uses that project policy and does not claim hardware timing accuracy. | open |  | 2026-11-11T04:51:53.172Z |  |
| 2 | 04 | stub | profiles/agb.json | 129 | The current-latch response for absent BIOS, unsupported I/O, and ROM tails remains an unverified placeholder; no hardware behavior is claimed. | open |  | 2026-11-11T05:59:19.341Z |  |
| 3 | 04 | skipped-test | tests/gates.cmake | 88 | signature.owner_local skipped because GBL_OWNER_LOGO_FILE was unset; the optional owner-local cross-check remains unrun. | fixed |  | 2026-11-11T05:59:19.431Z | 2026-11-11T06:05:21.279Z |
| 4 | 04 | deviation | src/inspect.c |  | Plan 04-06 core include verification rejected string.h in inspection code. Replaced memcpy with an original bounded byte-copy helper; focused inspection, core include, canonical CI and ASan/UBSan checks pass. | fixed |  | 2026-11-11T06:38:11.723Z | 2026-11-11T06:38:11.804Z |
| 5 | 04 | deviation | tests/hash_test.c | 95 | Corrected a malformed NIST long-message test vector; the final standard-vector, strict-warning and ASan/UBSan checks pass. | fixed |  | 2026-11-11T07:52:28.871Z | 2026-11-11T07:52:28.943Z |
| 6 | 04 | deviation | .planning/phases/04-cpu-execution-core-and-headless-runner/04-08-SUMMARY.md | 85 | Replaced the machine-specific GSD command path in the hash summary with a portable HOME reference. Repository scans and the canonical CI workflow including JUnit inventory pass on the corrected tree. | fixed |  | 2026-11-11T08:26:24.848Z | 2026-11-11T08:26:24.917Z |
| 7 | 04 | deviation | runner/main.c |  | Preserved the existing help exit-code text while documenting valid non-PASS run exit status 1. Existing help gates and current CLI, sanitizer and canonical CI checks pass. | fixed |  | 2026-11-11T09:07:11.761Z | 2026-11-11T09:07:11.828Z |
| 8 | 04 | deviation | tests/gates/runner_metadata.cmake |  | Updated the existing metadata wiring control to follow the new shared real producer source list; source-equivalence controls and canonical CI pass. | fixed |  | 2026-11-11T09:07:11.895Z | 2026-11-11T09:07:11.963Z |
| 9 | 04 | deviation | tests/gates/runner_case.cmake |  | Five external no-BIOS stop kinds are not CLI-reachable under direct-start. All five were verified through the existing CPU exception API and shared production runner classification/JSON path; no CLI triggers, interrupt source or abort model was added. | fixed |  | 2026-11-11T10:04:01.038Z | 2026-11-11T10:04:01.118Z |
| 10 | 04 | deviation | cmake/CheckResult.cmake |  | Moved trace-output-full and incomplete-trace consistency checks out of status-branch chaining so all applicable statuses reach the checks. Regression controls, actual producer checks, canonical CI and focused sanitizers pass. | fixed |  | 2026-11-11T10:46:06.440Z | 2026-11-11T10:46:06.510Z |
| 11 | 05 | deviation | scripts/fetch-toolchain.sh |  | Host-row URL validation now refuses a cross-host archive URL before curl; the regression control and offline matrix pass. | fixed |  | 2026-11-11T17:07:01.463Z | 2026-11-11T17:07:05.294Z |
| 12 | 05 | deviation | .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-19-SUMMARY.md |  | Plan19 original formal TDD record is incomplete: preserved original RED log is empty, no classifier evidence exists, and task2 has no separate RED commit. Fresh semantic/trace controls pass but cannot reconstruct historical compliance. Keep this process deviation visible for the existing Phase5 acceptance boundary. | open |  | 2026-11-13T05:37:03.666Z |  |
| 13 | 05 | deviation | cmake/GenerateFixtureIndex.cmake |  | Plan21 independent review found admission-copy, in-tree source and generated-index output escapes plus config-hidden producer mode drift. All four fixed with actual negative and clean controls; fresh canonical CI, all722 production fixtures and refusal controls pass, and independent re-review has zero active findings. See05-21-ADMISSION-REVIEW.md and05-21-OUTPUT-CONTAINMENT.md. | fixed |  | 2026-11-13T20:04:56.192Z | 2026-11-13T20:04:56.268Z |
| 14 | 05 | deviation | scripts/collect-hosted-proof.py |  | Plan23 budget-policy review found mixed synthetic tags and incomplete marked-ancestor isolation at collector/final-gate boundaries. Focused fixes and source/collector controls pass; independent final re-review pending. No live qualification or hosted attempt occurred. See05-23-JOB-BUDGET-FIX.md. | fixed |  | 2026-11-14T04:43:08.633Z | 2026-11-14T05:02:20.575Z |
| 15 | 05 | deviation | tests/toolchain.cmake, tests/gates/diagnostic_setup.cmake |  | Admitted integration exposed proof-only registration, ISA output path and API producer-receipt ordering defects. Corrected consumer-only; full admitted lane, canonical CI, focused sanitizer/refusal checks passed and independent review clean. Evidence:05-23-ADMITTED-EXECUTION.md. | fixed |  | 2026-11-14T05:47:21.776Z | 2026-11-14T05:47:31.286Z |
| 16 | 05 | deviation | scripts/collect-hosted-proof.py |  | Plan24 review reproduced synthetic path alias outside disposable marked scope. Original normalized and resolved source/output/document/log paths now all required inside same scope before API reads. All9alias negatives, admitted and legacy controls pass; re-review clean. See05-24-REVIEW-FIX.md. | fixed |  | 2026-11-14T06:35:00.881Z | 2026-11-14T06:35:00.984Z |
| 17 | 05 | deviation | tests/gates/diagnostic_setup.cmake |  | Plan25 actual full sanitizer exposed post-admission runner relink after receipt hash capture. Bound final executable after rebuild, retained stale-runner refusal and SDK RED/green. Fresh full sanitizer, current required diagnostics and independent code/security reviews pass. Safe Linux export privacy/mode/trap fixes verified; no producer change. See05-25-LOCAL-EXECUTION.md. | fixed |  | 2026-11-14T08:51:09.846Z | 2026-11-14T08:51:09.945Z |
| 18 | 05 | deviation | tests/gates/linux_snapshot.cmake |  | Local Nyquist gap: ignored snapshot prototype was not recurring and required installed semantics absent in diagnostic CI. Added offline multi-commit privacy/history/mode/signal/cleanup regression and required ARM/all relocation; fresh Mac/Linux/current inventories pass, independent rereviews clear. See05-25-COVERAGE-FIX.md. | fixed |  | 2026-11-14T08:51:10.037Z | 2026-11-14T08:51:10.137Z |
| 19 | 05 | deviation | 05-25-LOCAL-EXECUTION.md |  | Historical scheduling deviation: one isolated standalone ledger sweep briefly overlapped required diagnostics. Both completed in distinct roots; the overlap is recorded, not relabeled serial. All subsequent heavy work was serial. No resource cap changed, no throughput claim relies on that overlap, no persistent state failure remains. | fixed |  | 2026-11-14T08:51:10.231Z | 2026-11-14T08:51:10.323Z |
| 20 | 07 | deviation | runner/run.c |  | Added runner handling for the new internal-error and time-refusal CPU outcomes; strict build and workflow pass. | fixed |  | 2026-11-16T06:17:27.450Z | 2026-11-16T06:17:36.546Z |
| 21 | 07 | deviation | tests/gates/core_mutation.cmake |  | Moved the clock-overflow mutation to the checked addition in machine.c; its negative control passes. | fixed |  | 2026-11-16T06:18:15.494Z | 2026-11-16T06:18:15.660Z |
| 22 | 07 | deviation | tests/state_check.h |  | Made the new shared time-preview comparison inline to satisfy strict unused-function warnings. | fixed |  | 2026-11-16T06:18:15.825Z | 2026-11-16T06:18:16.003Z |
| 23 | 07 | deviation | .planning/phases/07-time-authority-and-timer-0-polling-proof/07-02-01-red-evidence.json |  | Replaced absolute JUnit report locations in both RED evidence commands with repository-relative paths. | fixed |  | 2026-11-16T06:18:16.167Z | 2026-11-16T06:18:16.334Z |
| 24 | 07 | deviation | src/machine.c |  | Ordered time preparation now advances by the shared quoted bus-operation cycle count instead of an independent fixed-one-cycle literal; canonical workflow passes. | fixed |  | 2026-11-16T07:56:10.142Z | 2026-11-16T07:56:39.776Z |
| 25 | 07 | deviation | src/cpu.c |  | Exception units now use the shared calculator for their internal-cycle attribution; determinism control and canonical workflow pass. | fixed |  | 2026-11-16T07:56:14.967Z | 2026-11-16T07:56:39.863Z |
| 26 | 07 | skipped-test | tests/diagnostics.cmake | 37 | The local full workflow skipped 726 ARM/Thumb diagnostic cases because arm-toolchain-absent; DIAG-04 remains open. | open |  | 2026-11-16T13:37:35.583Z |  |
| 27 | 8 | deviation | tests/determinism_test.c |  | Converted stale positional run-result initializers so the Phase 8 determinism target builds with warnings as errors. | fixed |  | 2026-11-17T00:28:03.040Z | 2026-11-17T00:28:45.212Z |
| 28 | 10 | deviation | src/cpu.c |  | Added PPU boundaries to the CPU live-event scan so a masked PPU IF request wakes HALT without becoming IRQ acceptance. | fixed |  | 2026-11-18T19:15:57.608Z | 2026-11-18T19:16:06.312Z |
| 29 | 10 | deviation | tests/bus_test.c |  | Corrected stale PPU/video fixtures for full DISPCNT preservation, cycle-zero supported sampling, VBlank/HBlank masks, and the new capture-reason baseline; focused selectors pass. | fixed |  | 2026-11-18T19:16:03.766Z | 2026-11-18T19:16:06.421Z |
| 30 | 10 | unrun-verify | scripts/play-playable0-local.sh |  | Installed RetroArch smoke could not start because managed macOS LaunchServices returned kLSNoLaunchPermissionErr; owner trial remains pending until an interactive local host is available. | open |  | 2026-11-19T04:52:36.486Z |  |

````json
[
  {
    "id": 1,
    "kind": "stub",
    "phase": "04",
    "file": "include/gamebloke/gamebloke.h",
    "line": 232,
    "description": "Bus accesses retain an explicit placeholder master-cycle cost; plan 04-03 uses that project policy and does not claim hardware timing accuracy.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-11T04:51:53.172Z",
    "resolved_at": null,
    "milestone": "v1.0"
  },
  {
    "id": 2,
    "kind": "stub",
    "phase": "04",
    "file": "profiles/agb.json",
    "line": 129,
    "description": "The current-latch response for absent BIOS, unsupported I/O, and ROM tails remains an unverified placeholder; no hardware behavior is claimed.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-11T05:59:19.341Z",
    "resolved_at": null,
    "milestone": "v1.0"
  },
  {
    "id": 3,
    "kind": "skipped-test",
    "phase": "04",
    "file": "tests/gates.cmake",
    "line": 88,
    "description": "signature.owner_local skipped because GBL_OWNER_LOGO_FILE was unset; the optional owner-local cross-check remains unrun.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T05:59:19.431Z",
    "resolved_at": "2026-11-11T06:05:21.279Z",
    "milestone": "v1.0"
  },
  {
    "id": 4,
    "kind": "deviation",
    "phase": "04",
    "file": "src/inspect.c",
    "line": null,
    "description": "Plan 04-06 core include verification rejected string.h in inspection code. Replaced memcpy with an original bounded byte-copy helper; focused inspection, core include, canonical CI and ASan/UBSan checks pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T06:38:11.723Z",
    "resolved_at": "2026-11-11T06:38:11.804Z",
    "milestone": "v1.0"
  },
  {
    "id": 5,
    "kind": "deviation",
    "phase": "04",
    "file": "tests/hash_test.c",
    "line": 95,
    "description": "Corrected a malformed NIST long-message test vector; the final standard-vector, strict-warning and ASan/UBSan checks pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T07:52:28.871Z",
    "resolved_at": "2026-11-11T07:52:28.943Z",
    "milestone": "v1.0"
  },
  {
    "id": 6,
    "kind": "deviation",
    "phase": "04",
    "file": ".planning/phases/04-cpu-execution-core-and-headless-runner/04-08-SUMMARY.md",
    "line": 85,
    "description": "Replaced the machine-specific GSD command path in the hash summary with a portable HOME reference. Repository scans and the canonical CI workflow including JUnit inventory pass on the corrected tree.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T08:26:24.848Z",
    "resolved_at": "2026-11-11T08:26:24.917Z",
    "milestone": "v1.0"
  },
  {
    "id": 7,
    "kind": "deviation",
    "phase": "04",
    "file": "runner/main.c",
    "line": null,
    "description": "Preserved the existing help exit-code text while documenting valid non-PASS run exit status 1. Existing help gates and current CLI, sanitizer and canonical CI checks pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T09:07:11.761Z",
    "resolved_at": "2026-11-11T09:07:11.828Z",
    "milestone": "v1.0"
  },
  {
    "id": 8,
    "kind": "deviation",
    "phase": "04",
    "file": "tests/gates/runner_metadata.cmake",
    "line": null,
    "description": "Updated the existing metadata wiring control to follow the new shared real producer source list; source-equivalence controls and canonical CI pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T09:07:11.895Z",
    "resolved_at": "2026-11-11T09:07:11.963Z",
    "milestone": "v1.0"
  },
  {
    "id": 9,
    "kind": "deviation",
    "phase": "04",
    "file": "tests/gates/runner_case.cmake",
    "line": null,
    "description": "Five external no-BIOS stop kinds are not CLI-reachable under direct-start. All five were verified through the existing CPU exception API and shared production runner classification/JSON path; no CLI triggers, interrupt source or abort model was added.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T10:04:01.038Z",
    "resolved_at": "2026-11-11T10:04:01.118Z",
    "milestone": "v1.0"
  },
  {
    "id": 10,
    "kind": "deviation",
    "phase": "04",
    "file": "cmake/CheckResult.cmake",
    "line": null,
    "description": "Moved trace-output-full and incomplete-trace consistency checks out of status-branch chaining so all applicable statuses reach the checks. Regression controls, actual producer checks, canonical CI and focused sanitizers pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T10:46:06.440Z",
    "resolved_at": "2026-11-11T10:46:06.510Z",
    "milestone": "v1.0"
  },
  {
    "id": 11,
    "kind": "deviation",
    "phase": "05",
    "file": "scripts/fetch-toolchain.sh",
    "line": null,
    "description": "Host-row URL validation now refuses a cross-host archive URL before curl; the regression control and offline matrix pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-11T17:07:01.463Z",
    "resolved_at": "2026-11-11T17:07:05.294Z",
    "milestone": "v1.0"
  },
  {
    "id": 12,
    "kind": "deviation",
    "phase": "05",
    "file": ".planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-19-SUMMARY.md",
    "line": null,
    "description": "Plan19 original formal TDD record is incomplete: preserved original RED log is empty, no classifier evidence exists, and task2 has no separate RED commit. Fresh semantic/trace controls pass but cannot reconstruct historical compliance. Keep this process deviation visible for the existing Phase5 acceptance boundary.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-13T05:37:03.666Z",
    "resolved_at": null,
    "milestone": "v1.0"
  },
  {
    "id": 13,
    "kind": "deviation",
    "phase": "05",
    "file": "cmake/GenerateFixtureIndex.cmake",
    "line": null,
    "description": "Plan21 independent review found admission-copy, in-tree source and generated-index output escapes plus config-hidden producer mode drift. All four fixed with actual negative and clean controls; fresh canonical CI, all722 production fixtures and refusal controls pass, and independent re-review has zero active findings. See05-21-ADMISSION-REVIEW.md and05-21-OUTPUT-CONTAINMENT.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-13T20:04:56.192Z",
    "resolved_at": "2026-11-13T20:04:56.268Z",
    "milestone": "v1.0"
  },
  {
    "id": 14,
    "kind": "deviation",
    "phase": "05",
    "file": "scripts/collect-hosted-proof.py",
    "line": null,
    "description": "Plan23 budget-policy review found mixed synthetic tags and incomplete marked-ancestor isolation at collector/final-gate boundaries. Focused fixes and source/collector controls pass; independent final re-review pending. No live qualification or hosted attempt occurred. See05-23-JOB-BUDGET-FIX.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T04:43:08.633Z",
    "resolved_at": "2026-11-14T05:02:20.575Z",
    "milestone": "v1.0"
  },
  {
    "id": 15,
    "kind": "deviation",
    "phase": "05",
    "file": "tests/toolchain.cmake, tests/gates/diagnostic_setup.cmake",
    "line": null,
    "description": "Admitted integration exposed proof-only registration, ISA output path and API producer-receipt ordering defects. Corrected consumer-only; full admitted lane, canonical CI, focused sanitizer/refusal checks passed and independent review clean. Evidence:05-23-ADMITTED-EXECUTION.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T05:47:21.776Z",
    "resolved_at": "2026-11-14T05:47:31.286Z",
    "milestone": "v1.0"
  },
  {
    "id": 16,
    "kind": "deviation",
    "phase": "05",
    "file": "scripts/collect-hosted-proof.py",
    "line": null,
    "description": "Plan24 review reproduced synthetic path alias outside disposable marked scope. Original normalized and resolved source/output/document/log paths now all required inside same scope before API reads. All9alias negatives, admitted and legacy controls pass; re-review clean. See05-24-REVIEW-FIX.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T06:35:00.881Z",
    "resolved_at": "2026-11-14T06:35:00.984Z",
    "milestone": "v1.0"
  },
  {
    "id": 17,
    "kind": "deviation",
    "phase": "05",
    "file": "tests/gates/diagnostic_setup.cmake",
    "line": null,
    "description": "Plan25 actual full sanitizer exposed post-admission runner relink after receipt hash capture. Bound final executable after rebuild, retained stale-runner refusal and SDK RED/green. Fresh full sanitizer, current required diagnostics and independent code/security reviews pass. Safe Linux export privacy/mode/trap fixes verified; no producer change. See05-25-LOCAL-EXECUTION.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T08:51:09.846Z",
    "resolved_at": "2026-11-14T08:51:09.945Z",
    "milestone": "v1.0"
  },
  {
    "id": 18,
    "kind": "deviation",
    "phase": "05",
    "file": "tests/gates/linux_snapshot.cmake",
    "line": null,
    "description": "Local Nyquist gap: ignored snapshot prototype was not recurring and required installed semantics absent in diagnostic CI. Added offline multi-commit privacy/history/mode/signal/cleanup regression and required ARM/all relocation; fresh Mac/Linux/current inventories pass, independent rereviews clear. See05-25-COVERAGE-FIX.md.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T08:51:10.037Z",
    "resolved_at": "2026-11-14T08:51:10.137Z",
    "milestone": "v1.0"
  },
  {
    "id": 19,
    "kind": "deviation",
    "phase": "05",
    "file": "05-25-LOCAL-EXECUTION.md",
    "line": null,
    "description": "Historical scheduling deviation: one isolated standalone ledger sweep briefly overlapped required diagnostics. Both completed in distinct roots; the overlap is recorded, not relabeled serial. All subsequent heavy work was serial. No resource cap changed, no throughput claim relies on that overlap, no persistent state failure remains.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-14T08:51:10.231Z",
    "resolved_at": "2026-11-14T08:51:10.323Z",
    "milestone": "v1.0"
  },
  {
    "id": 20,
    "kind": "deviation",
    "phase": "07",
    "file": "runner/run.c",
    "line": null,
    "description": "Added runner handling for the new internal-error and time-refusal CPU outcomes; strict build and workflow pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T06:17:27.450Z",
    "resolved_at": "2026-11-16T06:17:36.546Z",
    "milestone": "v2.0"
  },
  {
    "id": 21,
    "kind": "deviation",
    "phase": "07",
    "file": "tests/gates/core_mutation.cmake",
    "line": null,
    "description": "Moved the clock-overflow mutation to the checked addition in machine.c; its negative control passes.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T06:18:15.494Z",
    "resolved_at": "2026-11-16T06:18:15.660Z",
    "milestone": "v2.0"
  },
  {
    "id": 22,
    "kind": "deviation",
    "phase": "07",
    "file": "tests/state_check.h",
    "line": null,
    "description": "Made the new shared time-preview comparison inline to satisfy strict unused-function warnings.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T06:18:15.825Z",
    "resolved_at": "2026-11-16T06:18:16.003Z",
    "milestone": "v2.0"
  },
  {
    "id": 23,
    "kind": "deviation",
    "phase": "07",
    "file": ".planning/phases/07-time-authority-and-timer-0-polling-proof/07-02-01-red-evidence.json",
    "line": null,
    "description": "Replaced absolute JUnit report locations in both RED evidence commands with repository-relative paths.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T06:18:16.167Z",
    "resolved_at": "2026-11-16T06:18:16.334Z",
    "milestone": "v2.0"
  },
  {
    "id": 24,
    "kind": "deviation",
    "phase": "07",
    "file": "src/machine.c",
    "line": null,
    "description": "Ordered time preparation now advances by the shared quoted bus-operation cycle count instead of an independent fixed-one-cycle literal; canonical workflow passes.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T07:56:10.142Z",
    "resolved_at": "2026-11-16T07:56:39.776Z",
    "milestone": "v2.0"
  },
  {
    "id": 25,
    "kind": "deviation",
    "phase": "07",
    "file": "src/cpu.c",
    "line": null,
    "description": "Exception units now use the shared calculator for their internal-cycle attribution; determinism control and canonical workflow pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-16T07:56:14.967Z",
    "resolved_at": "2026-11-16T07:56:39.863Z",
    "milestone": "v2.0"
  },
  {
    "id": 26,
    "kind": "skipped-test",
    "phase": "07",
    "file": "tests/diagnostics.cmake",
    "line": 37,
    "description": "The local full workflow skipped 726 ARM/Thumb diagnostic cases because arm-toolchain-absent; DIAG-04 remains open.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-16T13:37:35.583Z",
    "resolved_at": null,
    "milestone": "v2.0"
  },
  {
    "id": 27,
    "kind": "deviation",
    "phase": "8",
    "file": "tests/determinism_test.c",
    "line": null,
    "description": "Converted stale positional run-result initializers so the Phase 8 determinism target builds with warnings as errors.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-17T00:28:03.040Z",
    "resolved_at": "2026-11-17T00:28:45.212Z",
    "milestone": "v2.0"
  },
  {
    "id": 28,
    "kind": "deviation",
    "phase": "10",
    "file": "src/cpu.c",
    "line": null,
    "description": "Added PPU boundaries to the CPU live-event scan so a masked PPU IF request wakes HALT without becoming IRQ acceptance.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-18T19:15:57.608Z",
    "resolved_at": "2026-11-18T19:16:06.312Z",
    "milestone": "v2.0"
  },
  {
    "id": 29,
    "kind": "deviation",
    "phase": "10",
    "file": "tests/bus_test.c",
    "line": null,
    "description": "Corrected stale PPU/video fixtures for full DISPCNT preservation, cycle-zero supported sampling, VBlank/HBlank masks, and the new capture-reason baseline; focused selectors pass.",
    "status": "fixed",
    "reason": "",
    "recorded_at": "2026-11-18T19:16:03.766Z",
    "resolved_at": "2026-11-18T19:16:06.421Z",
    "milestone": "v2.0"
  },
  {
    "id": 30,
    "kind": "unrun-verify",
    "phase": "10",
    "file": "scripts/play-playable0-local.sh",
    "line": null,
    "description": "Installed RetroArch smoke could not start because managed macOS LaunchServices returned kLSNoLaunchPermissionErr; owner trial remains pending until an interactive local host is available.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-11-19T04:52:36.486Z",
    "resolved_at": null,
    "milestone": "v2.0"
  }
]
````
