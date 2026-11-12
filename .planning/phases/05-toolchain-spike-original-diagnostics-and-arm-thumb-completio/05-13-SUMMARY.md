---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "13"
subsystem: cpu
tags: [arm7tdmi, armv4t, block-transfer, banked-registers, gba]
requires:
  - phase: 05-12
    provides:
      - bounded ordered ARM block-transfer unit and exact cycle/trace preflight
      - canonical user-bank and SPSR return integration seams
provides:
  - S-bit User-bank STM/LDM and SPSR-restoring LDM^ PC transfers
  - ARMv4 empty-list PC transfers and base-in-list writeback policy
  - arm_block_banks and arm_block_edges selector coverage
affects: [05-14, cpu-diagnostics, arm-block-transfers]
actuals:
  tokens: 16041
  tasks: 2
  commits: 5
  plan_head_before: dae2277078c03f4f44a9630fdb2d7f187339a49a
  plan_head_after: ac9ebe6877fa1587b2bfc986d6b7e4adcc537b70
tech-stack:
  added: []
  patterns:
    - captured original register-bank and SPSR views in one bounded CPU unit
    - independently asserted literal transfer addresses and refusal postconditions
key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - "Use ARM ARM DDI0100I for S-bit form legality and ARM7TDMI DDI0029G for exact-core mode and bank topology; keep their distinct scopes explicit."
  - "Adopt GBATEK 3.06's ARMv4 empty-list and base-in-list behaviors as GBA reference policy, not as a hardware measurement."
  - "For the empty-list PC slot, apply the normal first-transfer P/U address formula and the documented +/-0x40 base adjustment; record this mapping as an implementation policy."
patterns-established:
  - "Resolve all register-bank sources and destinations before effects; restore CPSR before the result-driven, state-width-correct refill."
  - "Use literal absolute addresses and separate memory/caller-buffer assertions for CPU transfer tests."
requirements-completed: []
coverage:
  - id: D1
    description: "S-bit User-bank transfers and LDM^ PC exception return use captured banks and restored refill state."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_block_banks"
        status: pass
      - kind: unit
        ref: "ctest cpu.arm_block_banks under native AppleClang ASan/UBSan"
        status: pass
    human_judgment: false
  - id: D2
    description: "Empty-list and base-in-list ARMv4 block policies use ordered slots and atomic capacity refusal."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_block_edges"
        status: pass
      - kind: unit
        ref: "ctest cpu.arm_block_edges under native AppleClang ASan/UBSan"
        status: pass
    human_judgment: false
duration: "5h 33m minimum from first committed RED; pre-commit start time was not recorded"
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 13: ARM block list and bank edge policies

**ARM block transfers now preserve captured register banks through exception return and execute the selected GBA ARMv4 empty-list and base-alias policies.**

## Performance

- **Duration:** At least 5h 33m, measured from the first committed RED; work before that commit was not timed.
- **Started:** 2026-11-12 15:38:25 -05:00 (first recorded task commit).
- **Completed:** 2026-11-12.
- **Tasks:** 2.
- **Files modified:** 5.

## Accomplishments

- Implemented S=1 STM/LDM User-bank views and LDM^ PC exception return through the canonical bounded CPU unit. Non-PC loads use their original destination banks, then CPSR is restored before actual-result refill requests use the restored ARM or Thumb width.
- Added `arm_block_banks` coverage for FIQ and exception banks, invalid SPSR/mode refusals, User/System S-bit restrictions, the maximum 20-cycle/19-record return, one-short refusal, and the generated Thumb wrap pair followed by its explicit unpredictable stop.
- Implemented the selected ARMv4 empty-list policy as one PC transfer with +/-0x40 base adjustment. Added base-in-list STM old/final-base source selection and LDM writeback suppression while retaining the register-list load.
- Added `arm_block_edges` coverage for all four P/U modes, W on/off, base first/middle/last, S-bit store and exception-return forms, literal transfer addresses, PC+12 stores, wrapped full-list accesses, and exact/one-short trace capacity.

## Task Commits

1. **Task 1 RED:** `cc112d8` — tests for User-bank transfers and exception return.
2. **Task 1 implementation:** `f76f0f1` — S-bit User-bank block transfers and SPSR return.
3. **Task 2 RED:** `7803a28` — empty-list and base-in-list tests and selector inventory.
4. **Task 2 implementation:** `b54ec48` — empty-list and alias policies plus corrected caller-buffer comparison extent.
5. **Task 2 sanitizer-driven test fix:** `ac9ebe6` — bank-aware setup and reads for r8-r14 in the test matrix.

## Source policy and decisions

| Topic | Source and application |
| --- | --- |
| S-bit mode restrictions | ARM ARM DDI0100I A4.1.21, A4.1.22 and A4.1.98 define the User/System restrictions and W rules for User-bank forms. ARM7TDMI DDI0029G §2.5 identifies User as the only nonprivileged mode; §2.6.1 and Table 1-2 establish exact-core bank topology and distinguish LDM^PC restore-CPSR syntax from User-register forms. DDI0029G does not restate the S-bit legality rules, so its silence is not used to override the ARM ARM. |
| Empty list | ARM ARM DDI0100I A5.4 describes an empty list as UNPREDICTABLE. The GBA reference policy follows GBATEK 3.06, ARM block transfer “Strange Effects on Invalid Rlist’s”: R15 transfers and the base changes by +/-0x40 on ARMv4. For the one actual PC slot, this implementation applies the normal first-transfer P/U formula. That address mapping is an explicit model policy inferred from the documented PC transfer and normal address modes, not a physical measurement. |
| Base in list | GBATEK 3.06 documents STM storing the old base when it is the first register, otherwise the updated base, and ARMv4 LDM suppressing writeback. The loaded base register still receives its memory value. |

References: [ARM7TDMI Technical Reference Manual DDI0029G](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129), [ARM Architecture Reference Manual DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a), and [GBATEK 3.06 ARM block data transfer](https://mgba-emu.github.io/gbatek/#arm-opcodes-memory-block-data-transfer-ldm-stm).

## Verification

- Task 1 tracer, after its implementation commit: `cmake --preset ci && cmake --build --preset ci && build/ci/cpu_test arm_block_banks` — **19/19 passed**.
- Task 2 selector: `cmake --preset ci && cmake --build --preset ci && build/ci/cpu_test arm_block_edges` — **70/70 passed**.
- Focused native CI regressions for `cpu.arm_block`, `cpu.arm_block_banks`, `cpu.arm_block_bounds`, `cpu.arm_block_edges`, `cpu.arm_pc_return`, `cpu.arm_transfer`, and `cpu.loaded_pc` — **7/7 passed**.
- The same seven focused selectors under native AppleClang 21 ASan/UBSan from `/usr/bin/clang` — **7/7 passed**. JUnit output: `build/phase05-plan13/asan/junit.xml`.
- The root-owned pinned GNU build-only helper passed at both Task 1 commit `f76f0f1` and final Task 2 source `ac9ebe6`; the final run used Ubuntu 26.04/aarch64 and GCC 15.2 and built `cpu_test` only.
- No full native or sanitizer inventory and no hosted CI were run for this plan. Root owns independent review and the final pinned Linux integration gates.

## TDD Gate Compliance

- Task 1 tests were committed before implementation in `cc112d8`; the recorded RED evidence was accepted by `gsd_run check tdd-red-evidence`. The post-commit tracer then passed before Task 2 expansion.
- Task 2 tests were committed in `7803a28`. The unchanged TAP report at `build/phase05-plan13/task2-red.tap` discovered 70 tests; its named empty-list LDM IA assertion failed because the decoder rejected an empty list. The classifier returned `RED_EVIDENCE_OK` for that target. The report also contained two one-short assertions whose test helper compared 21 records against 7-record arrays; the buffer extent was fixed in `b54ec48` before final green verification.
- ASan/UBSan found that the base-alias test wrote r8 through the r0-r7 array. The test now uses the System/User bank for r8-r14; the fix is isolated in `ac9ebe6` and the focused sanitizer set passes.

## Deviations from Plan

### Auto-fixed test issues

1. **Rule 1 — Incorrect store trace assertion.** The first Task 1 test expected a bus store trace record to expose its data value. Memory contents already provide the independent store oracle; the invalid trace-value assertion was removed. **Committed:** `f76f0f1`.
2. **Rule 1 — Caller-buffer comparison extent.** Two new Task 2 refusal tests used 7-record arrays while the shared equality helper compares 21 records. The arrays and initialization loop now cover all 21 canary records. **Committed:** `b54ec48`.
3. **Rule 1 — Banked test register access.** Sanitizer evidence found r8 being indexed in the 8-element r0-r7 array. The fixture now writes and reads through the correct User/System bank. **Committed:** `ac9ebe6`.

**Impact:** These corrections affect test reliability only. No production behavior was regressed to manufacture RED, and the supported behavior now passes the real interpreter tests.

## Issues and limits

- The GBA-specific empty-list and base-alias rules are source-backed reference policies, not hardware measurements. The generic ARM ARM classifies the empty list as unpredictable; this project intentionally selects the documented GBA ARMv4 behavior.
- CPU-01 remains phase-wide pending. This plan does not claim broad hardware accuracy, hardware timing qualification, ROM/BIOS qualification, or diagnostic admission.
- Full ARM inventories, pinned Linux GCC/Clang sanitizer verification, and independent review remain with root after its scoped review.

## User Setup Required

None.

## Next Phase Readiness

Plan 13 is complete locally. Root owns independent review and final full ARM integration verification before continuing. Plan 14 has not started; CPU-01 and phase-wide diagnostic requirements remain pending.

## Self-Check: PASSED

- Summary file is present at the required phase path.
- All five task commits are ancestors of the plan head recorded above.
- `.planning/state.json` was restored byte-for-byte after each GSD handler; shared dirty project files remain outside the plan commits.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*
