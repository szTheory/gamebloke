---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "08"
reviewed: 2026-11-12T13:32:19Z
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
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05 Plan 08: Code Review Report

**Reviewed:** 2026-11-12T13:32:19Z
**Depth:** standard
**Files Reviewed:** 5
**Status:** clean

## Summary

Re-reviewed the four PSR test-coverage warnings against the tests-only follow-up at `673a7dc`; each missing acceptance case now has a distinguishing guest instruction and independent state expectation. The subsequent hosted LeakSanitizer finding WR-05 is fixed in `819aac8`: the fixture now disposes the owned ROM and checks allocation balance, including a refused allocation. The source review has no open findings. Native normal and AppleClang ASan/UBSan logs show all 29 CPU CTest cases passing after the fix. The root-owned pinned Linux/Clang 21 sanitizer lane and corrected hosted CI run are still pending, so this report does not mark the hosted gate or full phase clean.

## Narrative Findings (AI reviewer)

No open findings. The four earlier coverage warnings and the later WR-05 lifetime finding are retained below as resolved history.

## Resolved Findings

### WR-01: Unallocated-bit rejection is not tested before field masking

**Prior severity:** WARNING
**Status:** RESOLVED by `673a7dc`
**File:** `tests/cpu_test.c:2551-2600`, `2840-2931`
**Resolution:** Zero-field register writes now pair legal `R0 = 0x13` retirement with the refusal for `R0 = 0x113`, where reserved bit 8 lies outside every selected field (`2849-2875`). The refusal asserts canonical machine state, instance identity, trace metadata, and initialized trace-record bytes (`2911-2930`). This distinguishes the required raw `UnallocMask` check from field-first masking.

### WR-02: The CPSR StateMask guard is only tested with the control field selected

**Prior severity:** WARNING
**Status:** RESOLVED by `673a7dc`
**File:** `tests/cpu_test.c:2840-2931`
**Resolution:** The added `MSR CPSR_f, R0` case uses `R0 = 0x33`: the mode bits encode valid SVC, T is set, and c is unselected (`2850-2875`). It checks refusal before effects. The allowed SPSR T-write remains covered at `tests/cpu_test.c:2801-2812`.

### WR-03: The saved-status MSR path lacks register-source and flags-field coverage

**Prior severity:** WARNING
**Status:** RESOLVED by `673a7dc`
**File:** `tests/cpu_test.c:2465-2547`, `2603-2675`
**Resolution:** Real SPSR register-source `c` writes and rotated-immediate `f` writes now run in FIQ, IRQ, SVC, Abort, and Undefined modes. Explicit expected constants verify the selected SPSR bank, preserved unselected bits, unchanged active CPSR, and unchanged other SPSR banks. Composed `c|f` register and immediate cases also exercise saved-status updates.

### WR-04: Zero and composed PSR field masks have no distinguishing positive case

**Prior severity:** WARNING
**Status:** RESOLVED by `673a7dc`
**File:** `tests/cpu_test.c:2551-2675`
**Resolution:** Zero-field register and immediate writes now retire as no-ops for both CPSR and SPSR. Composed `c|f` register and rotated-immediate writes now check independent expected values for both targets, including preserved x/s canaries and untouched SPSR banks.

### WR-05: PSR bank-visibility fixture leaks its owned ROM

**Prior severity:** WARNING
**Status:** RESOLVED by `819aac8`
**File:** `tests/cpu_test.c:53-80`, `2762-2815`
**Resolution:** The fixture captures the bank-visibility result before cleanup, verifies the successful 192-byte ROM allocation is live, forces the next allocation to fail and checks the returned resource/allocation status without changing live counts or bytes, then unconditionally calls `gbl_machine_dispose` and checks both live totals return to zero. Source tracing confirms disposal releases the ROM with the stored allocator context and exact length, marks the machine uninitialized before release, and safely no-ops for an uninitialized or already-disposed machine. The allocation-balance check fails in the recorded pre-cleanup negative control.

## Verification Evidence and Limits

- Existing logs show all 29 registered CPU CTest cases passing in the main normal and AppleClang ASan/UBSan builds and in the isolated Linux GCC clone. Focused `cpu.psr` and `cpu.arm_pc_return` runs passed in both main configurations; the summary addendum records 41 and 10 assertions respectively.
- The review checked that canonical-state comparisons compare named fields and contents rather than struct padding. The refusal test's record-buffer `memcmp` is intentional: the whole record array is initialized before the call and is compared to that byte snapshot afterward.
- The full project CI workflow and full ASan inventory were not rerun for this test-only follow-up. Earlier full-suite receipts remain historical evidence for the unchanged implementation. No hardware-accuracy or broader compatibility claim is made.
- After `819aac8`, local normal and native AppleClang ASan/UBSan logs show focused `cpu.psr` passing and all 29 registered CPU CTest cases passing. The saved pre-cleanup negative control fails the new allocation-balance assertion. These are local results, not substitutes for the hosted Linux/Clang 21 LeakSanitizer configuration.
- The original hosted failure remains historical evidence: run `43167646132`, sanitizer job `131013212024`, reported the 192-byte leak that led to WR-05. The matching pinned Linux/Clang 21 full sanitizer lane and corrected hosted run are root-owned and pending; this review makes no claim that either has passed. Plan 08's source review is clean, but the hosted gate and full phase remain unresolved.

---

_Reviewed: 2026-11-12T13:32:19Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_


### Root verification addendum (after independent source review)

The independent reviewer did not run tests. Root subsequently confirmed focused cpu.psr, all 78 required sanitizer checks and the post-inventory check passed with zero skips on pinned Ubuntu 26.04 native arm64/Clang 21 at `819aac865e472a8d12d55716c2fef864f249db2b`. This resolves the local Linux verification item above; the corrected hosted run is pending. No phase-wide clean claim follows.
