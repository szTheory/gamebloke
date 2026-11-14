---
schema_version: 1
open_count: 3
waived_count: 0
fixed_count: 12
total_count: 15
last_updated: 2026-11-14T05:47:31.286Z
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
  }
]
````
