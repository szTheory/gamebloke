---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T05:32:45Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - cmake/BuildArmDiagnostics.cmake
  - cmake/CheckArmCoverage.cmake
  - diagnostics/armv4t/assertions.inc
  - diagnostics/armv4t/cases-arm.S
  - diagnostics/armv4t/cases.json
  - diagnostics/armv4t/inputs.txt
  - src/cpu.c
  - tests/cpu_test.c
  - tests/gates/coverage_case.cmake
  - tests/gates/diagnostics_case.cmake
findings:
  critical: 0
  warning: 4
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-13T05:32:45Z  
**Depth:** standard  
**Files Reviewed:** 10  
**Status:** issues_found

## Summary

The builder, runner, receipt checks, and raw stop observations preserve the intended pre-admission boundary. The semantic ledger has four concrete coverage defects: one PC case exercises the wrong PC operand, eight no-writeback rows actually write back, eight halfword force-user rows describe an invalid ARMv4T form and duplicate ordinary post-index cases, and two shifter boundary rows duplicate ordinary tests instead of exercising the named vectors. The generic ledger validation accepts these mislabeled programs.

## Warnings

### WR-01: PC+12 case reads PC from the wrong operand

**Severity:** WARNING  
**File:** `diagnostics/armv4t/cases.json:3325, 3350`; `cmake/CheckArmCoverage.cmake:318-384`

**Issue:** Case `507654410` is labeled `arm-register-shifted-operand-pc-plus-12`, but its program executes `add r0,pc,r2,lsl #1`: PC is Rn, and the shift is immediate. ARM7TDMI reads PC as instruction address +8 for this form; the test's `label+14` calculation confirms it is checking +8 plus the shifted value 6. It does not exercise the +12 PC read required for a register-controlled Operand2 shift. The checker only validates that an assertion contains the expected and broken values, so this incorrect operand placement passes.

**Fix:** Use a register-controlled shift with R15 in Rm, encode the intended opcode explicitly or verify its disassembly, and derive the assertion from the +12 PC value. Add a checker rule that verifies the declared PC form matches the operand position and shift encoding.

### WR-02: The eight no-writeback block-transfer cases all write back

**Severity:** WARNING  
**File:** `diagnostics/armv4t/cases.json:8378, 8544, 8710, 8876, 9042, 9165, 9288, 9411`; `cmake/CheckArmCoverage.cmake:318-337`

**Issue:** Cases `416055714`, `355734903`, `495708335`, `314309498`, `344471309`, `469844796`, `489728888`, and `516644361` are labeled `normal-no-writeback`, but each program uses `!` on its LDM/STM base and checks that the base changes by the transfer size. Their `independent_expectation.writeback` fields are also `true`. These rows therefore repeat the corresponding writeback behavior and leave all four IA/IB/DA/DB no-writeback forms untested, while the ledger counts them as present.

**Fix:** Remove `!` from each no-writeback instruction, assert that the base remains unchanged, set `writeback` to `false`, and validate instruction/writeback consistency for each block-transfer form.

### WR-03: Halfword force-user inventory includes an unpredictable encoding

**Severity:** WARNING  
**File:** `cmake/CheckArmCoverage.cmake:96-110`; `diagnostics/armv4t/cases.json:7023, 7187, 7352, 7516, 7680, 7844, 8009, 8173`; `src/cpu.c:1958-1971`

**Issue:** The checker adds `post-index-force-user` to `index_forms` and reuses it for LDRH, STRH, LDRSB, and LDRSH. In ARMv4T Mode 3, post-index requires P=0 and W=0; P=0,W=1 is UNPREDICTABLE (ARM DDI0100I A5.3). The decoder correctly refuses that combination. The eight corresponding rows instead assemble ordinary post-index instructions without the T encoding and duplicate the normal post-index rows. Thus the inventory requires a form that is not defined for these instructions and reports those duplicates as coverage.

**Fix:** Restrict `post-index-force-user` to ARM word/byte Mode 2 transfers, remove it from the Mode 3 expected inventory, and remove or relabel the eight halfword/signed rows so the ledger contains only defined forms.

### WR-04: Shifter boundary rows omit their named input vectors

**Severity:** WARNING  
**File:** `diagnostics/armv4t/cases.json:3584, 3744, 4464, 4504`; `cmake/CheckArmCoverage.cmake:318-337`; `.planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-CPU-NOTES.md:113`

**Issue:** `incoming-c-boundary-vectors` (`293321832`) is byte-for-byte the same program as `immediate-lsl-zero-preserves-c` (`467141940`): both test LSL #0 with operand `0x80000001` and incoming C=1. `zero-one-sign-bit-and-all-ones-operands` (`292551648`) duplicates `immediate-lsr-one` (`269997436`), also using only `0x80000001`. Neither row tests the named input values 0, 1, or all ones, and the incoming-C row adds no vector that could reveal a carry-preservation defect for incoming C=0. The CPU notes explicitly call for those operand boundaries and incoming C.

**Fix:** Give the boundary rows distinct programs that execute and assert the zero, one, sign-bit, and all-ones operands, and exercise relevant carry-preserving operations with both incoming C values. Validate that each named vector appears in an executed instruction and its corresponding assertion.

---

_Reviewed: 2026-11-13T05:32:45Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
