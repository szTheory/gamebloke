---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "12"
subsystem: cpu
tags: [arm7tdmi, armv4t, block-transfer, preflight, ctest]

requires:
  - phase: 05-11
    provides: canonical bounded CPU transaction/result slots and dynamic PC refill
provides:
  - Normal ARM IA/IB/DA/DB STM/LDM forms with ordered transfers and writeback
  - Maximum-list PC refill through the actual final read result
  - Complete-unit timing, trace, counter, and buffer preflight for block transfers
affects: [05-13, CPU-01, armv4t]

actuals:
  tokens: 13037
  tasks: 2
  commits: 6
plan_head_before: 95ec9aa648660bc2d6e030e2aa9dbacfe23ee9d9
plan_head_after: d534ec0a41f34a4b13021d67c495a6cb126d9668

tech-stack:
  added: []
  patterns:
    - Per-slot bus request class with scalar nonsequential defaults
    - Separate post-data internal and abstract pipeline cycle accounting

key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "DDI0029G section 6.1 signals SEQ one cycle ahead; Tables 6-12/6-13 therefore map the first block data address to N and subsequent addresses to S."
  - "Use the DDI0029G LDM final I cycle and PC pipeline cost in the unit cost; the extra PC S cost is abstract uniform-bus work with no additional memory trace record."
  - "Keep user-bank/exception-return and ARM7-specific base-list quirks for their planned follow-up; preserve valid-unsupported classification."

patterns-established:
  - "Block transfers use one decoded ordered plan; preflight and execution share the same slots."
  - "Loaded PC refill addresses resolve from the actual final data result, with no preflight read or second interpreter."

requirements-completed: []
coverage:
  - id: D1
    description: "Normal nonempty STM/LDM transfers cover IA, IB, DA, and DB with ascending register/address order and captured writeback operands."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_block"
        status: pass
      - kind: unit
        ref: "build/asan/cpu_test arm_block"
        status: pass
    human_judgment: false
  - id: D2
    description: "A sixteen-register LDM to PC fits its exact modeled cycle and trace bounds; one-short/overflow refusal preserves state, memory, time, and caller buffers."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_block_bounds"
        status: pass
      - kind: unit
        ref: "build/asan/cpu_test arm_block_bounds"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
      - kind: integration
        ref: "ctest --preset asan && ctest --preset asan-post"
        status: pass
    human_judgment: false

duration: 57min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 12: ARM block transfers across all four address modes Summary

**Normal ARM7TDMI STM/LDM transfers now cover IA/IB/DA/DB, with bounded 16-slot execution and actual-read PC refill.**

## Performance

- **Duration:** 57 minutes
- **Started:** 2026-11-12T18:54:34Z (approximate)
- **Completed:** 2026-11-12T19:51:26Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Implemented normal nonempty ARM block loads and stores across IA, IB, DA, and DB. Addresses wrap modulo 32 bits, registers map in ascending order, store sources are captured before effects, and writeback uses the original base.
- Reserved 16 data slots, two refill slots, and up to 19 trace records in the same canonical decoded unit. LDM to PC derives both refill addresses from the actual final data read.
- Added exact-fit and one-short cycle, trace, sequence, and counter tests. Refusals preserve canonical state and identity, memory, open-bus latch, trace prefix, and caller-buffer canaries.
- Applied the ARM7TDMI LDM/STM timing table. A 16-register LDM to PC costs 20 modeled cycles; it still uses 19 trace records (one instruction, sixteen data, two refill reads).

## Verification

- Task 1 pre-implementation `arm_block` RED: all 18 semantic assertions failed against the prior decoder; the feature commit then passed the tracer selector.
- Task 2 capacity assertions were already green after Task 1. Kept that implementation and used isolated alignment, PC-result, and trace-capacity fault controls; each made its targeted assertions fail.
- After the timing review, commit `a384782` added failing expectations for DDI cycle cost and request classes. Commit `d534ec0` implemented the correction; post-commit selectors passed 18/18 and 10/10.
- `cmake --workflow --preset ci`: 343 expected, 342 ran, one allowed `signature.owner_local` skip because `GBL_OWNER_LOGO_FILE` was unset; `ci-post` passed with the same inventory.
- AppleClang ASan/UBSan: 86/86 expected tests ran with zero skips; `asan-post` confirmed the inventory.
- The N/S mapping follows DDI0029G section 6.1 and Tables 6-12/6-13: the row's `SEQ` describes the next address cycle. Trace classes therefore come from the preceding row's signal, not from the data value shown in the same row. The extra PC pipeline S cost is charged through the uniform one-cycle bus abstraction and has no separate trace record. This is not a literal hardware bus trace or hardware timing measurement.
- No hosted CI was run for this local Plan 12 source. The latest published green remains the earlier Plan 09 run; root owns final pinned Linux verification and independent review.

## Task Commits

1. **Task 1: add and implement normal ARM block transfers** — `d76851b`, `fbe7b98`, `67b12dd`
2. **Task 2: prove bounded PC refill and atomic refusal** — `53d7a01`
3. **Timing regression and correction** — `a384782` (test RED), `d534ec0` (fix)

## Files Created/Modified

- `src/cpu.c` — block transfer decoding, ordered slots, per-request N/S class, exact internal/pipeline cost, preflight, and execution.
- `tests/cpu_test.c` — normal mode matrix, PC store/read behavior, 16-register refill, exact capacity, cycle placement, and atomic refusal tests.
- `tests/core.cmake` — registered `arm_block` and `arm_block_bounds` selectors.
- `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — inventoried both CPU selectors in both lanes.

## Decisions Made

- The reference for address ordering and timing is the exact-core ARM7TDMI DDI0029G, cross-checked against ARM ARM DDI0100I where applicable. The GBA alignment policy remains a reference policy, not a hardware measurement.
- Normal LDM keeps the final I cycle. LDM with PC additionally reserves one abstract S pipeline cycle before its two result-dependent refill reads. All costs are included in unit preflight before effects.
- Valid user-bank/exception-return forms and ARM-specific base-list behavior remain separate follow-up work. The `LDM^` valid-unsupported decode control remains distinct from UND and unpredictable outcomes.

## Deviations from Plan

### In-scope timing correction

The first complete Plan 12 pass exposed that the generalized implementation was missing DDI0029G's final LDM I cost and PC pipeline cost, and that the N/S sequence needed to follow the one-cycle signal lead in section 6.1. Added explicit timing RED tests before the correction, then reran the full native and sanitizer inventories after the source commit. This was required by the plan's exact internal-cost and complete-unit preflight requirements.

### Task 2 TDD sequencing

The capacity tests were already green after Task 1's generalized bounded unit. Preserved that correct behavior instead of introducing a production regression to manufacture a RED. Isolated fault controls demonstrated that the new alignment, PC-result, and preflight assertions detect their targeted wrong behaviors. The later timing-specific RED/fix sequence is recorded above.

**Impact on plan:** No scope expansion. The additional fields remain private to the existing CPU unit, and all cycle/access/trace/counter limits are reserved before effects.

## Issues Encountered

- Initial timing-test expectations needed two evidence-based corrections before final green: sequence exhaustion starts at `UINT64_MAX - 18` for 19 required records, and a one-register LDM includes its final internal cycle. No production behavior was weakened to accommodate the tests.
- A first implementation attempt did not set the sequential unit kind and was rejected by the existing preflight. The decoder setup was corrected before the feature commit.

## User Setup Required

None.

## Next Phase Readiness

Plan 12 is complete locally. Plan 13 has not started. Root owns the independent Plan 12 review and pinned Linux GCC/Clang sanitizer verification before continuing the phase. CPU-01 remains phase-wide pending; no ROM, BIOS, hosted-run, or hardware-accuracy claim was added.

## Self-Check: PASSED

- Summary file exists at the required phase path.
- All six plan commits are ancestors of the current HEAD.
- `.planning/state.json` still matches its pre-handler snapshot byte-for-byte.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*
