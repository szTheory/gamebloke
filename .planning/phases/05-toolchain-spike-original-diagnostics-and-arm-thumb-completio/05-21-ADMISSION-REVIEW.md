---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T20:01:29Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - CMakeLists.txt
  - cmake/CheckDiagnosticAuthority.cmake
  - cmake/CheckManifest.cmake
  - cmake/ExpectFail.cmake
  - cmake/GenerateFixtureIndex.cmake
  - cmake/PathContainment.cmake
  - tests/fixtures/manifest.json
  - tests/gates/artifact_case.cmake
  - tests/gates/diagnostic_admission.cmake
  - tests/gates/diagnostic_admission_controls.cmake
  - tests/gates/diagnostics_case.cmake
  - tests/gates.cmake
  - tests/gates/json_case.cmake
  - tests/inventory/hygiene.txt
  - tests/gates/runner_metadata.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05 Plan 21 Task 2: Code Review Report

**Reviewed:** 2026-11-13T20:01:29Z
**Depth:** standard
**Files Reviewed:** 15
**Status:** clean

## Summary

Re-reviewed the admission fixes, the consumer-only strict-loader runtime correction, and the final output-containment change. All four previously reported findings are resolved in the current source and have focused controls. The output guard requires an independently supplied absolute root, preflights all three output destinations before mutation, and routes stale-header removal, directory creation, and writes through the containment helpers (`GenerateFixtureIndex.cmake:10-20,57-99,425`). All direct generator callers now pass an output root. The new control source covers parent, final-file, and dangling symlinks for each output, plus a clean contained-output companion (`artifact_case.cmake:127-183`). The linked report records the passing focused controls and CI run; I reviewed those receipts but did not rerun them. All reviewed files meet quality standards; no active issues remain.

## Narrative Findings (AI reviewer)

No active findings.

## Resolved Findings (historical)

- **CR-01 — resolved in `0ced20f`.** `diagnostic_admission.cmake` now preflights selected output paths, copies through `gbl_copy_file_contained`, and uses contained writes/removals for result artifacts. `diagnostic_admission_controls.cmake` covers nested-directory, final-file, and dangling-symlink escapes with external sentinels plus a clean contained copy. The focused production admitted-pair path is recorded in [05-21-ADMISSION-REVIEW-FIX.md](05-21-ADMISSION-REVIEW-FIX.md).
- **CR-02 — resolved in `0ced20f`.** `GenerateFixtureIndex.cmake` now resolves in-tree source paths beneath the canonical source root before hashing. `artifact_case.cmake` covers an escaping parent symlink, stale-index cleanup, an unchanged external sentinel, and a contained symlink alias that remains admissible.
- **WR-01 — resolved in `6ffac6e`.** Both Git comparisons force `core.filemode=true` per command. A disposable clone configured with `core.filemode=false` verifies executable-mode drift is refused, then restores the mode and verifies the clean companion. The focused control is registered in the hygiene lane.

- **CR-03 — resolved in `fa62cbf`.** `GenerateFixtureIndex.cmake` now requires an explicit absolute `GBL_OUTPUT_ROOT`, checks the production header, control header, and control-manifest destinations against it before output mutations, and uses `PathContainment.cmake` for stale-header removal, output-directory creation, and every write. The root is supplied by the CMake build rule and every direct test caller. `artifact_case.cmake` exercises parent, final-file, and dangling symlinks for each of the three outputs, checks outside sentinels remain unchanged or absent, and verifies ordinary contained output generation. See [05-21-OUTPUT-CONTAINMENT.md](05-21-OUTPUT-CONTAINMENT.md). I reviewed the changed control flow and control definitions; the report records the passing control run and fresh canonical CI result, which I did not rerun.

## Consumer Performance Correction Reviewed

Reviewed `cmake/ExpectFail.cmake`, `tests/gates/json_case.cmake`, the CTest registration, and hygiene inventory changes alongside [05-21-ADMISSION-PERFORMANCE.md](05-21-ADMISSION-PERFORMANCE.md). The paired `NO_ECHO` check captures one child execution, checks for the marker before printing child output, and then enforces the failure status and reason. Its meta-controls cover leakage, unexpected success, a wrong reason, and exactly one clean-companion execution. The trailing-content case still generates seven real loader checks for every document plus the tiny base; only those read-only child checks run in a nested three-worker CTest pool. The parent requires the exact JUnit test count, zero failures, and no skips. Other project tests remain on the existing serial path. I found no additional defect in this correction.

---

_Reviewed: 2026-11-13T20:01:29Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## First-pass resolution evidence (historical)

The first three findings were addressed in commits `0ced20f` and `6ffac6e`; CR-03 was addressed in `fa62cbf`. Their reproductions, passing controls, observed outcomes, and verification limits are recorded in [05-21-ADMISSION-REVIEW-FIX.md](05-21-ADMISSION-REVIEW-FIX.md) and [05-21-OUTPUT-CONTAINMENT.md](05-21-OUTPUT-CONTAINMENT.md). No active findings remain.
