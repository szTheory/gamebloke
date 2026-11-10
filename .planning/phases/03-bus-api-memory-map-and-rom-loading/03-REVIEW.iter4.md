---
phase: 03-bus-api-memory-map-and-rom-loading
reviewed: 2026-11-10T20:27:43Z
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
  warning: 2
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-11-10T20:27:43Z
**Depth:** standard
**Files Reviewed:** 21
**Status:** issues_found

## Summary

The ROM and allocator identity snapshots are covered by passing refusal and mutation controls. The profile validator still has two false-negative paths: it can count a policy table hidden in an HTML comment, and its timing arithmetic can overflow while reporting success. The owner-pending logo status and the documented hardware and compatibility limits remain explicit.

## Warnings

### WR-01: Profile table validation counts rows hidden in HTML comments

**Severity:** WARNING
**File:** `cmake/CheckProfile.cmake:598-600`, `tests/gates/profile_case.cmake:152-173`
**Issue:** The section is passed through a filter that removes fenced and indented code, but leaves HTML comments intact. The first-table selector then accepts pipe rows inside a comment. I verified that placing a complete copy of the valid bus policy table inside an HTML comment before the visible table, then changing the visible ROM request row from `1` to `2`, still makes `CheckProfile.cmake` print `profile: ok`.
**Fix:** Remove HTML comment blocks before selecting the policy table. Add a negative control with a commented valid table before a stale or incomplete visible table.

### WR-02: Timing arithmetic overflow is accepted as valid

**Severity:** WARNING
**File:** `cmake/CheckProfile.cmake:315-324`
**Issue:** The timing operands are accepted as arbitrary nonnegative decimal integers, then multiplied with signed CMake `math(EXPR)`. With `dots_visible=4611686018427387904`, `dots_hblank=0`, `dots_per_line=4611686018427387904`, `cycles_per_dot=4`, `cycles_per_line=0`, and `cycles_per_frame=0`, CMake warns about signed overflow, evaluates the product as zero, and the validator reports `profile: ok` even though the arithmetic invariant is false.
**Fix:** Check operand bounds and addition/multiplication overflow before evaluating invariants, or use overflow-safe decimal arithmetic. Add a profile negative control for the overflowing product and require a `timing-arithmetic` finding.

---

_Reviewed: 2026-11-10T20:27:43Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
