---
phase: 03-bus-api-memory-map-and-rom-loading
reviewed: 2026-11-10T20:00:57Z
depth: standard
files_reviewed: 21
files_reviewed_list:
  - CMakeLists.txt
  - cmake/CheckProfile.cmake
  - docs/bus-api.md
  - docs/profile-agb.md
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - profiles/agb.json
  - src/bus.c
  - src/loader.c
  - src/machine.c
  - tests/bus_test.c
  - tests/core.cmake
  - tests/gates.cmake
  - tests/gates/core_mutation.cmake
  - tests/gates/profile_case.cmake
  - tests/inventory/asan.txt
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
  - tests/loader_test.c
  - tests/state_check.h
  - tests/version_test.c
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-11-10T20:00:57Z
**Depth:** standard
**Files Reviewed:** 21
**Status:** issues_found

## Summary

The ownership snapshot fix and its ROM/allocator mutation controls are effective. The profile validator now rejects misplaced rows from other sections, but it still accepts expected policy rows anywhere inside the target section; a row copied into prose or a code example can hide a missing row in the actual table.

## Warnings

### WR-01: Policy validation counts rows outside the actual table

**Severity:** WARNING
**File:** `cmake/CheckProfile.cmake:448-456`, `tests/gates/profile_case.cmake:144-151`
**Issue:** The validator counts matching row text throughout the extracted heading section, not only within the policy table. If a row is removed from the table and copied verbatim into a fenced example or another table under the same heading, the row count can still equal the expected count and every expected string can still be found, so `profile.lint` passes with an incomplete policy table. The new negative control moves a duplicate under `## Machine`, so it does not cover this same-section case.
**Fix:** Restrict matching to the contiguous Markdown policy table and require each expected key exactly once there. Add a negative control that removes a row from the table and places its expected text in a fenced example within the same section.

---

_Reviewed: 2026-11-10T20:00:57Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
