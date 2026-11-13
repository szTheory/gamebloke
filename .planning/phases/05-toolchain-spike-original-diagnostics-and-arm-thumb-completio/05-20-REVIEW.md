---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T06:57:08Z
depth: standard
files_reviewed: 8
files_reviewed_list:
  - cmake/BuildArmDiagnostics.cmake
  - cmake/CheckArmCoverage.cmake
  - diagnostics/armv4t/assertions.inc
  - diagnostics/armv4t/cases-thumb.S
  - diagnostics/armv4t/cases.json
  - diagnostics/armv4t/inputs.txt
  - tests/gates/coverage_case.cmake
  - tests/gates/diagnostics_case.cmake
findings:
  critical: 1
  warning: 2
  info: 0
  total: 3
  necessity: 0
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-13T06:57:08Z  
**Depth:** standard  
**Files Reviewed:** 8  
**Status:** issues_found

## Summary

Reviewed the Thumb assembly, 117 semantic pairs, eight stop witnesses, producer and coverage checker, and raw trace gate. The current rows build and observe the intended instruction forms, but the coverage crosswalk does not independently enforce several form-defining bits, and several shift boundary vectors do not distinguish the expected carry behavior from stale carry state.

## Critical Issues

### CR-01: Coverage crosswalk does not enforce format-defining operand bits

**Severity:** BLOCKER  
**File:** `cmake/CheckArmCoverage.cmake:622-626`
**Issue:** The format 5 crosswalk masks only `0xff00`, which checks ADD/CMP/MOV/BX opcode class but omits H1/H2 bits 7 and 6 that distinguish high from low registers. Format 14 similarly masks `0xfe00` at lines 658-663 and omits bit 8, which distinguishes PUSH/POP with LR/PC. The later validation at lines 708-711 compares only those masks against `encoding_checks` values supplied by the same case row. A coordinated change to the program and its supplied opcode value can therefore keep a row such as `ADD:low-high` or `PUSH:include-LR` while executing a different form and still pass the format crosswalk. The current source rows contain the named forms, but the checker does not protect them from silent coverage loss.
**Fix:** Derive and require the H1/H2 selector bits for format 5 subforms and the LR/PC bit for format 14 forms. Require each row's check mask to include those bits and compare them against fixed values derived from its `form`; add mutation controls that change both the instruction and its matrix encoding value and assert the checker rejects the mismatch.

## Warnings

### WR-01: Special LSR and ASR carry cases can pass with stale carry

**Severity:** WARNING  
**File:** `diagnostics/armv4t/cases.json:9879-9903`
**Issue:** Case `742650290` tests immediate LSR #0 as LSR #32 using input `0x80000009` and seeds C=1; the expected outgoing C is also 1. Case `1800200023` does the same for ASR #32. The register-count-zero LSR/ASR vectors (`1800200027` and `1800200028`) also seed C=1 with an operand whose sign bit is 1. These vectors do not distinguish the specified carry result or preservation from an implementation that reuses the old C bit.
**Fix:** Add vectors where incoming C differs from the required result: use C=0 with bit 31 set for immediate LSR/ASR #0, and C=0 with count zero for register LSR/ASR while asserting that C remains 0. Keep the existing C=1 vectors as the complementary cases.

### WR-02: Register ROR omits zero and non-multiple over-32 boundaries

**Severity:** WARNING  
**File:** `diagnostics/armv4t/cases.json:10062-10090`
**Issue:** The only ROR boundary case runs counts 32 and 64; the regular `ROR:register` case uses count 3. There is no ROR count-zero case to verify operand/C preservation, nor a non-multiple count above 32 (such as 33) to verify the result and carry path after the special multiple-of-32 cases. The format crosswalk therefore counts the ROR form without covering these distinct boundary behaviors.
**Fix:** Add a stable-ID ROR boundary case covering count 0 with both incoming carry values and a non-multiple count above 32, such as 33, with an operand whose shifted-out bit distinguishes the expected C. Retain the current 32/64 vectors for the modulo-32 special case.

---

_Reviewed: 2026-11-13T06:57:08Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
