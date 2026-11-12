---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-12T20:05:34Z
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
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 05 Plan 12: Code Review Report

**Reviewed:** 2026-11-12T20:05:34Z
**Depth:** deep
**Files Reviewed:** 5
**Status:** issues_found

## Summary

Reviewed the Plan 05-12 source changes from `95ec9aa` through `55cf4c2`. The block-transfer decoder's IA/IB/DA/DB start and writeback formulas match ARM ARM DDI0100I A5.4.2–A5.4.5, and the N/S requests and LDM internal/refill costs match ARM7TDMI DDI0029G §6.1, Tables 6-12, 6-13, and 6-23. The bounded unit preflight includes the 16 data slots, up to two refills, trace records, post-data cycles, and machine counters before effects. The PC refill resolves from the actual final LDM result and wraps through unsigned address arithmetic.

One warning remains in the four-mode test oracle: its STM cases can pass without performing stores, and its expected start address repeats the production formula. The separate guest stream verifies an STMIA case, but it does not independently cover the other store modes. This review did not run tests. The Plan 12 summary reports native CI and AppleClang ASan/UBSan passes; root-owned pinned Linux verification and the full Phase 05 CPU/diagnostic review remain pending.

## Narrative Findings (AI reviewer)

### Warnings

### WR-01: Four-mode STM cases can pass when stores are missing

**File:** `tests/cpu_test.c:2262-2306`
**Severity:** WARNING
**Category:** code quality / test reliability
**Issue:** `arm_block_start` reproduces the production P/U address expression at `src/cpu.c:2171-2174`, so a shared wrong start-address formula can place both test data and implementation accesses at the same incorrect addresses. In every store case, `arm_block_transfer_case` also preloads those addresses with the exact values expected after the store, then checks those unchanged values after execution. Consequently, a missing or mode-specific no-op store in the IA/IB/DA/DB matrix still passes. The separate `STMIA`-then-`LDMIA` stream proves one IA-with-writeback path, but the matrix does not independently prove the other store modes.
**Fix:** Use an independent expected-address table with explicit per-mode offsets (for the two-register list: IA = base, IB = base + 4, DA = base - 4, DB = base - 8). For STM cases, initialize destination words to distinct sentinels and assert that execution replaced them with the captured source values at those literal addresses; retain the existing independent load values and writeback assertions.

---

_Reviewed: 2026-11-12T20:05:34Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
