---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
fixed_at: 2026-11-12T05:59:11Z
review_path: .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-08-REVIEW.md
iteration: 1
findings_in_scope: 4
fixed: 4
skipped: 0
status: all_fixed
---

# Phase 05 Plan 08: Code Review Fix Report

**Fixed at:** 2026-11-12T05:59:11Z
**Source review:** .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-08-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 4
- Fixed: 4
- Skipped: 0

## Fixed Issues

### WR-01: Unallocated-bit rejection is not tested before field masking

**Files modified:** `tests/cpu_test.c`
**Commit:** `673a7dc`
**Applied fix:** Added paired zero-field register-source vectors: allocated `R0 = 0x13` retires without changing status, while `R0 = 0x113` refuses because reserved bit 8 is outside the selected fields. The refusal checks canonical machine state, ownership identity, each trace metadata field, and the initialized raw record buffer.

### WR-02: The CPSR StateMask guard is only tested with the control field selected

**Files modified:** `tests/cpu_test.c`
**Commit:** `673a7dc`
**Applied fix:** Added privileged `MSR CPSR_f, R0` refusal with `R0 = 0x33` in SVC mode. Its allocated mode bits are valid, while T is set with c unselected. The existing allowed SPSR T-write case remains.

### WR-03: The saved-status MSR path lacks register-source and flags-field coverage

**Files modified:** `tests/cpu_test.c`
**Commit:** `673a7dc`
**Applied fix:** Added real-instruction SPSR register-source c-field writes and rotated-immediate f-field writes in FIQ, IRQ, SVC, Abort, and Undefined modes. Explicit expected status constants check the selected SPSR bank, saved flags/control preservation, active CPSR stability, and all other SPSR banks.

### WR-04: Zero and composed PSR field masks have no distinguishing positive case

**Files modified:** `tests/cpu_test.c`
**Commit:** `673a7dc`
**Applied fix:** Added retiring no-op vectors for zero-field register and immediate writes to CPSR and SPSR. Added c|f register and rotated-immediate writes for CPSR and SPSR with independent status constants and preserved x/s-byte canaries and unselected banks.

## Verification

- Main checkout, normal build: all 29 registered CPU CTest cases passed; after the final test-only fixture cleanup, focused `cpu.psr` (41 assertions) and `cpu.arm_pc_return` (10 assertions) passed.
- Main checkout, native AppleClang ASan/UBSan: all 29 registered CPU CTest cases passed; after the final cleanup, the same focused groups passed.
- Isolated Ubuntu 26.04/GCC source clone at commit `673a7dc`: build of `cpu_test` and all 29 registered CPU CTest cases passed.
- Logs are under ignored `build/phase05-review08/`: `normal-cpu-ctest.log`, `asan-cpu-ctest.log`, `normal-focused-ctest.log`, `asan-focused-ctest.log`, and `linux-gcc-cpu-ctest.log`.
- This was test-only; `src/cpu.c` is unchanged. No full CI/ASan inventory run was repeated, and these checks make no hardware-accuracy claim.

---

_Fixed: 2026-11-12T05:59:11Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
