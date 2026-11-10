---
phase: 03-bus-api-memory-map-and-rom-loading
reviewed: 2026-11-10T22:13:08Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - cmake/CheckProfile.cmake
  - tests/gates/profile_case.cmake
  - docs/profile-agb.md
findings:
  critical: 0
  warning: 2
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-11-10T22:13:08Z · **Depth:** standard · **Files Reviewed:** 3 · **Status:** issues_found

## Summary

Reviewed the parser-fix delta in commits `8ffbfa1`, `b222c1d`, and `d0a852c`, including the earlier normalized-view invariants they complete. Generic HTML declaration and processing-instruction forms and element tags are rejected after normalization; multiline inline-code spans are hidden from version, heading, and table discovery while same-line identifiers remain available; and the targeted canonical-header, delimiter, body-cell, key, and ID controls behave as expected. The focused CTest run passed all four selected checks, including signed arithmetic boundary controls. Two additional temporary document mutations still pass: duplicate Unmodeled IDs and extra table rows without outer pipes.

## Warnings

### WR-01: Duplicate Unmodeled rows pass after ID deduplication

**Severity:** WARNING

**File:** `cmake/CheckProfile.cmake:993-1003, 1237-1246`

**Issue:** The Unmodeled parser checks each recognized row’s two-cell shape and full ID cell, but it never compares the validated body-row count or per-ID occurrence counts with the JSON entries. It removes duplicates from `doc_ids` before checking membership, so duplicating a valid expected row leaves the compared ID set unchanged. Reproduced by duplicating the `sp-micro-differences` row; the checker returned `profile: ok`.

**Fix:** Require the validated Unmodeled row count to equal the expected ID count and require each expected ID to occur exactly once before comparing IDs.

### WR-02: Pipe-separated rows without outer pipes can be silently ignored

**Severity:** WARNING

**File:** `cmake/CheckProfile.cmake:870-880, 934-939`

**Issue:** Both table collectors treat a line as a body row only when it has both outer pipes. When a nonmatching line follows a recognized table, they end collection without rejecting a nonblank line that still contains pipe-separated cells. GFM permits table rows to omit the optional outer pipes, so an extra rendered row after the final expected policy or Unmodeled row is omitted from validation. Reproduced with an extra pipe-separated row after each table; both variants returned `profile: ok`.

**Fix:** While inside a table, end it only at a blank line or heading and reject any other pipe-containing line that does not match the supported row form, or parse optional outer pipes and validate the resulting row.

---

_Reviewed: 2026-11-10T22:13:08Z_ · _Reviewer: the agent (gsd-code-reviewer)_ · _Depth: standard_
