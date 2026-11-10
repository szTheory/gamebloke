---
phase: 03-bus-api-memory-map-and-rom-loading
fixed_at: 2026-11-10T20:38:47Z
review_path: .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
iteration: 3
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-11-10T20:38:47Z
**Source review:** `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md`
**Iteration:** 3

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### WR-01: Profile table validation counts rows hidden in HTML comments

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`
**Commit:** `170360f`
**Fix status:** fixed: requires human verification
**Applied fix:** The policy-table check now removes HTML comment blocks before selecting the contiguous rendered table. Added a negative control that places a complete valid table in a comment before a stale visible table. Existing section, fenced-code, indentation, and duplicate-key controls remain in place.

### WR-02: Timing arithmetic overflow is accepted as valid

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`
**Commit:** `b8a23b3`
**Fix status:** fixed: requires human verification
**Applied fix:** Timing values are now checked against the signed 64-bit `math(EXPR)` range using decimal text before entering invariants. Sum and product invariants preflight overflow before evaluating arithmetic and report stable `timing-arithmetic` findings. Added controls for the reproduced product overflow, sum overflow, and an out-of-range operand while retaining the valid AGB profile.

## Verification

Verification ran in the main checkout (`workflow.use_worktrees=false`). This report is not committed.

- Focused `bus_policy_doc` control passed, including the new commented-table negative control and the existing section, fence, indentation, and duplicate-key cases.
- Focused `timing_arithmetic` control passed, including the reproduced product overflow, sum overflow, out-of-range operand, and existing invariant controls.
- Direct validation of `profiles/agb.json` and `docs/profile-agb.md` reported `profile: ok (agb v2, 11 timing constants, 11 unmodeled)`.
- `cmake --workflow --preset ci`: passed all 292 registered tests; inventory reported 290 ran and 2 allowed skips. Wall time was 53.1 seconds; configure took 0.1 seconds and the build reported no work, with no duplicate configure or build step observed.
- `cmake --workflow --preset hygiene`: passed all 200 registered tests; inventory reported 198 ran and the same 2 allowed skips. Wall time was 28.9 seconds; configure took 0.1 seconds and the build reported no work, with no duplicate configure or build step observed.
- The real-logo digest and owner-local signature checks remain skipped because owner-provided logo evidence is pending and `GBL_OWNER_LOGO_FILE` is unset. No logo, hardware, or rights criterion is marked complete by this report.
- ASan was not run because the changes are limited to CMake validation and its gate; no C source or ASan coverage changed.
- `git diff --check` passed for the fix changes before commit.

---

_Fixed: 2026-11-10T20:38:47Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 3_
