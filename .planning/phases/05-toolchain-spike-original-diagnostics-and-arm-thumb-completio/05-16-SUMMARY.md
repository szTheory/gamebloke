---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "16"
subsystem: cpu
tags: [armv4t, thumb, gba, cpu]
requires:
  - phase: 05-15
    provides: Thumb transfer implementation and the canonical bounded CPU-unit path
provides:
  - Thumb PUSH and POP with bounded ordered memory slots and ARMv4 Thumb PC refill
  - Thumb STMIA and LDMIA with source-backed list, alias, and writeback behavior
affects: [cpu, thumb-integration]
actuals:
  tokens: 15137
  tasks: 2
  commits: 7
plan_head_before: 450b5a9386e83b1ce32f83e02181a91164dbed2c
plan_head_after: bbab30f90bdd001e3511f802031c276a3e573fc3
tech-stack:
  added: []
  patterns:
    - Canonical bounded transaction slots for Thumb multi-register operations
    - Result-driven POP-PC refill using the actual final bus read
key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - DDI0100I defines Thumb LDMIA base-in-list loads as taking precedence over writeback; Thumb STMIA always writes back, while a listed base that is not lowest has an UNPREDICTABLE stored value.
  - Gamebloke keeps its deterministic pre-effect stop policy for empty or architecturally UNPREDICTABLE forms; this is a project policy, not a hardware result.
  - POP-PC bit zero is ignored and execution remains Thumb under ARMv4T; the actual data-read result drives refill.
  - CPU-02 remains phase-wide pending until the remaining instruction families and integration gates are complete.
patterns-established:
  - Thumb format-specific list rules are kept separate from ARM block-transfer rules.
  - Whole-unit cycle, trace, sequence, and counter headroom is checked before any bus or register effect.
requirements-completed: []
coverage:
  - id: D1
    description: Thumb PUSH/POP and STMIA/LDMIA execute through the canonical bounded CPU unit.
    verification:
      - kind: unit
        ref: build/ci/cpu_test thumb_stack (15 assertions)
        status: pass
      - kind: unit
        ref: build/ci/cpu_test thumb_multiple (17 assertions)
        status: pass
      - kind: unit
        ref: AppleClang 21 ASan/UBSan thumb_stack and thumb_multiple
        status: pass
    human_judgment: false
duration: 48m40s
completed: 2026-11-13
status: complete
---

# Phase 05 Plan 16: Thumb Stack and Multiple Transfers Summary

**Thumb PUSH/POP and STMIA/LDMIA now use bounded ordered slots, architectural base rules, and preflighted bus work.**

## Performance

- **Duration:** 48m40s, measured from the dispatch receipt (including setup)
- **Started:** 2026-11-12T23:26:36Z
- **Completed:** 2026-11-13T00:15:16Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Implemented Thumb PUSH/POP for low registers plus LR/PC, including banked SP/LR access, ascending transfers, and ARMv4T POP-PC refill from the actual loaded value. The target keeps Thumb state and uses the architectural bit-zero/alignment rule.
- Implemented Thumb STMIA/LDMIA with captured store sources, ascending addresses, modulo-32-bit address/writeback arithmetic, LDM base-in-list writeback suppression, and STM base-first old-value storage plus writeback.
- Added exact and one-short checks for cycle budget, trace capacity, sequence numbers, master-cycle overflow, and retirement overflow. Refusals preserve canonical machine state, machine identity, and caller trace storage.
- Registered `thumb_multiple` in CTest and both CI/ASan inventories.

## Source Decisions

The rules were cross-checked against the cached ARM manuals: DDI0029G Table 1-7 identifies Thumb formats 14–15; DDI0100I A7.1.27 (A7-44/A7-45) defines LDMIA order and base-in-list suppression; A7.1.49/.50 (A7-82/A7-85) define POP/PUSH; A7.1.57 (A7-96/A7-97) defines STMIA order and its base-in-list restriction.

The ARM manual marks an empty list UNPREDICTABLE. For STMIA, it marks the stored base value UNPREDICTABLE when the base is listed after another register; it does not prescribe stopping the whole instruction. Gamebloke’s existing deterministic policy stops the whole unit before effects in these cases. Tests name that as project policy and do not treat it as measured GBA behavior. The public run result still maps these internal unpredictable stops to the existing unsupported-instruction reason; public classification remains for the later ISA audit.

Transfer timing and N/S access classes use the project’s abstract uniform-bus model. The trace records issued requests and observed bus results; they are not a literal physical bus trace or a hardware timing measurement.

## Verification

- RED: `build/ci/cpu_test thumb_multiple` exited 1 on the defined, not-yet-implemented format15 cases; the four empty/unpredictable controls passed. `gsd_run check tdd-red-evidence build/phase05-plan16/task2-red-evidence.json --raw` returned `RED_EVIDENCE_OK`.
- GREEN/postcommit tracer: `cmake --preset ci`, `cmake --build --preset ci`, `build/ci/cpu_test thumb_stack` (15/15), and `build/ci/cpu_test thumb_multiple` (17/17).
- Targeted AppleClang 21 ASan/UBSan build under `build/phase05-plan16/asan`; both `thumb_stack` and `thumb_multiple` passed.
- Targeted native CTest regressions for ARM block, loaded PC, and Thumb groups passed 10/10.
- The pinned GNU 15.2 build-only helper passed at the Task1 and final Task2 commits, compiling `src/cpu.c` and linking `cpu_test`.

Full native/compiler/sanitizer inventories and hosted CI were intentionally left to the parent’s later Thumb-family review and integration gate. No hardware, commercial ROM, or private BIOS data was used.

## Task Commits

1. **Task 1 RED:** `f8829dd`, `fa71515` — stack transfer and atomic-boundary tests.
2. **Task 1 GREEN:** `28319da` — bounded PUSH/POP and fixed-Thumb PC refill preflight.
3. **Task 2 RED:** `61e2084` — format15 semantic and preflight tests.
4. **Source-policy labels:** `ef1847b` — distinguish manual UNPREDICTABLE behavior from Gamebloke’s stop policy.
5. **Test oracle correction:** `d5de934` — use the already-queued pipeline’s next fetch address and the actual trace count.
6. **Task 2 GREEN:** `bbab30f` — bounded STMIA/LDMIA execution and base rules.

The seven measured plan commits include both Task 1 RED commits. The summary itself is committed separately.

## Deviations from Plan

- Task 1 exposed a preflight restriction that rejected valid fixed-Thumb POP-PC refill. The guard now allows Thumb-state result refill for a multiple-transfer unit while retaining scalar ARM restrictions. The 15-case tracer and sanitizer checks passed afterward.
- The first GREEN run showed two test-oracle assumptions were wrong: store trace values are bus-return values, so stored data is checked in memory; and the next fetch is `0x08000004` because the following Thumb instruction is already queued. Those expectations were corrected and committed before final verification.
- CMake initially rejected the new selector because inventory entries must be lexically ordered. Selector and inventory order were corrected before the RED run.

## Issues Encountered

None remain. The deterministic stop policy for unpredictable forms and the abstract timing model are documented above.

## User Setup Required

None.

## Next Phase Readiness

Plan05-16 is complete and ready for the parent’s independent source review and full Thumb-family integration checks. Plan17 and phase-wide CPU-02 completion remain with the parent.

## Self-Check: PASSED

- Summary path exists.
- All seven measured plan commits are ancestors of the dispatch-base range through `bbab30f`.
- No owner-managed config/state/handoff files were staged or changed by this plan.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-13*
