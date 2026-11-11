---
phase: 03-bus-api-memory-map-and-rom-loading
fixed_at: 2026-11-10T22:02:48Z
review_path: .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
iteration: 7
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-11-10T22:02:48Z
**Source review:** `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md`
**Iteration:** 7

**Summary:**
- Findings in scope: 3
- Fixed: 3
- Skipped: 0

## Fixed Issues

### WR-01: Raw HTML block forms bypass the element-tag check

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`, `docs/profile-agb.md`
**Commit:** `8ffbfa1`
**Fix status:** fixed: requires human verification
**Applied fix:** The document check now rejects generic `<!` declaration and `<?` processing-instruction forms after code and comment filtering, alongside element tags. The profile documentation names these unsupported forms. Negative controls cover a processing instruction and CDATA.

### WR-02: Multiline inline code spans trigger false HTML findings

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`, `docs/profile-agb.md`
**Commit:** `b222c1d`
**Fix status:** fixed: requires human verification
**Applied fix:** Inline code matching now scans across line breaks using equal-width backtick delimiters. Multiline spans are masked during canonical normalization so headings and version text inside them are not discovered; same-line spans remain available for table identifiers. The HTML check removes all inline spans. A positive control contains a multiline `<section>` example, and a negative control proves a fake inline version is hidden while the visible stale version is reported.

### WR-03: Malformed or extra body rows can pass the table checks

**Files modified:** `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`
**Commit:** `d0a852c`
**Fix status:** fixed: requires human verification
**Applied fix:** Every policy body row is checked for four cells and a known policy key, and the total body-row count must match the expected rows. Each Unmodeled body row must have two cells, with the complete ID occupying the first cell. Controls cover an extra policy key, a five-cell policy row, a three-cell Unmodeled row, and an ID embedded in extra identifier-cell text.

## Earlier Fixes Retained

- `b05cb42`: Reject generic and custom HTML element tags.
- `4160886`: Require exact policy and Unmodeled headers and separators.
- `62db5f9`: Normalize the canonical document once and reject hidden or duplicate version content.
- `16b3d80`: Require visible synchronized profile content.
- `b8a23b3`: Reject timing arithmetic overflow.
- `170360f`: Ignore commented policy tables when checking visible content.
- `00f1f48`: Restrict policy-row checks to a rendered table.

## Verification

Verification ran in the main checkout (`workflow.use_worktrees=false`).

- Focused CTest run passed `profile.lint`, `profile.nc.bus_policy_doc`, and `profile.pos.bus_policy_copy` (3/3).
- The negative-control gate confirmed PI and CDATA rejection, multiline-span masking, exact row-cell counts, unexpected policy-row rejection, and full-cell Unmodeled IDs.
- Direct validation of `profiles/agb.json` and `docs/profile-agb.md` reported `profile: ok (agb v2, 11 timing constants, 11 unmodeled)`.
- `git diff --check` passed before the WR-03 commit.
- CI, hygiene, and ASan were not rerun here; the orchestrator owns the bounded post-change reruns.

---

_Fixed: 2026-11-10T22:02:48Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 7_
