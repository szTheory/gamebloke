---
phase: 05-18
fixed_at: 2026-11-13T01:46:41Z
review_path: .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-18-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 05-18: Code Review Fix Report

**Verification ran in:** the main checkout, `/Users/jon/projects/gamebloke`.

## Fixed issues

### CR-01: Annul failed ARM conditions before decode stops

**Files modified:** `src/cpu.c`, `tests/cpu_test.c`  
**Commit:** `5d3501b`  
**Applied fix:** The common decoder now resolves failed ARM conditions 0–14 as sequential no-ops after static fetch checks and before operand/extension classification. Condition 1111 retains its existing NV and extension reporting behavior. Regression coverage compares failed-EQ reserved-Rn MOV and CLZ against legal no-op execution, checking retirement, cycles, fetch sequence, CPU banks, flags, stop/exception state, trace capacity and canonical state; passing-EQ companions retain their existing refusal classifications.

**Negative control:** `build/phase05-plan18/review-fix/condition-red.tap` shows new checks 69 and 70 failing before the decoder change; `condition-green.tap` shows both passing afterward, with the positive stop controls still passing.

**Verification:** Focused CTest selectors passed 11/11 in both `build/ci` and `build/asan` (configured with `/usr/bin/clang`, AddressSanitizer and UndefinedBehaviorSanitizer). The selection covered ARM condition/ALU/classification/decode/exception groups and runner unit, result-contract, relocation, tracer, hash and version checks. Receipts are in each build tree’s `Testing/Temporary/LastTest.log`.

**Status:** Fixed; this decoder behavior change is a logic correction and remains subject to human review.

### WR-01: Correct Thumb format mapping

**Files modified:** `profiles/agb.json`, `docs/profile-agb.md`, `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake`, `05-18-SUMMARY.md`  
**Commit:** `a30f05e`  
**Applied fix:** The policy rows now identify formats 6–12 as PC-relative load, register-offset transfers, signed transfers, immediate-offset transfers, halfword transfers, SP-relative transfers and PC/SP address formation; formats 13–19 now identify SP adjustment, PUSH/POP, STMIA/LDMIA, conditional B, SWI, unconditional B and BL. The summary crosswalk uses the corrected 6–12 and 13–19 ranges. Profile field counts, types, and policy labels are unchanged.

**Negative controls:** `cpu_policy_value` rejects the former 6–14 row in the profile; `cpu_policy_doc` rejects a stale 6–14 row in the documentation. Both cases also pass their clean-input checks.

**Verification:** `cmake/CheckProfile.cmake` passes with `profile: ok (agb v5, 11 timing constants, 11 unmodeled)`. The focused normal and ASan runs above include the ARM/runner regressions. After both commits, `sh build/phase05-plan18/final-gnu-build-only.sh` passed at HEAD `a30f05e2c907c6a38bcfc70abf5cbf416a86ecec` using pinned GNU 15.2 on aarch64; it compiled the requested CPU and runner targets without running inventories. The helper emitted developer warnings for absent ignored diagnostic ROM paths during fixture-index generation, then reported the index valid and completed the build successfully.

---

_Fixed: 2026-11-13T01:46:41Z_  
_Fixer: gsd-code-fixer_  
_Iteration: 1_
