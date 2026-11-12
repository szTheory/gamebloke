---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
fixed_at: 2026-11-12T21:50:06Z
review_path: .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-13-REVIEW.md
iteration: 1
findings_in_scope: 1
fixed: 1
skipped: 0
status: all_fixed
---

# Phase 05 Plan 13: Code Review Fix Report

**Fixed at:** 2026-11-12T21:50:06Z  
**Source review:** `.planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-13-REVIEW.md`  
**Iteration:** 1

## Summary

- Findings in scope: 1
- Fixed: 1
- Skipped: 0

## Fixed Issue

### WR-01: S-bit tests did not distinguish the current-mode base from the User transfer bank

**Files modified:** `tests/cpu_test.c`  
**Commit:** `fc83cf4`  
**Applied fix:** Added paired privileged FIQ `STMIA^` and `LDMIA^` cases with `Rn=R8`, `S=1`, and `W=0`. The fixture gives FIQ R8 (`0x03000300`) and User R8 (`0x03000380`) different valid IWRAM addresses. Literal trace and memory checks prove the FIQ base chooses transfer addresses while STM sources and LDM destinations use the User bank. Distinct sentinels at both address pairs expose a wrong-bank access. The tests also assert FIQ R8 and R9 remain unchanged, and that User R8/R9 retain or receive the expected transfer values. The `arm_block_banks` group grows from 19 to 21 assertions. Production source was not changed.

**Negative control:** In an ignored copied-source build, the block decoder was mutated to select User R8 as the base for banked S-bit forms. The mutant compiled successfully and failed both new assertions (STM^ #3 and LDM^ #4); the unmodified copy passed both and the full 21-assertion selector. Logs are `build/phase05-plan13/review-fix/user-base-mutant.log` and `user-base-clean.log`; the isolated control is `user-base-control.cmake`.

## Verification

Verification ran in the main checkout using the existing native build trees.

- Normal build: `cmake --build build/ci --target cpu_test` passed.
- Normal focused test: `ctest --test-dir build/ci --output-on-failure -R '^cpu\.arm_block_banks$'` passed (1/1).
- AppleClang ASan/UBSan build: `cmake --build build/asan --target cpu_test` passed; the existing cache uses `/usr/bin/clang`.
- AppleClang ASan/UBSan focused test: `ctest --test-dir build/asan --output-on-failure -R '^cpu\.arm_block_banks$'` passed (1/1).
- `git diff --check` passed before commit.

No full inventory, Linux lane, Docker run, or hosted run was started for this test-only fix. Root owns integration verification.

---

_Fixed: 2026-11-12T21:50:06Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_
