---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
fixed_at: 2026-11-12T18:40:25Z
review_path: .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-11-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 05 Plan 11: Code Review Fix Report

**Fixed at:** 2026-11-12T18:40:25Z  
**Source review:** `.planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-11-REVIEW.md`  
**Iteration:** 1

## Summary

- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### WR-02: GCC rejects implicit narrowing of transform-count arithmetic

**Files modified:** `src/cpu.c`  
**Commit:** `4c3ed1b`  
**Applied fix:** The bounded sum of four boolean transform flags now uses `unsigned int`; the existing mutual-exclusion and width/index checks remain intact. No compiler flags or warning suppressions changed.

**Compiler failure provenance:** The root-observed pinned Ubuntu 26.04 native ARM64 GCC 15.2 build at source `a4f29029004fdb23ce63e3b0e19b3c2ecef426b1` failed at `src/cpu.c:2320` under `-Werror=conversion`. The raw evidence is `build/phase05-plan11/linux-gcc.log`. Compilation stopped before Linux tests ran, and the sequential Linux Clang lane was not started. This agent did not rerun those Linux lanes; root owns final GCC and Clang verification.

### WR-01: Addressing tests did not cover both U polarities for each P/W form

**Files modified:** `tests/cpu_test.c`  
**Commit:** `69f3095`  
**Applied fix:** Added paired U=+ and U=- rows for immediate and register offset, pre-index, and post-index addressing. All four transfers (LDRH, STRH, LDRSB, LDRSH) use nonzero offsets and retain independent effective-address, writeback, and data checks. The matrix adds 24 assertions; the `arm_subword` group count is now 70.

**Negative control:** In an ignored build-directory copy only, the register-offset decoder was mutated to force U=+. The isolated 70-assertion selector failed the new LDRH register-offset U=- assertion 8 while its U=+ companion assertion 7 passed. The unmodified copy passed the full selector. Logs are `build/phase05-plan11/review-fix/wrong-u-mutant.log` and `wrong-u-clean.log`; the isolated control script is `wrong-u-control.cmake`.

## Verification

Verification ran in the main checkout at source commits `4c3ed1b` and `69f3095`, using the existing native build trees.

- Normal build: `cmake --build build/ci --target cpu_test` passed.
- Normal focused tests: `ctest --test-dir build/ci --output-on-failure -R '^cpu\.(arm_subword|arm_subword_alignment)$'` passed both selectors (2/2) after each finding.
- AppleClang ASan/UBSan build: configured with `-DCMAKE_C_COMPILER=/usr/bin/clang`; `cmake --build build/asan --target cpu_test` passed.
- AppleClang ASan/UBSan focused tests: `ctest --test-dir build/asan --output-on-failure -R '^cpu\.(arm_subword|arm_subword_alignment)$'` passed both selectors (2/2) after each finding.
- `git diff --check` passed before the tests-only commit.

The final Linux GCC build and the full Linux/Clang sanitizer inventories remain with root. No full local suite, Docker run, hosted run, or emulator-policy change was made for these review fixes.

---

_Fixed: 2026-11-12T18:40:25Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_


## Root closure

Independent re-review is clean. At exact final source `69f309546c4a61fa429500edbe587532096799d5`, full pinned Ubuntu 26.04 native arm64 GCC canonical CI passed 340 tests plus the existing one allowed owner-local skip and its inventory postcheck. The sequential Clang 21 ASan/UBSan/LeakSanitizer lane passed 84/84 with zero skips and postcheck. The initial GNU compiler failure is resolved without changing flags or suppressing warnings. Raw logs/receipts: `build/phase05-plan11/review-fix/final-linux-*`. Routine publication remains deferred to the phase's explicit hosted-witness/final gates; no hosted claim is made for this local source.
