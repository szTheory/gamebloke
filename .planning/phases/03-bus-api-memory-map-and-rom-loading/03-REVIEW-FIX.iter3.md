---
phase: 03-bus-api-memory-map-and-rom-loading
fixed_at: 2026-11-10T20:19:25Z
review_path: .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
iteration: 2
findings_in_scope: 1
fixed: 1
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-11-10T20:19:25Z
**Source review:** `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md`
**Iteration:** 2

**Summary:**
- Findings in scope: 1
- Fixed: 1
- Skipped: 0

## Fixed Issues

### WR-01: Policy validation counts rows outside the actual table

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`
**Commit:** `00f1f48`
**Fix status:** fixed: requires human verification
**Applied fix:** Policy validation now examines the contiguous Markdown pipe table in the target heading section, after masking fenced and indented code. It requires each expected first-column key exactly once and validates the complete expected row. Added same-section fenced and indented example controls, including tab indentation, plus a duplicate-key control. The earlier other-section control remains in place. The supported table format is a pipe table with a header, separator, and contiguous body rows.

## Verification

All verification ran in the main checkout (`workflow.use_worktrees=false`). The report is not committed.

- Focused profile subset: passed, 29/29 tests.
- `cmake --workflow --preset ci`: passed, 292/292 tests; inventory confirmed 290 ran and 2 owner-only checks were skipped as allowed.
- `cmake --workflow --preset hygiene`: passed, 200/200 tests; inventory confirmed 198 ran and the same 2 owner-only checks were skipped as allowed.
- The real-logo digest and owner-local signature checks remain configured and unchanged. They reported `SKIPPED` because the owner-provided logo evidence is still pending and `GBL_OWNER_LOGO_FILE` is unset.
- ASan was not run: this change is confined to the CMake profile validator and its CMake gate; the ASan inventory does not include profile tests. CI and hygiene cover the affected profile tests.
- `git show --check` passed for the committed fix. A whole-worktree `git diff --check` also reported trailing whitespace in the pre-existing modified `03-REVIEW.md`; that review file was not edited by this fix.

## Skipped Issues

None.

---

_Fixed: 2026-11-10T20:19:25Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 2_
