---
phase: 03-bus-api-memory-map-and-rom-loading
reviewed: 2026-11-10T19:34:19Z
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

**Reviewed:** 2026-11-10T19:34:19Z  
**Depth:** standard  
**Files Reviewed:** 21  
**Status:** issues_found

## Summary

Reviewed the bus and loader implementation, profile validation, tests, and policy/evidence documentation. I found no confirmed defect in the current bus or loader behavior. Two test-gate gaps can allow future ownership-state regressions or stale profile documentation to pass validation.

## Warnings

### WR-01: Profile policy rows are validated outside their sections

**Severity:** WARNING  
**File:** `cmake/CheckProfile.cmake:431-442`  
**Issue:** `gbl__profile_doc_policy_table` counts matching rows and searches for expected row text across the entire document. The validator checks that policy headings exist, but does not limit each policy table check to the corresponding section. If a policy table is removed or made stale while duplicate expected rows remain elsewhere in the Markdown, `profile.lint` can still pass with the actual policy narrative out of sync with `profiles/agb.json`.
**Fix:** Extract each policy section between its heading and the next heading, then count and compare rows only inside that section. Add a negative control that moves the expected rows outside their policy table while leaving the table stale.

### WR-02: Refusal snapshots omit allocator and ROM identity

**Severity:** WARNING  
**File:** `tests/state_check.h:34-60`, `tests/loader_test.c:212-225`, `tests/bus_test.c:225-263`  
**Issue:** The equality helpers intentionally exclude allocator callback/context identity and compare ROM pointers only for nullness plus byte contents. The refusal and overflow snapshot assertions rely on those helpers, so a regression that changes the allocator binding or swaps the installed ROM pointer for byte-identical storage can still compare equal. Such a mutation can later release through the wrong allocator or leak the original allocation, violating the loader's ownership and refusal-atomicity contract.
**Fix:** Keep the identity-agnostic canonical comparison for comparing independent machines, but add same-instance snapshot checks on refusal paths for exact `rom` pointer, `rom_length`, `allocator.context`, `allocator.allocate`, and `allocator.release` equality.

---

_Reviewed: 2026-11-10T19:34:19Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
