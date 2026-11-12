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


## Subsequent hosted finding

This report records completion of the original four coverage-warning fixes. A subsequent hosted Linux LeakSanitizer result found the separate WR-05 fixture-lifetime issue. At report creation it had not been fixed or retried; see 05-08-CI-HANDOFF.md. Its later local correction and current verification are recorded below. The historical hosted failure remains unresolved until the root-owned Linux/Clang 21 verification and corrected hosted run complete.

## WR-05: PSR bank-visibility fixture leaks its owned ROM

**Status:** Source fix committed as `819aac8`; local verification passed.
**Files modified:** `tests/cpu_test.c`
**Applied fix:** Added successful-allocation, live-allocation, and live-byte accounting to the existing CPU test allocator seam. The bank-visibility fixture now proves its 192-byte ROM allocation is live before cleanup, confirms a forced failed allocation does not change those counts, then unconditionally calls `gbl_machine_dispose` and proves live allocation count and bytes return to zero. Updated the PSR assertion plan from 41 to 42.

**Negative control:** Before adding disposal, the focused `cpu.psr` test failed at the allocation-balance assertion. Output is saved at ignored `build/phase05-plan08/leak-fix/red.log`.

**Verification:** All local checks ran in the main checkout. The native Apple Clang 21 normal build passed focused `cpu.psr` and all 29 CPU CTest cases; logs are `build/phase05-plan08/leak-fix/normal-psr-final.log` and `normal-cpu-final.log`. The native Apple Clang 21 ASan/UBSan build passed focused `cpu.psr` and all 29 CPU CTest cases; logs are `appleclang-asan-psr-final.log` and `appleclang-asan-cpu-final.log` in the same ignored directory. The Linux/Clang 21 full sanitizer lane and hosted retry are owned by the root agent and were not run here.


### Root Linux verification after source review

Pinned Ubuntu 26.04 native arm64 with Clang 21 at exact source `819aac865e472a8d12d55716c2fef864f249db2b` passed focused cpu.psr, all 78 required ASan/UBSan/LeakSanitizer checks and inventory postcheck (zero skips). Deliberate UBSan/ASan failure controls passed; no suppression or detector disable was used. Full CTest time 153.09 seconds. The local architecture is arm64; hosted x64 is still pending. Raw log/helper: `build/phase05-plan08/leak-fix/linux-clang-sanitizers.log` and `linux-clang-sanitizer-verification.sh`.


## Corrected hosted gate — resolved

The one owner-approved corrected run43228795941 passed CI required and every dependency at source29966518ff112968569358e83fca5014aaf991bd. The Linux x64 Clang sanitizer inventory passed all78 with zero skips, both regression hosts passed with only the existing allowed owner-local skip, both diagnostics hosts and hygiene passed. All hosted jobs remained within180s; measured critical-path review and raw logs are in 05-08-CI-HANDOFF.md and build/phase05-plan08/leak-fix/. The original hosted leak is resolved. Resume authorized Plan09; full-phase review and phase-wide CPU/DIAG completion remain later. Any new hosted red renews the stop.
