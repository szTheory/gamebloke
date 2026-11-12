---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-12T02:01:34Z
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
  warning: 2
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 05 Plan 06: Code Review Report

**Reviewed:** 2026-11-12T02:01:34Z  
**Depth:** standard  
**Files Reviewed:** 5  
**Status:** issues_found

## Summary

Reviewed the Plan06 CPU transaction refactor, SWP/SWPB tests, and their CTest/CI/ASan registration. The current SWP path captures operands, reads before writing, computes its fetch and data costs, and reserves the tested cycle, trace, sequence, and retirement limits before effects. I found no concrete C17 overflow or bounds defect in the exercised paths. Two verification gaps remain around the declared maximum descriptor and the SWP lane/alias matrix. This review does not claim LDM/STM execution or close phase-wide CPU/diagnostic requirements.

## Warnings

### WR-01: Maximum transaction descriptor is not exercised by reservation tests

**Severity:** WARNING  
**File:** `tests/cpu_test.c:2907-3032`  
**Issue:** These boundary tests execute the real SWP descriptor, which has two data slots and one sequential fetch. The implementation declares capacity for 16 data/result/register effects, two fetches, and 19 trace records (`src/cpu.c:22-104`), but no test constructs that maximum-work descriptor and runs it through the same checked cost, preflight, and trace-reservation logic (`src/cpu.c:1472-1566`). The compile-time capacity assertions prove the constants, not that full-capacity cycle/trace/sequence/counter reservations or one-over-capacity refusal are correct. This leaves the Plan06 maximum-work-boundary contract unverified while every real-SWP boundary test can pass.

**Fix:** Add a test-only maximum descriptor and exercise the same side-effect-free reservation path with 16 data slots, two refill fetches, and the maximum effects/results. Check exact-fit and one-short cycle, master-cycle, retirement, trace-capacity, and trace-sequence limits, plus refusal when any count exceeds its bound, without executing bus operations.

### WR-02: SWP lane and byte-alias vectors cover only one address lane and word aliases

**Severity:** WARNING  
**File:** `tests/cpu_test.c:2474`  
**Issue:** `prepare_swp_machine` always sets Rn to `0x03000041`, so every word-SWP semantic and alias vector uses lane 1 and only pins the ROR-by-8 result. Lanes 0, 2, and 3 are untested. The alias cases at `tests/cpu_test.c:2741-2755` use word-SWP encodings only; the SWPB case at `tests/cpu_test.c:2758-2803` uses distinct Rn/Rd/Rm. A lane-specific rotation error or a byte-width alias/captured-operand regression could therefore pass the new suite, despite the plan requiring independently pinned byte/word lane and alias behavior.

**Fix:** Parameterize the fixture address, add independently computed word-SWP results for all four address lanes, and add SWPB vectors for Rd/Rn/Rm alias combinations while checking the exact byte address and width.

---

_Reviewed: 2026-11-12T02:01:34Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
