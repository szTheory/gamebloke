---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-12T18:29:27Z
depth: deep
files_reviewed: 5
files_reviewed_list:
  - src/cpu.c
  - tests/cpu_test.c
  - tests/core.cmake
  - tests/inventory/ci.txt
  - tests/inventory/asan.txt
findings:
  critical: 0
  warning: 2
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 05 Plan 11: Code Review Report

**Reviewed:** 2026-11-12T18:29:27Z  
**Depth:** deep  
**Files Reviewed:** 5  
**Status:** issues_found

## Summary

Reviewed the Plan 05-11 diff from `8f861f7` to `a4f2902` for ARM7TDMI Mode 3 halfword and signed transfers, actual-bus alignment transforms, legality/refusal handling, preflight, test oracles, and selector inventories. The implementation’s operand checks and bus-result transforms agree with ARM DDI0100I and the ARM7TDMI DDI0029G scope; GBATEK’s odd-address behavior is correctly presented as reference-backed emulator policy, not a hardware measurement. One test-matrix gap remains: U polarity is not exercised independently for each P/W addressing form.

This is a plan-scoped review only. No tests were run for this review; the CI and sanitizer results in the plan summary are recorded evidence, not independently reproduced here. Linux compiler/sanitizer verification and the full Phase 05 CPU/diagnostic gate remain outside this report.

## Warnings

### WR-01: Addressing tests do not cover both U polarities for each P/W form

**File:** `tests/cpu_test.c:2234-2240`
**Issue:** The six matrix rows cover the six immediate/register offset, pre-index, and post-index syntaxes, but they fix U to one value per syntax and offset kind. For example, immediate post-index is only tested with U=1 while register post-index is only tested with U=0; neither immediate nor register offset/pre-index exercises both legal U values for the same P/W combination. A defect that couples add/subtract behavior to P, W, or offset kind could therefore pass this matrix despite the requirement to cover U plus and minus.
**Fix:** Add cases that exercise both U values for each immediate/register offset, pre-index, and post-index form, retaining the existing independent expected-address and writeback checks for all four transfer kinds.

---

_Reviewed: 2026-11-12T18:29:27Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_


## Root local compiler finding (after independent review)

### WR-02: GCC rejects implicit narrowing of transform-count arithmetic

**Severity:** WARNING (blocks the local GNU build).
**Observed evidence:** Root pinnedUbuntu26.04nativearm64GCC15.2 Release build at exactsourcea4f29029004fdb23ce63e3b0e19b3c2ecef426b1 fails src/cpu.c:2320 with -Werror=conversion. The new constuint8_t transform_count sums individually cast uint8 operands; integer promotion makes the sum int, which is implicitly narrowed on assignment. Clang native tests passed, but the GNU lane did not run tests because compilation failed. The sequential LinuxClang lane was not started. This is a local failure under existing fix authority, not a new hosted-CI stop. Raw log:build/phase05-plan11/linux-gcc.log.
**Fix:** Use an unsigned arithmetic/count type large enough for the bounded sum of boolean flags, preserving the mutually-exclusive-transform validation. Add no suppression or compiler flag change. Rebuild GNU and verify the corrected final source on the normal/sanitizer lanes. This is root-observed compiler evidence; the independent reviewer did not run tests.
