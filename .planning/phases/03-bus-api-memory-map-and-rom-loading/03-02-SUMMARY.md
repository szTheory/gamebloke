---
phase: 03-bus-api-memory-map-and-rom-loading
plan: "02"
subsystem: bus
tags: [gba-profile, bus-policy, deterministic-state, cmake, ctest]

requires:
  - phase: 03-bus-api-memory-map-and-rom-loading
    provides:
      - Canonical timing-bearing bus API and deterministic state behavior
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides:
      - Profile JSON validation, narrative synchronization, and mutation-control helpers
provides:
  - Version 2 agb profile with source-labelled bus, initialization, and read policies
  - Semantic validator checks that compare profile policy values with narrative tables
  - Registered policy negative controls and clean/coordinated positive controls
affects: [phase-03-memory-map, phase-03-rom-loader, phase-04-cpu]

actuals:
  tokens: 8186
  tasks: 2
  commits: 2
plan_head_before: 3116c6585d433c51545534eea6b9e93d2f83a854
plan_head_after: 1137c55eb711c55bb7fc7594ac816707e7f2fab6
commits: 2

tech-stack:
  added: []
  patterns:
    - "Build narrative policy rows from parsed profile JSON, then compare exact values and source labels."
    - "Mutation controls use real input copies and require a non-zero validator result with the targeted stable reason."

key-files:
  created: []
  modified:
    - profiles/agb.json
    - docs/profile-agb.md
    - cmake/CheckProfile.cmake
    - tests/gates/profile_case.cmake
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "Keep schema gamebloke.profile/1 and bump the additive policy-bearing profile to profile_version 2."
  - "Record one-cycle bus costs and zero initialization as IMPL project policy, not measured hardware behavior."
  - "Keep absent-BIOS, unsupported-I/O, and ROM-tail current-latch reads explicitly unverified; WAITCNT, prefetch, BIOS protection, and active I/O devices remain unmodeled."

patterns-established:
  - "The narrative policy table is generated from parsed JSON values for comparison, preventing a file path from being mistaken for parsed policy data."
  - "Each profile policy reason has a semantic negative control; clean and coordinated copies prove the validator accepts valid policy changes."

requirements-completed: [BUS-01]

coverage:
  - id: D1
    description: "The agb profile declares and validates bus request costs, initialization values, and read policies against the narrative."
    verification:
      - kind: other
        ref: "cmake -DPROFILE=profiles/agb.json -DDOC=docs/profile-agb.md -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckProfile.cmake"
        status: pass
      - kind: integration
        ref: "profile.lint and profile.pos.bus_policy_copy"
        status: pass
    human_judgment: false
  - id: D2
    description: "Registered mutation controls reject missing, changed, mislabelled, and stale profile policies for their intended reasons."
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset hygiene"
        status: pass
    human_judgment: false
  - id: D3
    description: "The profile explicitly separates project determinism from hardware evidence and retains the unresolved BUS-02 transparency constraint."
    requirement: BUS-02
    verification: []
    human_judgment: true
    rationale: "Automated checks can preserve policy labels and wording but cannot establish physical hardware behavior or resolve the flagged manual scope review."
  - id: D4
    description: "The profile records zero initialization, while full BUS-03 completion still needs its runtime/load integration evidence."
    requirement: BUS-03
    verification: []
    human_judgment: true
    rationale: "This plan validates the profile declaration; it does not verify every initialization and loading path against that policy."

duration: 21min
completed: 2026-11-10
status: complete
---

# Phase 03 Plan 02: Versioned Bus and Initialization Profile Policy Summary

**Version 2 of the agb profile now validates bus costs, deterministic initialization, and explicitly unverified read policy against the narrative.**

## Performance

- **Duration:** 21 min
- **Started:** 2026-11-10T16:25:18Z
- **Completed:** 2026-11-10T16:46:12Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- Added source-labelled one-cycle bus request costs, raw widths/classes, zero initialization values, and current-latch read choices to `profiles/agb.json` while retaining schema `gamebloke.profile/1`.
- Extended the validator and profile guide to compare exact policy values and labels; corrected the parsed-profile argument passed into narrative synchronization.
- Added registered semantic negative controls for every policy reason, a clean policy copy, and a coordinated version 3 positive; ran the canonical CI and hygiene workflows.

## Task Commits

1. **Task 1: The actual profile declares and validates one bus-policy path** - `945b1e8` (`feat`)
2. **Task 2: Prove every new policy reason and coordinated-change behavior** - `1137c55` (`test`)

The summary and planning-state update are committed separately from these task commits.

## Files Created/Modified

- `profiles/agb.json` - Version 2 bus, initialization, and read policy declarations.
- `docs/profile-agb.md` - Policy tables, provenance labels, and explicit evidence limits.
- `cmake/CheckProfile.cmake` - Policy validation, narrative comparison, and stable reason codes.
- `tests/gates/profile_case.cmake` - Semantic policy mutations and clean/coordinated positive cases.
- `tests/gates.cmake` - CI and hygiene test registration.
- `tests/inventory/ci.txt`, `tests/inventory/hygiene.txt` - Sorted inventory entries for the new controls.

## Decisions Made

- Kept the profile schema at `gamebloke.profile/1`; the additive profile policy contract is represented by `profile_version: 2`.
- Kept all one-cycle costs and zero initialization values labelled IMPL project policy. Hardware power-on RAM values remain unknown.
- Kept absent BIOS, unsupported I/O, and ROM tails on the current-open-bus policy while clearly labelling those behaviors unverified. WAITCNT, prefetch, BIOS protection, and active I/O devices remain unmodeled.

## Verification

- Direct `CheckProfile.cmake` validation passes for the actual profile and narrative.
- `profile.lint` and the `bus_policy_missing` direct negative control pass; the latter reports `bus-policy-missing`.
- The new bus value/source/doc, initialization, and read-policy mutation cases all pass with their intended reason codes. The clean policy copy and coordinated version 3 update pass.
- Scratch copies with an incorrect expected reason and a disabled validator branch both caused their corresponding controls to go red; scratch files were removed.
- `cmake --workflow --preset ci`: 272 passed, with two existing owner-dependent signature checks skipped for their recorded reasons. The measured test path was 33.40 seconds; Ninja had no work to build.
- `cmake --workflow --preset hygiene`: 200 passed, with the same two recorded skips. The measured test path was 27.63 seconds; Ninja had no work to build.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Pass parsed profile JSON to narrative synchronization**
- **Found during:** Task 1
- **Issue:** `gbl__profile_doc_sync` expected parsed JSON as argument 2, but the caller passed the profile file path, causing policy fields to appear missing during comparison.
- **Fix:** Changed the call to pass `${doc}`.
- **Files modified:** `cmake/CheckProfile.cmake`
- **Verification:** Direct profile validation, `profile.lint`, and the `bus_policy_missing` mutation all passed after the correction.
- **Committed in:** `945b1e8`

**2. [Rule 1 - Bug] Update legacy controls to the new profile baseline**
- **Found during:** Task 2
- **Issue:** The existing narrative-version mutation still changed version 1 to 2 after the profile baseline became version 2; the existing unmodeled-list control also expected the prior nine-entry list after two required entries were added.
- **Fix:** Changed the mismatch control to mutate version 2 to 3 and updated the unmodeled control to cover all eleven required entries.
- **Files modified:** `tests/gates/profile_case.cmake`
- **Verification:** The focused version mismatch control and both full workflows passed.
- **Committed in:** `1137c55`

**Total deviations:** 2 auto-fixed (2 Rule 1)
**Impact on plan:** Both fixes were necessary to make the profile/document path and existing controls correct after the planned profile-version change.

## Open Transparency Constraint

The plan's flagged BUS-02 edge remains unresolved pending manual scope review, and full BUS-02 decode evidence belongs to the later Phase 3 coverage. The profile records the present current-latch behavior for absent BIOS, unsupported I/O, and ROM tails as unverified project behavior; this work does not establish hardware accuracy or broad compatibility. BUS-03 remains pending until its runtime and load integration paths are verified against the recorded zero-fill policy.

## Issues Encountered

The first full CI run found the stale version-mismatch test described above. The control was updated to the version 2 baseline and passed both the focused rerun and the subsequent full CI run.

## User Setup Required

None.

## Next Phase Readiness

Plan 03-03 remains the next runnable Phase 3 plan. Phase 3 remains in progress, and the flagged BUS-02 scope review remains visible for downstream resolution.

## Self-Check: PASSED

- Summary file exists at the required phase path.
- Task commits `945b1e8` and `1137c55` are ancestors of the current branch head.
- Frontmatter records plan status, measured task commits, and the unresolved transparency constraint.

---
*Phase: 03-bus-api-memory-map-and-rom-loading*
*Completed: 2026-11-10*
