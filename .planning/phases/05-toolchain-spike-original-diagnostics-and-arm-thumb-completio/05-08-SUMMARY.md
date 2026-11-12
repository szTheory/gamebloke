---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "08"
subsystem: cpu
tags: [ARMv4T, ARM7TDMI, CPSR, SPSR, CMake, CTest]

requires:
  - phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
    provides: "Plan07 ARM ALU/shift behavior, canonical CPU preflight path, and inventoried native test groups"
provides:
  - "Operand-specific ARM/Thumb PC reads, ARM data-processing writes to PC, and captured-SPSR exception returns"
  - "MRS CPSR/SPSR and MSR register/immediate field, privilege, mode, and saved-bank handling"
  - "Inventoried arm_pc_return and psr CPU test selectors"
affects: ["Phase05 remaining CPU plans", "Plan18 unpredictable-result projection"]

actuals:
  tokens: 13629
  tasks: 2
  commits: 5
commits: 5
plan_head_before: 77bc4ae1a2f8f48a327110c3e78c8f77c5fa8117
plan_head_after: 673a7dc84cee4e98327e4678081eb6bafd0ff2fc

tech-stack:
  added: []
  patterns:
    - "Keep CPSR/SPSR changes as captured, bounded effects in the existing decode/preflight/execute unit."
    - "Pass explicit PC-read context where ARM/Thumb operands have different architectural values."

key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Apply the ARM ARM DDI0100I A4.1.39 MSR operation and ARMv4T Table A4-1 masks, including the UnallocMask check before selected field masking."
  - "Keep unpredictable instructions internal to the CPU unit; projecting a distinct public reason remains Plan18 work."
patterns-established:
  - "A CPSR write captures its source and bank before effects; an SPSR write carries its original SPSR index in the ephemeral status effect."
  - "ARM/Thumb PC values are selected by operand use, and a return refill uses the captured restored state."

requirements-completed: []
coverage:
  - id: D1
    description: "ARM/Thumb PC operand contexts and ARM PC/SPSR return refill behavior."
    verification:
      - kind: unit
        ref: "build/phase05-plan08/tracer-verify.log: cpu_test arm_pc_return (10/10)"
        status: pass
    human_judgment: false
  - id: D2
    description: "MRS/MSR field forms, mode/bank visibility, privilege rules, and deterministic refusal postconditions."
    verification:
      - kind: unit
        ref: "build/phase05-plan08/psr-green.log: cpu_test psr (13/13)"
        status: pass
      - kind: integration
        ref: "build/phase05-plan08/ci-workflow.log: canonical CI workflow and inventory post-check"
        status: pass
      - kind: integration
        ref: "build/phase05-plan08/asan-post.log: ASan/UBSan inventory post-check (78/78, zero skips)"
        status: pass
    human_judgment: false

duration: 49min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 08: PC, SPSR Return, and PSR Transfers Summary

**ARM7TDMI PC writes and MRS/MSR now preserve ARMv4T mode, bank, and pipeline semantics.**

## Performance

- **Duration:** 49m 15s
- **Started:** 2026-11-12T04:21:14Z
- **Completed:** 2026-11-12T05:10:29Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Added explicit operand-use PC reads: ARM normal reads use current+8, register-shift Operand2 and sourced STR PC use current+12, Thumb reads use current+4, and Thumb literal address formation aligns the address.
- Enabled ARM data-processing writes to PC, including S+PC returns that capture the active mode's SPSR before effects and refill using the restored ARM or Thumb state. Condition-failed returns retire sequentially without restoring status.
- Added MRS CPSR/SPSR and MSR register/rotated-immediate c/x/s/f handling through the existing bounded CPU unit, with mode/bank checks and pre-effect unpredictable stops.
- Registered `cpu.arm_pc_return` and `cpu.psr` in CMake and both CI/ASan inventories.

## Task Commits

1. **Task 1: PC-sensitive data processing and SPSR return** — `ef3e290` (`feat`)
2. **Task 2 RED: PSR behavior tests** — `3a9c15b` (`test`)
3. **Task 2 test correction: ARMv4T reserved-mask semantics** — `dc2c417` (`test`)
4. **Task 2 GREEN: PSR transfers** — `f862a70` (`feat`)
5. **Post-review PSR edge coverage** — `673a7dc` (`test`; implementation unchanged)

## Files Created/Modified

- `src/cpu.c` — operand-specific PC contexts, PC-write refill, MRS/MSR decode, and bounded status effects.
- `tests/cpu_test.c` — real guest instruction streams, independent state expectations, mode/bank coverage, and refusal snapshots.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — selector registration and inventory entries.

## Decisions Made

- The MSR behavior follows the complete ARM ARM DDI0100I A4.1.39 operation and Table A4-1 for ARMv4T. Its `UnallocMask` rejection occurs before field masking; privileged CPSR writes with the StateMask bit set are unpredictable even if the c lane is unselected. User mode writes only the flags mask, while SPSR c writes may set T. See the [ARM Architecture Reference Manual, DDI0100I, A4.1.38–A4.1.39](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).
- The internal unpredictable class remains private. Existing public results still report the existing unsupported-instruction reason until the coherent Plan18 projection.
- `requirements-completed` remains empty: CPU-01 is phase-wide and is still pending.

## Verification

- Task 1 RED: `build/phase05-plan08/task1-red.log`; MOVS/SUBS PC returns failed before implementation. The initial one-short assertion also failed because the test's trace count/capacity initializer was reversed, so that initial failure is not treated as preflight evidence. The corrected full-state, trace-buffer, and raw-record-byte assertion passes in `tracer-verify.log` (10/10).
- Task 2 RED: `build/phase05-plan08/psr-red-evidence.json` was accepted by `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK`; the target MRS assertion failed before implementation. The final selector passes 13/13.
- `cmake --workflow --preset ci`: passed; 334 tests ran and the existing `signature.owner_local` input-dependent check was the single configured skip. The inventory post-check accepted exactly that skip.
- Native AppleClang ASan/UBSan: `cmake --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang`, build, `ctest --preset asan`, then `ctest --preset asan-post` all passed. Inventory: 78/78 ran, zero skips.
- Isolated Linux GCC: `sh build/phase05-plan08/linux-gcc-verification.sh` passed the same 334 tests plus the same one configured owner-input skip on the exact committed source.
- Focused CPU regression groups are recorded under `build/phase05-plan08/*-verify.log`; all 16 selectors passed, including ALU, conditions, PC return, PSR, slots, tracing, modes, exceptions, and refusal controls.

## Deviations from Plan

### TDD commit sequencing

Task 1's RED was run before implementation, but its tracer tests and implementation were committed together in `ef3e290` instead of separate RED/GREEN commits. Task 2 used separate RED and GREEN commits. The task behavior and verification are complete; this is a commit-sequencing deviation.

The PSR operand vectors were corrected to match the exact ARM operation semantics before GREEN; the reserved-bit negative control remains explicit.

## Issues Encountered

- CMake initially rejected the new selector because the CI inventory entry was not in lexical order. The entry was moved to its sorted position; configure/build then passed.
- An early rotated-immediate x-lane vector contained bit 5, which the privileged CPSR operation classifies as an unpredictable state attempt. It was changed to a legal operand, while explicit active CPSR T-write refusal and User-mode ignored-control cases remain covered.

## User Setup Required

None.

## Next Phase Readiness

Plan 08 is complete locally. Plan 09 remains not started. Phase-wide CPU-01 and the public unpredictable-result projection remain pending; this summary makes no broad ISA, hardware-accuracy, BIOS, or game-compatibility claim.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists at the required phase path.
- All four initial task commits and the post-review test commit are ancestors of the recorded plan head.
- No task-created stubs or generated untracked files were found in the owned source/test files.

## Review-fix Addendum — 2026-11-12

The four PSR acceptance-test gaps from `05-08-REVIEW.md` are covered by test-only commit `673a7dc` (`tests/cpu_test.c`). The vectors now distinguish reserved-bit rejection before field masking, privileged CPSR StateMask rejection with c unselected, SPSR c/f status effects in all five exception banks, and zero/composed masks across CPSR/SPSR register and rotated-immediate forms. The refusal vectors check canonical state and ownership, trace metadata, and the complete initialized trace-record buffer. `src/cpu.c` was inspected and left unchanged because it already implements the reviewed ARM ARM DDI0100I A4.1.39/Table A4-1 ordering and separate saved-status effect path.

Verification added after the review:

- Main checkout normal build and native AppleClang ASan/UBSan each passed all 29 registered CPU CTest cases. After a small test-only fixture cleanup, fresh focused `cpu.psr` (41 assertions) and `cpu.arm_pc_return` (10 assertions) runs passed in both configurations.
- An isolated Ubuntu 26.04/GCC clone of commit `673a7dc` built `cpu_test` and passed all 29 registered CPU CTest cases.
- Exact logs are retained under ignored `build/phase05-review08/`. The full project CI and ASan inventories were not rerun for these test-only changes; earlier full-suite evidence above remains historical. No hardware-accuracy claim follows from these software vectors.
- Phase state was not advanced. Plan 08 remains locally complete pending review closure; Plan 09 remains not started, and phase-wide CPU/DIAG requirements remain pending.


## Hosted integration stop after publication

Published head b0fe8812de9151b1e42f4bac1d6193e8f1ee9564 failed run https://github.com/szTheory/gamebloke/actions/runs/43167646132 because Linux LeakSanitizer detected a 192-byte test-fixture ROM leak in cpu.psr. The bank-visibility block in test_psr_mode_reads_and_banks omits gbl_machine_dispose; other substantive lanes passed. The prior local/source-review evidence above remains true for its actual scope, but the hosted gate is unresolved. See 05-08-CI-HANDOFF.md. No fix/retry followed before owner approval. The owner subsequently approved the bounded correction; progress is recorded below. Plan 09 has not started.


## Owner-approved fixture lifetime correction

Owner replied "Yes." to cleanup, a portable allocation-balance regression, matching Linux sanitizers and one corrected hosted run. Tests-only commit `819aac865e472a8d12d55716c2fef864f249db2b` unconditionally disposes the PSR bank-visibility machine. Its allocator-seam check demonstrates a live owned ROM, unchanged counts on refused allocation and zero live allocations/bytes after disposal. The saved pre-disposal negative control failed at that assertion, proving it detects the original defect. Native normal and AppleClang ASan/UBSan passed all 29 CPU groups; scoped independent source re-review is clean.

The pinned Ubuntu 26.04 native arm64/Clang 21 full ASan/UBSan/LeakSanitizer run at that exact commit passed focused cpu.psr, all 78 required sanitizer checks and inventory postcheck with zero skips. Both intentional sanitizer failure controls passed. Evidence: `build/phase05-plan08/leak-fix/`. No source/CPU implementation, frozen producer input, ROM pin, required check, skip policy, sanitizer setting or hosted time limit changed. Hosted x64 confirmation remains pending on the one approved corrected run; no phase-wide completion is claimed.
