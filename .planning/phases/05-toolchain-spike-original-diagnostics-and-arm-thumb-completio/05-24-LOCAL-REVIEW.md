---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-14T06:33:05Z
depth: standard
files_reviewed: 18
files_reviewed_list:
  - CMakePresets.json
  - tests/toolchain.cmake
  - cmake/CiPreflight.cmake
  - tests/ci/admission_cases.cmake
  - tests/ci/preflight_cases.cmake
  - .github/workflows/ci.yml
  - scripts/check-workflow.sh
  - tests/ci/ci_cases.cmake
  - tests/ci/history_cases.cmake
  - tests/gates/diagnostic_dependency.cmake
  - tests/gates/diagnostic_hosted_log.cmake
  - scripts/collect-hosted-proof.py
  - tests/gates/final_diagnostic_proof.cmake
  - tests/gates/admitted_hosted_proof.cmake
  - tests/ci/admitted_hosted_cases.py
  - tests/ci/job_budget_cases.py
  - scripts/linux-lane.sh
  - docs/verify.md
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05 Plan 24: Local Candidate Review

**Reviewed:** 2026-11-14T06:33:05Z
**Depth:** standard
**Files Reviewed:** 18
**Status:** clean

## Summary

The focused re-review covered the original WR-01 and IN-01 against the current collector, local control driver, compatibility controls, workflow comment, and fix evidence. Both findings are resolved; there are no active findings. The warning and info entries below remain as review history, and the frontmatter counts only active findings.

## Warnings

### WR-01: Synthetic isolation accepts external symlink aliases — RESOLVED

**File:** `scripts/collect-hosted-proof.py:119-122`
**Classification:** WARNING
**Status:** RESOLVED
**Issue:** In synthetic mode, the collector chooses a marked scope from the resolved output path and checks that the resolved source and input targets are inside it. It does not check the original paths supplied for `source_root`, `output`, inputs, or logs. `collect()` also resolves `source_root` before passing it to this check at line 124. A focused reproduction passed a `source_root` symlink whose lexical path was outside the marked build tree but whose target was inside it. This bypasses the stated rule that synthetic source/output/input/log paths, including their aliases, must be contained by the marked disposable tree. The explicit synthetic flag and tagged documents still prevent that receipt from qualifying as live evidence.
**Fix:** Preserve the original paths and require both each normalized absolute path and its resolved target to be within the same marked build scope before reading or writing. Add synthetic-mode refusal controls for source, output, document, and log symlink aliases.
**Resolution:** The collector now checks the normalized original path and resolved target for `source_root`, output, all three API documents, and all four logs against one marked scope. `collect()` performs this path check before reading the API documents, then validates their synthetic tags after parsing. The live marked-ancestor refusal remains intact.
**Re-review evidence:** The unmodified pre-fix driver failed on `external-alias-source_root` (`build/phase05-plan24/review-alias-red.log`). The current driver rejects all nine source/output/document/log aliases, and the actual admitted local log plus collector/CMake clean and refusal controls pass in 42.37 seconds (`build/phase05-plan24/review-alias-green-2.log`). Legacy producer-only source, collector, and isolation controls pass (`build/phase05-plan24/review-legacy-green.log`). The first post-fix harness run stopped on an expected-reason mismatch after the earlier path refusal; updating that expected reason did not weaken the control, and the corrected run passes. The current source hash matches `build/phase05-plan24/review-fix-file-hashes.json`.

## Info

### IN-01: Workflow comments still describe pre-admission and shallow jobs — RESOLVED

**File:** `.github/workflows/ci.yml:142-144`
**Classification:** INFO
**Status:** RESOLVED
**Issue:** The comment says the diagnostics job runs “pre-admission” proof and that other lanes stay shallow. This candidate switches diagnostics to admitted execution and adds full-history checkouts to build and ASan as well, so the comment now misstates what the hosted workflow proves and which jobs fetch full history.
**Fix:** Update the comment to describe admitted diagnostics and list the build, ASan, and diagnostics jobs as full-history lanes.
**Resolution:** The comment now describes admitted smoke/ARM/Thumb execution, local-only ROMs, and full-history producer checks for build, ASan, and diagnostics. Current workflow lines 142-144 match that behavior.
**Re-review evidence:** The updated workflow comment is visible in the current source diff and its content hash matches `build/phase05-plan24/review-fix-file-hashes.json`.

---

_Reviewed: 2026-11-14T06:33:05Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
