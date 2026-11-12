---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "07"
subsystem: cpu
tags: [arm7tdmi, armv4t, operand2, arithmetic-flags, shifters, conditions]
requires:
  - phase: 05-06
    provides: Canonical preflighted CPU transaction path, decoded units, and bus-owned time
provides:
  - All 16 ARM data-processing operations with unsigned carry, borrow, overflow, and exact flag masks
  - Legal ARM Operand2 shifts, condition predicates, PC operand behavior, and register-shift internal work
  - Explicit cond=1111 UNPREDICTABLE refusal and normal-condition ARMv4T undefined-opcode coverage
affects: [05-08]
actuals:
  tokens: 12316
  tasks: 2
  commits: 8
  plan_head_before: f62d2ffbefc4ab61ce34284c99aca1bc75abb123
  plan_head_after: 7e0d1bf1f9ae6b05ab84d5501d170fe7db713443
tech-stack:
  added: []
  patterns:
    - One canonical ephemeral CPU decode, preflight, and execute path
    - Unsigned bit-vector flag helpers avoid signed overflow and invalid shifts
    - Decoded CPU internal work is charged through bus-owned time
key-files:
  created: []
  modified:
    - src/cpu.c
    - src/cpu_helpers.h
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/fixtures/instruction_image.c
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - Condition 1111 is refused as UNPREDICTABLE before machine effects.
  - E7F000F0 is the normal-condition ARMv4T undefined fixture; F1A00000 belongs to the reserved cond=1111 space.
patterns-established:
  - ARM data-processing tests execute real instruction streams through gbl_cpu_run and assert independent result, flags, trace, and refusal postconditions.
  - Register-specified shifters use a source-referenced internal-cycle table and canonical bus advancement.
requirements-completed: []
coverage:
  - id: D1
    description: All sixteen ARM ALU opcodes have independent result and flag checks, including test-only opcodes and carry/overflow boundaries.
    verification:
      - kind: unit
        ref: "build/phase05-plan07/task2-green-alu.tap; build/ci/cpu_test arm_alu"
        status: pass
    human_judgment: false
  - id: D2
    description: Legal immediate and register shifters, all ARM predicates, PC operand behavior, cycle bounds, and cond=1111 refusal are covered.
    verification:
      - kind: unit
        ref: "build/phase05-plan07/task2-green-shifts.tap; build/ci/cpu_test arm_shifts_conditions"
        status: pass
    human_judgment: false
  - id: D3
    description: A normal-condition ARMv4T undefined encoding reaches the existing UND and no-BIOS result paths.
    verification:
      - kind: integration
        ref: "build/phase05-plan07/undefined-green-no-bios.tap; build/phase05-plan07/undefined-green-decode.tap; build/phase05-plan07/undefined-green-runner-ctest.log"
        status: pass
    human_judgment: false
duration: 54min
completed: 2026-11-12
status: complete
---

# Phase 5 Plan 7: ARM ALU, conditions, and shifter flags Summary

**All ARM data-processing operations now execute through the canonical CPU path with tested NZCV behavior, Operand2 shifters, condition handling, and bounded bus-charged internal work.**

## Performance

- **Duration:** about 54 minutes from the first task commit through verification
- **Started:** 2026-11-12T03:09:31Z (first task commit)
- **Completed:** 2026-11-12T04:03:55Z
- **Tasks:** 2
- **Files modified:** 7 implementation, test, and inventory files

## Accomplishments

- Implemented AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, and MVN with unsigned carry/borrow/overflow logic, exact flag masks, no-destination test behavior, and old-C/V preservation where required.
- Implemented legal ARM immediate and register shifter forms, all fourteen conditions plus AL, register-shift PC+12 behavior, condition-failed retirement, and an exact/one-short bus-preflighted internal cycle.
- Refused ARM condition 1111 as UNPREDICTABLE before effects and retained a separate genuine ARMv4T undefined instruction path.

## Test Evidence

- Task 1 began with a failing real-stream ADC carry-in assertion; its independent ALU vectors pass in `arm_alu` (18/18 assertions).
- Task 2 began with a failing shifted-PC vector and expanded to two concrete failures: shifted-PC+12 and condition-1111 classification. The completed shifter/condition selector passes 73/73 assertions. Its timing bounds, condition sentinels, and refusal state/buffer checks pass.
- Fixture synchronization first failed on the stale F1A00000 no-BIOS/decode expectations. After updating the project-owned fixture generator to E7F000F0, no-BIOS passed 2/2, decode passed 8/8, and the runner result-contract, decode, and no-BIOS CTests passed 3/3.
- Canonical local `cmake --workflow --preset ci` passed: 332 checks ran, with only the existing allowed owner-logo check skipped because `GBL_OWNER_LOGO_FILE` was unset; post-inventory validation passed.
- Native AppleClang ASan/UBSan passed 76/76 CTests with no skips; its post-inventory check passed.
- Isolated Linux GCC canonical CI passed on committed source `7e0d1bf`: 332 checks ran, with only the same allowed owner-logo check skipped; post-inventory validation passed.
- Plan 06 hosted run [43144072990](https://github.com/szTheory/gamebloke/actions/runs/43144072990) is green at its earlier head `f62d2ffbefc4ab61ce34284c99aca1bc75abb123`. This is not Plan 07 hosted evidence; no Plan 07 hosted run has been triggered.
- Detailed ignored receipts are under `build/phase05-plan07/`; native sanitizer reports are under `build/phase05-plan07/native-asan-*` and `build/asan/`.

## Task Commits

1. **Task 1: ADC carry-in RED** — `80b969d` (test)
2. **Task 1: independent ALU vectors** — `3a1d75b` (test)
3. **Task 1: ARM ALU implementation** — `7b071e6` (feat)
4. **Task 2: shifted-PC RED** — `69ef0f5` (test)
5. **Task 2: shifter and condition vectors** — `76d6bb4` (test)
6. **Task 2: shifter, condition, and cond=1111 implementation** — `05bcdc0` (feat)
7. **Authorized fixture synchronization: use a normal-condition undefined encoding** — `225d86a` (test)
8. **Authorized fixture synchronization: classify ARMv4T undefined opcode space** — `7e0d1bf` (fix)

## Files Created/Modified

- `src/cpu.c` — ARM ALU, Operand2 shifts, conditions, undefined-opcode classification, and internal-cycle handling.
- `src/cpu_helpers.h` — unsigned ALU and shift helper declarations shared by the CPU tests.
- `tests/cpu_test.c` — real-stream ALU, shifter, condition, cycle, and refusal vectors.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — selector and inventory registration.
- `tests/fixtures/instruction_image.c` — normal-condition ARMv4T undefined fixture encoding.

## Decisions Made

- Condition 1111 remains globally classified as UNPREDICTABLE and is refused before any machine or caller-buffer effects.
- ARM ARM DDI0100I A3.16.5 documents the fixed undefined pattern; E7F000F0 matches it with condition AL. The former F1A00000 fixture has condition 1111 and is therefore not an undefined-opcode test under the accepted policy. ARM7TDMI implements ARMv4T (DDI0029G §1.4).

## Deviations from Plan

### Authorized contract and fixture synchronization

The newly applied global condition-1111 policy exposed that the existing synthetic undefined fixture `F1A00000` was encoded in the reserved condition space. The no-BIOS and decode controls consequently expected the wrong classification. Replaced it with source-documented `E7F000F0`, retained normal-condition UND/no-BIOS/runner coverage, and added explicit cond=1111 no-effect refusal checks. The adjustment changes only the project-owned synthetic fixture generator and CPU/test controls. It does not change result schemas, rights metadata, diagnostic producer recipes, or the frozen diagnostic inputs.

- **Files:** `src/cpu.c`, `tests/cpu_test.c`, `tests/fixtures/instruction_image.c`
- **Verification:** fixture RED and GREEN receipts above; full local CI, native ASan/UBSan, and isolated Linux GCC CI passed.
- **Commits:** `225d86a`, `7e0d1bf`
- **Impact:** required to keep the accepted cond=1111 contract and the independent undefined-instruction control consistent.

## Issues Encountered

The initial native ASan post-inventory command expected `build/asan/junit.xml` while the fresh report was stored under the ignored Plan07 evidence directory. Copied that generated report into the configured location and reran only the post-inventory check; all 76 unit CTests had already passed and post-inventory then passed. No source change was needed.

## User Setup Required

None.

## Next Phase Readiness

Plan 07 completes only this ARM ALU/condition/shifter slice. It does not establish full ARM7TDMI coverage, wait-state timing accuracy, broad compatibility, or hardware accuracy. CPU-01/CPU-02 and DIAG-01/02/03 remain phase-wide pending, and `requirements-completed` is intentionally empty. Plan 08 and final phase acceptance remain with the orchestrator; no Plan 07 hosted result is claimed.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists at the planned path.
- All eight listed task commits are ancestors of the current plan head.
- The measured pre-summary plan head is `7e0d1bf1f9ae6b05ab84d5501d170fe7db713443` and the plan contains eight commits since `f62d2ffbefc4ab61ce34284c99aca1bc75abb123`.
