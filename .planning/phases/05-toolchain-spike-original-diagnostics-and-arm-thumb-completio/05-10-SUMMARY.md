---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "10"
subsystem: cpu
tags: [armv4t, arm-transfer, ldr-pc, preflight, ctest, asan]

requires:
  - phase: 05-09
    provides: Canonical CPU unit dispatch and validated native CI path
provides:
  - ARMv4T ARM byte/word single-transfer addressing and writeback semantics
  - Dynamic LDR PC refills resolved from the actual word-read result
  - The loaded_pc selector registered in CI and sanitizer inventories
affects:
  - Later Phase 05 CPU completion plans
  - Phase 06 cross-compiler and CPU fixture verification

actuals:
  tokens: 15838.75
  tasks: 2
  commits: 7
commits: 7
plan_head_before: 32bdd5be3b4c3124bdc4c7329a807f697c4c7b2c
plan_head_after: 99d036abdb0d10fd520eb55bd76311c69a18ef69

tech-stack:
  added: []
  patterns:
    - Bounded symbolic fetch slots reference an earlier actual bus-read result and resolve only during execution
key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - "ARMv4T LDR PC masks the loaded target to a word boundary and continues in ARM state; the second refill address wraps modulo 2^32."
  - "The entire dynamic data/refill, trace, cycle, and counter cost is reserved before the first read; no preflight read predicts the target."
  - "Misaligned LDR PC source addresses and LDRT with Rd=R15 are rejected as architecturally unpredictable before effects."
patterns-established:
  - "Dynamic refill: use a bounded result index, alignment mask, and aligned modulo-32 addends in the canonical CPU unit."
requirements-completed: []

coverage:
  - id: D1
    description: "ARMv4T byte and word transfers execute legal immediate and scaled-register forms with snapshots, writeback, bus alignment behavior, and refusals."
    verification:
      - kind: unit
        ref: "cmake --workflow --preset ci; cpu.arm_transfer"
        status: pass
    human_judgment: false
  - id: D2
    description: "ARMv4T LDR PC consumes the actual prior word-read result, reserves both ARM refill reads, and preserves atomic preflight refusals."
    verification:
      - kind: unit
        ref: "cpu.loaded_pc"
        status: pass
      - kind: unit
        ref: "AppleClang ASan/UBSan: ctest --preset asan"
        status: pass
    human_judgment: false

duration: 43min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 10: ARM Transfers and Dynamic PC Loads Summary

**ARMv4T byte/word transfers now use preflighted effects, and LDR PC refills from its actual loaded word result.**

## Performance

- **Duration:** 43 minutes from the first recorded task commit; setup began earlier and was not timestamped.
- **Started:** 2026-11-12T10:48:29-05:00 (first recorded task commit)
- **Completed:** 2026-11-12T11:31:32-05:00
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Implemented legal ARM immediate and scaled-register word/byte transfers, including pre/post-indexing, writeback, PC operand contexts, word rotations, aligned-down stores, and flat-bus force-user T forms.
- Added result-indexed ARM refill slots for LDR PC. Both refill addresses derive from the actual completed word read, the first masks loaded bits [1:0], and the second address wraps modulo 2^32. ARMv4T state is retained.
- Reserved the complete 3-cycle data-plus-refill unit, trace records, trace sequence space, master-cycle headroom, and retired-instruction count before reading. No preflight memory read or second execution path was introduced.
- Added the `loaded_pc` selector to the CI and sanitizer test inventories.

## Task Commits

Task 1: Execute legal pre/post-indexed immediate and shifted-register transfers:

1. `43e77aa` — failing transfer writeback test
2. `0a38e60` — immediate and scaled-register form matrix
3. `be3a383` — alias and modulo-wrap tests
4. `81e1493` — transfer implementation

Task 2: Retire a memory-loaded PC using symbolic already-reserved refill slots:

1. `0b5c76e` — failing loaded-PC refill tests
2. `c4afbd7` — failing architectural guard tests
3. `71c2827` — loaded-PC implementation and guards

The plan ledger records seven task commits from `32bdd5b` through `71c2827`.

## Files Modified

- `src/cpu.c` — transfer decoding, symbolic result-derived refill scheduling, preflight validation, and execution-time address resolution.
- `tests/cpu_test.c` — 355 transfer assertions and 17 loaded-PC assertions, including full-state, ownership, trace-buffer, bus-outcome, wrap, and counter-boundary checks.
- `tests/core.cmake` — registered the new selector.
- `tests/inventory/ci.txt` and `tests/inventory/asan.txt` — included the new selector in both required inventories.

## Verification Evidence

- Task 1 RED: `task1-red-evidence.json` classified `RED_EVIDENCE_OK`; the named writeback assertion failed before implementation. After the implementation commit, the tracer rerun passed all 355 `arm_transfer` assertions.
- Task 2 RED: `task2-red-evidence.json` classified `RED_EVIDENCE_OK`; all 12 initial loaded-PC assertions failed because dynamic LDR PC was unsupported.
- Architectural guard RED: `task2-red-architecture-evidence.json` classified `RED_EVIDENCE_OK`; 15 of 17 assertions passed, while unaligned LDR PC source and LDRT-to-PC refusal assertions failed before the guards were added.
- Focused GREEN: `build/ci/cpu_test arm_transfer` passed 355/355 and `build/ci/cpu_test loaded_pc` passed 17/17.
- Native canonical workflow: `cmake --workflow --preset ci` passed. The CTest inventory reconciled 339 entries: 338 passed and the owner-local logo fixture check was allowed to skip because `GBL_OWNER_LOGO_FILE` was unset. The post-inventory check passed.
- AppleClang sanitizer lane: `/usr/bin/clang` ASan/UBSan build and `ctest --preset asan` passed 82/82 with no skips. `ctest --preset asan-post` confirmed all 82 expected tests ran.
- The native CI test workflow took 200.90 seconds; its configured run performed one configure/build sequence. No CI tooling or timeout changes were needed.

## Decisions Made

- The dynamic target mask applies to the loaded PC value, which ARMv4T treats like a MOV to PC. The memory source itself must be word-aligned.
- Post-indexed LDRT to R15 is rejected as unpredictable per ARM DDI0100I A4.1.31. The ordinary LDR PC path keeps ARM state per A4.1.23.
- Symbolic fetch plans remain bounded to one prior word-read result, the exact ARM alignment mask, and two aligned addends; execution resolves them only after the read.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Test oracle] Corrected the transfer wrap test encoding**
- **Found during:** Task 1
- **Issue:** The wrap-case opcode set the register-offset bit, so it did not exercise the intended immediate-offset form.
- **Fix:** Corrected the opcode to the immediate-offset encoding and retained the wrap assertion.
- **Files modified:** `tests/cpu_test.c`
- **Verification:** The corrected transfer selector passed all 355 assertions.
- **Committed in:** `81e1493`

**2. [Rule 2 - Missing architectural guard] Refused unpredictable loaded-PC cases before effects**
- **Found during:** Task 2 manual-reference review
- **Issue:** The transfer decoder accepted unaligned LDR PC source addresses and post-indexed LDRT with R15, although the ARM architecture reference marks these cases unpredictable.
- **Fix:** Added decode-time refusals before any bus read or register/trace mutation.
- **Files modified:** `src/cpu.c`, `tests/cpu_test.c`
- **Verification:** Both red guard assertions became green and verified atomic state and trace preservation.
- **Committed in:** `71c2827`

**Total deviations:** 2 auto-fixed; both were correctness or test-oracle fixes within this plan's scope. No scope expansion.

## Issues Encountered

- The owner-local logo fixture check did not run because its opt-in path was unset. The CI inventory marked this skip as allowed; no private ROM or BIOS content was accessed.
- No sanitizer errors, build failures, or unresolved implementation issues were found.

## User Setup Required

None.

## Next Phase Readiness

Plan 10 is complete. Phase-wide requirement CPU-01 remains pending; this plan covers only ARM single transfers and LDR PC behavior. Root owns Linux cross-compiler verification, code review, hosted monitoring, and the Phase 05 close-out decision. No Plan 11 work was started.

## Self-Check: PASSED

- Summary file exists and the seven measured task commits are ancestors of the recorded plan head.
- Task commit count and plan head were read from the persisted plan ledger.
- The dirty `.planning/state.json` snapshot still matches byte-for-byte before state handlers.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*



## Independent review corrections and final Linux verification

The first scoped review found two architectural store-refusal defects and one wrapped-pipeline consumer inconsistency. Those findings were corrected in separate source/test commits:8213adb (STR/STRB writeback alias refusal and legal non-writeback companions),4c5ed7c (STRB-PC refusal),99d036a (exact generated ARMFFFFFFFC/0 pair validation and explicit next-run unpredictable stop). Meaningful pre-fix controls failed their named assertions; the reviewer inspected the logs and re-reviewed code/tests.05-10-REVIEW.md is now clean with resolved history;05-10-REVIEW-FIX.md retains all evidence and source rules. The seven original task commits/ledger remain the original implementation scope; these three fixes are additional review commits. Final source is99d036abdb0d10fd520eb55bd76311c69a18ef69.

At that exact source, nativefullCI passed338 required checks with one existingallowed owner-local logo skip and inventorypost; nativeAppleClangASan/UBSan passed82/82zero skips/post. Root sequentially ran full pinnedUbuntu26.04nativearm64GCC and Clang21ASan/UBSan/LeakSanitizer: GCC338pass+oneallowedskip/post, Clang82/82zero skips/post. Logs/receipts are under build/phase05-plan10/review-fix/final-linux-*. Ordinary static B/BX guards and word STR-PC+12 behavior remain; new private unpredictable stops use the existing unsupported public projection until Plan18. No hardware or general compatibility result is inferred.

Wave hooks found no schema drift or UI change requiring a contract; codebase drift has the existing advisory no-STRUCTURE.md skip. Phase-wide CPU/DIAG requirements remain pending. Root batches routine implementation-plan publication until explicit hosted-witness/final phase gates under the existing one-PR-per-phase policy; this plan remains local and does not claim hosted coverage of its source. Latest published green source is a16dad4/run43249533205. This reduces repeated full hosted setup/build/test work while preserving per-plan local checks/review, mandatory phase CI/acceptance/merge gates and stop-on-new-red policy. Plan11 is next, no owner input needed.
