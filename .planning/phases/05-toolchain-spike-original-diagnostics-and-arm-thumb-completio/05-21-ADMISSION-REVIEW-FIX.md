---
phase: 05
fixed_at: 2026-11-13T19:33:45Z
review_path: .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-21-ADMISSION-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
commits:
  - 0ced20f
  - 6ffac6e
---

# Phase 05: Admission Review Fix Report

**Fixed at:** 2026-11-13T19:33:45Z
**Source review:** [05-21-ADMISSION-REVIEW.md](05-21-ADMISSION-REVIEW.md)
**Iteration:** 1

All three findings were fixed and committed. The producer input list remained unchanged.

## Fixed issues

### CR-01: Admission copies fixture outputs through an escaping symlink

**Commit:** `0ced20f`
**Files:** [PathContainment.cmake](../../../cmake/PathContainment.cmake), [diagnostic_admission.cmake](../../../tests/gates/diagnostic_admission.cmake), [diagnostic_admission_controls.cmake](../../../tests/gates/diagnostic_admission_controls.cmake)

Admission now resolves the binary root and each destination before creating directories or copying. It checks every selected artifact path before any artifact copy, stages files under the resolved root, then renames them into place. Result logs, result JSON, and summary output use the same containment checks. Stale generated indexes are safely removed at admission start.

The controls exercise nested-directory, final-file, and dangling symlink escapes with synthetic external sentinels. They verify that sentinels remain byte-identical, a dangling target is not created, and an ordinary contained copy succeeds. The production `admitted-pair` path was also exercised against a nested escape with stale index files present.

### CR-02: An in-tree fixture path can hash a file outside the source tree

**Commit:** `0ced20f`
**Files:** [GenerateFixtureIndex.cmake](../../../cmake/GenerateFixtureIndex.cmake), [artifact_case.cmake](../../../tests/gates/artifact_case.cmake), [CMakeLists.txt](../../../CMakeLists.txt)

Index generation resolves the source root and candidate file before hashing and refuses a resolved path outside the root. The manifest contract defines an in-tree source by relative path and content SHA, so a contained symlink alias remains valid; a clean alias companion verifies that behavior. The escaping-parent-symlink control also seeds stale indexes and verifies that refusal removes them.

### WR-01: Git mode changes can pass the diagnostic authority check

**Commit:** `6ffac6e`
**Files:** [CheckDiagnosticAuthority.cmake](../../../cmake/CheckDiagnosticAuthority.cmake), [diagnostic_admission_controls.cmake](../../../tests/gates/diagnostic_admission_controls.cmake)

Both producer-path comparisons now invoke Git with per-command `-c core.filemode=true`; repository configuration is not changed. The control uses a disposable local clone with `core.filemode=false`, confirms `scripts/fetch-toolchain.sh` starts executable, removes its execute bit, and requires `stale-diagnostic-source-input`. Clean and restored-mode companions pass. The control is registered in the hygiene lane inventory.

## Verification

### Red reproductions before the fixes

1. **CR-01**

   ```sh
   cmake -DCASE=admitted-pair -DSRC=. -DBIN=build/phase05-spike/admission-review-red -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake
   ```

   With the output directory symlinked to a disposable sibling directory, the pre-fix command copied the admitted ROM through the symlink before the index generator later reported `fixture-index: artifact-path-escape`. The external sentinel changed from the synthetic control-image SHA-256 `05db906e94d35bbe0b90e100555fefb8b8d8fe99175601286798c22517d1c42f` to the admitted image SHA-256 `72cc9a6e508ac4bd4a1bbbdb9dd9fd65bfbce5809faa65f8eb580841c694fe95`.

2. **CR-02**

   ```sh
   cmake -DCASE=compatibility-and-controls -DSRC=. -DBIN=build/ci -P tests/gates/artifact_case.cmake
   ```

   Before the fix, the new external-parent-symlink control failed because the generator returned `fixture-index: ok` for the synthetic external file and replaced the seeded stale indexes.

3. **WR-01**

   ```sh
   cmake -DCASE=admission-controls -DSRC=. -DBIN=build/ci -P tests/gates/artifact_case.cmake
   ```

   Before the fix, the disposable clone had `core.filemode=false`; the tracked executable was changed to non-executable mode, but generation returned `fixture-index: ok` and the control failed with `filemode-drift-was-not-refused`.

### Passing focused checks

- `cmake --preset ci` — configure and generate succeeded.
- `cmake -DCASE=compatibility-and-controls -DSRC=. -DBIN=build/ci -P tests/gates/artifact_case.cmake` — passed, including external in-tree-source refusal, stale-index removal, and a contained-alias clean companion.
- `cmake -DCASE=admission-controls -DSRC=. -DBIN=build/ci -P tests/gates/artifact_case.cmake` — passed the existing source/output/pin/index controls and clean runtime companions.
- `cmake -DCASE=admitted-pair -DSRC=. -DBIN=build/ci -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake` — passed both admitted results: one positive and one assertion-broken result.
- `cmake -DCASE=path-controls -DSRC=. -DBIN=build/ci -P tests/gates/diagnostic_admission_controls.cmake` — passed nested, final, and dangling symlink refusal controls; contained-copy and file-mode clean companions passed.
- `cmake --preset hygiene` — configure and generate succeeded.
- `ctest --preset hygiene -R '^artifact\.admission\.controls$'` — passed 1/1 registered tests. This selector covers CR-01, CR-02, and WR-01 with synthetic controls and no emulator build.
- `git diff --check` — passed before commits.

All checks ran in the main checkout because `workflow.use_worktrees=false`; synthetic files and generated outputs remained under ignored `build/` paths. The full canonical CI workflow and `all-admitted` matrix were not rerun. The known canonical CI critical path is 253.63 seconds, and the requested focused checks covered the changed boundaries without repeating that run. No network, GUI, private ROM, external personal file, or producer-input change was involved.
