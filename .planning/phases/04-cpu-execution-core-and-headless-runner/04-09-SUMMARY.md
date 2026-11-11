---
phase: 04-cpu-execution-core-and-headless-runner
plan: "09"
subsystem: runner
tags: [C17, CMake, SHA-256, fixture-metadata, synthetic-fixtures]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: "Plan 04-08 SHA-256 span helper and vector test, ready for runner registration."
provides:
  - "Original build-only ARM and Thumb instruction images for hermetic runner controls."
  - "Separate immutable production and synthetic fixture indexes generated through the existing validators."
  - "Metadata, image-digest, runner-wiring, and hash-expectation controls registered in CI and ASan."
affects: [headless-runner, runner-metadata, phase-04-plans-10-13]

# Actuals use chars/4 over this plan's realized source diff.
actuals:
  tokens: 9083
  tasks: 2
  commits: 2
commits: 2
plan_head_before: 9a0e3a06e1a3ed9898e11e29ad4529d7a044ecbc
plan_head_after: 1cda649484462a17045293d309b9b11afcc7cf5c

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Production fixture metadata derives only from fatal-validated canonical manifest rows; an empty manifest produces a zero-row index."
    - "Synthetic build controls use a separate generated manifest/index and never imply rights for real content."
    - "Production and control runner targets share source, option, core, warning, and sanitizer wiring; only their private generated-index include directory differs."

key-files:
  created:
    - cmake/GenerateFixtureIndex.cmake
    - tests/fixtures/instruction_image.c
    - tests/gates/runner_metadata.cmake
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-09-SUMMARY.md
  modified:
    - CMakeLists.txt
    - tests/core.cmake
    - tests/hash_test.c
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Keep the canonical manifest authoritative and empty; do not add a synthetic row, license, or redistribution permission to it."
  - "Generate artificial synthetic validator inputs only under the build tree, with local-only use and an explicit no-rights-grant notice."
  - "Represent mailbox expectations with the project five-word v1 final-marker convention and preserve validated provenance, dispute IDs, BIOS requirement, and permitted-use values."
  - "Leave rows without a manifest byte digest non-matchable instead of deriving a ROM digest from a source revision."

requirements-completed: []
coverage:
  - id: D1
    description: "Fatal-validated canonical metadata and separate synthetic control metadata compile into immutable indexes."
    verification:
      - kind: integration
        ref: "tests/gates/runner_metadata.cmake"
        status: pass
    human_judgment: false
  - id: D2
    description: "Generated instruction images exercise mailbox, timeout, exception, Thumb, and BX paths; stale image digests are rejected."
    verification:
      - kind: integration
        ref: "tests/gates/runner_metadata.cmake"
        status: pass
    human_judgment: false
  - id: D3
    description: "SHA-256 vectors and metadata identity controls are registered in CI and ASan lanes."
    verification:
      - kind: unit
        ref: "ctest --test-dir build/ci -R '^runner\.(hash|metadata)$' --no-tests=error --output-on-failure"
        status: pass
      - kind: unit
        ref: "ctest --test-dir build/asan -R '^runner\.(hash|metadata)$' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false

metrics:
  duration: "not captured"
  completed: 2026-11-11
  status: complete
---

# Phase 4 Plan 9: Immutable Fixture Indexes and Metadata Controls Summary

**Original build-only ARM and Thumb images now feed separate validated production and synthetic indexes, with hash and metadata identity gates in CI and ASan.**

## Performance

- **Duration:** Not captured.
- **Started:** Not captured.
- **Completed:** 2026-11-11.
- **Tasks:** 2/2.
- **Implementation files modified:** 8.
- **Measured full CI test time:** 75.06 seconds; configure and build had no additional work after the task commits.

## Accomplishments

- Added a native C17 image generator that writes only build-tree images. The images contain explicit ARM/Thumb instruction words and exercise complete PASS/FAIL mailboxes, a partial mailbox, a branch loop, SWI, undefined and coprocessor stops, plus a Thumb/BX transition. The entry branch skips the zero-filled header area. A one-byte mutated image is generated for digest-drift controls.
- Added a CMake metadata generator that reuses the vocabulary, dispute-registry, and manifest readers. It fails before header generation when any reader reports findings. The canonical manifest remains empty and yields a zero-row production index. The separate control manifest contains one generated synthetic row with artificial license fields, `unknown` redistribution status, `local-only` use, and the real registry dispute ID `disp-gbatek-errata`; these fields grant no rights to real fixtures.
- Generated immutable metadata rows that preserve validated ID, available source digest, provenance, dispute IDs/count, BIOS requirement, permitted-use list, and the explicit five-word mailbox convention. C-string output rejects values outside safe allowlisted character grammars. In-tree canonical digests and generated control-image digests are independently checked by CMake.
- Established `GBL_RUNNER_SOURCES` for both `gamebloke-run` and `gamebloke-run-control`. Their actual target properties confirm identical source, compile-option, compile-definition, link-option, and core/warning wiring; only their private generated-index include path differs.
- Registered `runner.hash` and `runner.metadata` in the CI and ASan inventories. The metadata gate rejects malformed manifests before index output, wrong-index selection, stale image digests, wrong independent SHA expectations, unknown hash selectors, and source/options wiring drift.

## Task Commits

1. **Task 1: Generate immutable fixture metadata and hermetic owned control images** — `4af53e1` (`feat`).
2. **Task 2: Register hash and metadata identity controls** — `1cda649` (`test`).

## Files Created/Modified

- `tests/fixtures/instruction_image.c` — original build-only ARM/Thumb image generator.
- `cmake/GenerateFixtureIndex.cmake` — validated production and synthetic metadata index generation.
- `tests/gates/runner_metadata.cmake` — authority, digest, wrong-index, and target-wiring controls.
- `CMakeLists.txt` — generator/image/index dependencies and identical production/control runner targets.
- `tests/core.cmake`, `tests/hash_test.c`, `tests/inventory/ci.txt`, and `tests/inventory/asan.txt` — hash/metadata test registration and controls.

## Verification

- `cmake --preset ci && cmake --build --preset ci && cmake -DSRC=. -DBIN=build/ci -P tests/gates/runner_metadata.cmake` — passed, including malformed-manifest, digest-drift, and wrong-index controls. The same command passed again after Task 1's commit at the tracer feedback gate.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^runner\.(hash|metadata)$' --no-tests=error --output-on-failure` — passed 2/2 focused tests.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --test-dir build/asan -R '^runner\.(hash|metadata)$' --no-tests=error --output-on-failure` — passed 2/2 focused tests with AddressSanitizer and UndefinedBehaviorSanitizer.
- `cmake --workflow --preset ci` — configure and build passed; the full CTest run completed in 75.06 seconds and had one failure: the pre-existing `hygiene.tree` check flags a home-directory path in `.planning/phases/04-cpu-execution-core-and-headless-runner/04-08-SUMMARY.md` at line 85. The plan-owned hash and metadata tests passed in that run.
- `ctest --preset ci-post` — failed because its JUnit inventory correctly reports the same `hygiene.tree` failure. No change was made to the prior summary; the parent executor will address this before tracking Plan 04-09 complete or starting Plan 04-10.
- `git diff --check` — passed. The stub scan of all eight implementation files found no TODO, FIXME, placeholder, or empty-value stub patterns.

## Decisions Made

- The production index uses only canonical manifest authority; it remains empty unless an approved canonical row is later added.
- The synthetic control row exists only in the generated build tree and is marked artificial, disputed, local-only, and non-redistributable unknown.
- A Git source row with no manifest byte digest is emitted without a matchable digest; no digest is inferred from its revision.
- The existing host SHA-256 helper remains dependency-free and is built directly into the test executable and the shared runner source list.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed an invalid integer macro token in the Thumb/BX image recipe**
- **Found during:** Task 1 build.
- **Issue:** A trailing `u` inside `UINT32_C(...)` expanded to the invalid suffix `uU` under AppleClang.
- **Fix:** Removed the suffix from the macro argument; the macro supplies its own suffix.
- **Files modified:** `tests/fixtures/instruction_image.c`.
- **Verification:** CI and ASan builds passed.
- **Committed in:** `4af53e1`.

**2. [Rule 1 - Bug] Changed the target-property parity check to compare actual lines**
- **Found during:** Task 1 metadata verification.
- **Issue:** The initial newline-regex check falsely reported option divergence for identical empty target properties.
- **Fix:** Read the generated wiring report as lines and compare the paired target property values directly.
- **Files modified:** `tests/gates/runner_metadata.cmake`.
- **Verification:** The metadata control and full CI run both executed the parity checks successfully.
- **Committed in:** `4af53e1`.

**Total deviations:** 2 auto-fixed (Rule 1 - Bug). No scope or rights decision changed.

## Supported Scope and Limits

The generated files are original test programs, stored under `build/`; no ROM binary is committed. The control metadata is a validator fixture only and is not an approved fixture or redistribution grant. This plan does not run private inputs, use a BIOS, qualify hardware behavior, or complete the command-line producer. `RUN-01` and `RUN-02` remain partial until their later runner plans complete. The Phase 4 unresolved probe assumptions and descriptor-less rights/compatibility restrictions remain unchanged.

## Issues Encountered

The full CI failure is in the existing 04-08 summary path check and is outside this plan's owned files. Parent executor ownership applies to that documentation repair and final state/roadmap/requirements tracking. The plan implementation and focused verification gates are complete; phase requirements and phase completion are not claimed here.

## User Setup Required

None.

## Next Phase Readiness

The generated `owned-arm-pass.gba` image and synthetic control index are available for the actual CLI work in later plans. Plan 04-10 may proceed after the parent resolves the existing CI hygiene failure and reruns the canonical workflow.

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Completed: 2026-11-11*

## Self-Check: PASSED

- Generated index script, image generator, metadata gate, and this summary exist.
- Task commits `4af53e1` and `1cda649` are ancestors of the current phase branch.
- Focused CI and ASan checks passed; the full CI failure is recorded above for parent-owned repair.

## Parent Verification Reconciliation

The parent corrected the pre-existing machine-specific command path in the Plan 08 summary, then reran repository hygiene and signature scans successfully. A fresh canonical CI workflow passed in 79.10 seconds on the corrected tree, including JUnit inventory: 323 expected, 322 ran successfully, and the one allowed owner-local signature skip. The earlier failed run above remains historical evidence. Local logs: `build/phase04-tracer/plan09-ci-reconciled.log` and `.json`. Plan 09 may now advance.
