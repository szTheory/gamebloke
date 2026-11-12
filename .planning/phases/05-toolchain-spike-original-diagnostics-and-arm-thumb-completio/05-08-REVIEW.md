---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "08"
reviewed: 2026-11-12T05:33:01Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - src/cpu.c
  - tests/cpu_test.c
  - tests/core.cmake
  - tests/inventory/ci.txt
  - tests/inventory/asan.txt
findings:
  critical: 0
  warning: 4
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 05 Plan 08: Code Review Report

**Reviewed:** 2026-11-12T05:33:01Z
**Depth:** standard
**Files Reviewed:** 5
**Status:** issues_found

## Summary

Reviewed all five scoped source and test files against Plan08 and the phase's ARMv4T CPU notes. The PC operand contexts, PC-return refill ordering, PSR decode priority, and effect preflight paths are consistent with the stated ARMv4T contracts on inspection. The implementation has no confirmed correctness or security defect in this scope. Four warnings identify missing test evidence for PSR edge cases that the plan and review focus explicitly require. The summary reports green CI and sanitizer runs; no checks were rerun for this review. No necessity findings.

## Warnings

### WR-01: Unallocated-bit rejection is not tested before field masking

**Severity:** WARNING
**File:** `tests/cpu_test.c:2634-2645`
**Issue:** The only MSR reserved-bit refusal selects the `x` field and supplies `0x100`, which is inside that selected byte. It would still be rejected by an implementation that applied the selected field mask before checking `UnallocMask`, so the test does not protect the explicitly chosen check order in `src/cpu.c:1205`.
**Fix:** Add a refusal case with an unallocated operand bit outside the selected fields, such as a zero field mask with `R0 = 0x100` or a `c`-only mask. Assert the deterministic stop and unchanged machine, trace metadata, and caller buffer.

### WR-02: The CPSR StateMask guard is only tested with the control field selected

**Severity:** WARNING
**File:** `tests/cpu_test.c:2625-2628`
**Issue:** The active-CPSR T-bit refusal uses `MSR CPSR_c, #T`, so the c field includes the T bit. It does not show that the privileged `StateMask` guard at `src/cpu.c:1229` rejects T even when the c field is unselected, as required by the recorded ARM operation semantics. The SPSR c-field test at lines 2584-2597 does establish the separate allowed saved-T path.
**Fix:** Add a privileged `MSR CPSR_f` case whose operand has only T set; assert it refuses before any machine or trace effects while retaining the SPSR T-write positive case.

### WR-03: The saved-status MSR path lacks register-source and flags-field coverage

**Severity:** WARNING
**File:** `tests/cpu_test.c:2568-2597`
**Issue:** The positive SPSR write test covers only an immediate `c`-field update. Register-source and rotated-immediate `c/x/s/f` coverage at lines 2373-2463 exercises CPSR only. Since SPSR writes take a separate status-effect path in `src/cpu.c:1215-1253`, the suite does not establish that register operands and saved flags-field writes target and preserve the selected exception bank correctly.
**Fix:** Add real-instruction SPSR cases for register-source and rotated-immediate writes, including an `f`-field update with non-default saved status, and independently assert the updated active SPSR bank plus preserved unselected fields. Include a second exception mode if the acceptance matrix is intended to cover bank selection for writes.

### WR-04: Zero and composed PSR field masks have no distinguishing positive case

**Severity:** WARNING
**File:** `tests/cpu_test.c:2373-2463`
**Issue:** The CPSR mask loops test each individual lane, but `x` and `s` use zero operands and expect no write, while `c` and `f` are tested separately. No case proves that a zero field mask retires without a status effect, or that a composed mask updates selected fields while preserving unselected flags/control state. This leaves the zero-mask and field-composition contracts unguarded despite the other single-lane cases.
**Fix:** Add a zero-mask MSR with a legal operand and assert status is unchanged while the instruction retires normally. Add a composed `c|f` case with independently chosen values and assert both selected fields change and all other CPSR/SPSR fields remain unchanged.

---

_Reviewed: 2026-11-12T05:33:01Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
