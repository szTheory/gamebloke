---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-14T17:45:37Z
depth: deep
files_reviewed: 11
files_reviewed_list:
  - .github/workflows/ci.yml
  - cmake/CiJobBudgets.cmake
  - cmake/CiJobBudgets.json
  - cmake/CiPreflight.cmake
  - tests/ci/preflight_cases.cmake
  - tests/ci/job_budget_cases.py
  - tests/ci/admitted_hosted_cases.py
  - docs/verify.md
  - tests/gates/final_diagnostic_proof.cmake
  - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-DECISIONS.md
  - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-LINUX-NATIVE-BUDGET240.md
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 05: Linux Native Budget 240 Review

**Reviewed:** 2026-11-14T17:45:37Z
**Depth:** deep
**Files Reviewed:** 11
**Status:** clean

## Summary

The initial blocker is closed by a bounded test-only correction. The candidate hashes for the eight scoped implementation, test, and guide files and the updated boundary test match the supplied snapshot at base `2fb31ffbb36111d3b90e7b48bcc40779dd18f712`. The policy remains the owner-approved 240-second exception for the Linux native build only: the two Mac exceptions remain 300 seconds and the other eight jobs remain 180 seconds. Producer/input pins and workflow actions, permissions, needs, and upload paths remain unchanged.

The corrected boundary control derives the accepted timestamp and its one-second over-limit witness from each job's exact budget. Its registered `diagnostics.final.budget` test passed in 3.26 seconds, with preflight confirming the producer bytes, modes, ancestry, and exact 11-job policy. The actual old failure is preserved in `build/phase05-linux-native-budget240/boundary-before.log`. The earlier native CI receipt is green at 218.60 seconds but predates this test-only correction; no full current corpus/CI rerun or hosted result is claimed.

## Closed Finding History

### CR-01 (BLOCKER, resolved): The 240-second allowance made the recurring final-budget control fail

**File:** `cmake/CiJobBudgets.json:11` (original control: `tests/gates/final_diagnostic_proof.cmake:648-660`; corrected branch: `tests/gates/final_diagnostic_proof.cmake:645-670`; registered at `tests/toolchain.cmake:673-678`)

**Issue:** The newly approved native row is 240 seconds, but `final-budget-controls` chooses a 180-second endpoint for every limit other than 300. It then adds one second and expects `final-budget-check` to refuse that 181-second duration. `proof_budget` correctly accepts elapsed time up to the policy limit, so 181 seconds is valid for the native row; the child command returns success and the parent control raises `budget-control-missed`. This test is registered as `diagnostics.final.budget` and appears in the diagnostics test inventory, so the policy change causes the recurring CTest gate to fail even though the policy parser controls pass.

**Fix applied:** Derive the accepted endpoint from each job's exact `limit`, then check that endpoint succeeds and endpoint plus one second is refused. The malformed-time and unknown-job refusal controls remain, and the success message now describes the policy without hard-coding only 180/300-second limits.

**Closure evidence:** The source diff in `tests/gates/final_diagnostic_proof.cmake` is limited to the `final-budget-controls` test branch. The post-correction registered CTest receipt is `build/phase05-linux-native-budget240/budget-registered.xml`: one test, zero failures, 3.26 seconds, including `ci-preflight: 11 exact job budgets; producer bytes/modes/ancestry verified` and the success result from all 11 exact-boundary/+1-second refusal controls. The updated `candidate-hashes.json` binds this correction and the previously reviewed policy files. This closes the finding within the bounded scope.

## Current Findings

None.

## Review Limits

The actual focused setup/output receipt and current admitted parser controls remain distinct from synthetic parser evidence; the focused configure/build/output checks recorded 35.76 seconds. No heavy suite, shared build, or hosted run was started for this closure review. The authorized one-attempt hosted allowance remains unused. The Linux ARM64 local duration remains above 180 seconds and is not evidence of hosted x64 behavior. The review does not claim a full post-correction CI run or hosted qualification.

---

_Reviewed: 2026-11-14T17:45:37Z_
_Reviewer: gsd-code-reviewer agent_
_Depth: deep_
