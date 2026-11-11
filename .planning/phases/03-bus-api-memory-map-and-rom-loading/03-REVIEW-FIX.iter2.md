---
phase: 03-bus-api-memory-map-and-rom-loading
fixed_at: 2026-11-10T19:52:23Z
review_path: .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-11-10T19:52:23Z
**Source review:** `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### WR-01: Profile policy rows are validated outside their sections

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`
**Commit:** `5aeed9e`
**Applied fix:** Policy row counts and expected-row checks now inspect only the text between each policy heading and the next Markdown heading. The profile negative control makes the in-section ROM policy row stale and moves a valid copy into `## Machine`; the validator rejects it for the intended reason.

### WR-02: Refusal snapshots omit allocator and ROM identity

**Files modified:** `tests/state_check.h`, `tests/bus_test.c`, `tests/loader_test.c`, `tests/gates/core_mutation.cmake`, `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt`
**Commit:** `3287992`
**Applied fix:** Added a same-instance snapshot comparison for exact ROM pointer and length plus allocator context, allocate callback, and release callback. Bus invalid-argument and overflow snapshots and loader refusal/allocation-failure snapshots now check it alongside canonical state. Canonical comparison and digest remain identity-agnostic; the loader test proves identical ROM bytes with a different pointer and each allocator-field mutation remain canonically equal but fail the identity comparison. Two registered mutation controls inject ROM pointer and allocator callback changes on refusal and require the identity assertion to fail.

## Verification

All verification ran in the main checkout (`workflow.use_worktrees=false`); no isolated worktree was used.

- Focused profile check and `profile.nc.bus_policy_doc`: passed.
- Focused bus and loader checks (`bus.timing`, `bus.video`, `loader.lengths`, `loader.transaction`, `loader.diagnostics`): passed.
- New refusal identity mutation controls (`loader.nc.refusal_rom_identity`, `loader.nc.refusal_allocator_identity`): passed.
- `cmake --workflow --preset ci`: passed; 292 tests passed, then the JUnit inventory passed. The real-logo digest and owner-local signature checks were skipped as configured because they require owner-provided evidence.
- `cmake --workflow --preset hygiene`: passed; 200 tests passed, then the JUnit inventory passed. The same two owner-only signature checks were skipped as configured.
- The `asan` workflow preset could not configure because its `clang-21` executable name is absent from `PATH`. The installed `/usr/bin/clang` reports AppleClang 21.0.0, so the equivalent ASan/UBSan configuration was built in `build/asan-clang21` with the preset's sanitizer flags. All 38 tests passed, including both sanitizer controls, and the ASan JUnit inventory passed.
- `git diff --check`: passed before both fix commits.

## Skipped Issues

None.

---

_Fixed: 2026-11-10T19:52:23Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
