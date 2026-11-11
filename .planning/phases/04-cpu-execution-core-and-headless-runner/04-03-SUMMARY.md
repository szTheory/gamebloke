---
phase: 04-cpu-execution-core-and-headless-runner
plan: "03"
subsystem: cpu
tags: [C17, ARM7TDMI, Thumb, CMake, ASan, UBSan]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: Bus-backed machine, CPU lifecycle/state, and bounded single-unit interpreter from plans 04-01 and 04-02.
provides:
  - Unsigned guarded arithmetic and shift helpers with ARM flag results.
  - A defined ARM arithmetic, Thumb, branch/BX, and aligned transfer execution slice.
  - CI and ASan CPU selectors with pipeline and RRX semantic negative controls.
affects: [CPU execution, headless runner integration, Phase 5 full ISA]

# Actuals measured as chars/4 over the realized implementation diff.
actuals:
  tokens: 23274
  tasks: 3
  commits: 5
commits: 5
plan_head_before: a0b48a9e0d7f3b855f0610aa6baae024324905e4
plan_head_after: 236b8759f84d5376bdea43e7174067b046277914

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Unsigned, guarded shift and arithmetic helpers return value, carry, and overflow explicitly.
    - ARM and Thumb instructions use one decoded-unit preflight and the canonical bus for fetches and transfers.

key-files:
  created: [src/cpu_helpers.h]
  modified:
    - src/cpu.c
    - include/gamebloke/gamebloke.h
    - tests/cpu_test.c
    - tests/gates/core_mutation.cmake
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Expose distinct public stop/run reasons for reserved undefined encodings and valid but unsupported instructions, without adding persistent machine state."
  - "Use ARMv4T-scoped semantics and keep other instruction families explicitly unsupported for Phase 5."
patterns-established:
  - "Check shift counts before C shifts; implement ASR and RRX using unsigned operations."
  - "Preflight complete-unit cost and fetch/data spans before committing instruction effects."

# The plan contributes CPU behavior evidence, but the parent owns cross-plan
# requirement roll-up; this summary does not mark broad requirements complete.
requirements-completed: []
coverage:
  - id: D1
    description: "ARM arithmetic and defined shift behavior, including flag and boundary cases."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arithmetic; cpu.nc.shift_carry"
        status: pass
    human_judgment: false
  - id: D2
    description: "The supported Thumb, branch/BX, and word/byte transfer slice uses bus-backed fetch/refill and data requests."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test thumb; build/ci/cpu_test decode; build/ci/cpu_test alignment; cpu.nc.pc_refill"
        status: pass
    human_judgment: false
  - id: D3
    description: "CPU semantic selectors and their clean/mutated controls are registered in CI and ASan lanes."
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
      - kind: integration
        ref: "build/asan focused cpu.* CTest selectors"
        status: pass
    human_judgment: false

duration: not-recorded
completed: 2026-11-10
status: complete
---

# Phase 4 Plan 3: ARM Arithmetic and Thumb Execution Summary

**Defined ARM arithmetic and shifts, Thumb/BX control flow, and bus-backed word and byte transfers with focused semantic controls.**

## Performance

- **Duration:** Not recorded at plan start; no total plan duration is claimed.
- **Completed:** 2026-11-10
- **Tasks:** 3
- **Files modified:** 8 implementation/test/configuration files, plus this summary.
- **CI test critical path:** 95.02 seconds for 305 passing tests and one allowed owner-local skip; the inventory post-check passed.

## Accomplishments

- Added `gbl_shift`, `gbl_add_flags`, and `gbl_sub_flags` with guarded unsigned operations for counts 0, 1, 31, 32, 33, and 255; RRX; ARM carry/overflow; and signed boundaries.
- Implemented the planned ARM data-processing subset and Thumb immediate/high-register operations, ARM B/BL/BX, Thumb conditional/unconditional B and BX, and bus-backed ARM/Thumb word and byte transfers. PC reads follow the slice's ARM +8 and Thumb +4 rules; transfer alignment and word-load rotation are tested.
- Added distinct public unsupported-instruction and undefined-instruction reasons without adding CPU state, and registered four CPU selectors plus branch-refill and RRX mutation controls in both CI and ASan inventories.

## Verification

- TDD RED evidence was classified `RED_EVIDENCE_OK` for task 1's arithmetic witnesses (13 failing assertions against the unsupported baseline) and task 2's Thumb/BX witnesses (8 failing assertions against the unsupported baseline).
- `cmake --build --preset ci && build/ci/cpu_test arithmetic` — 28/28 passed.
- `build/ci/cpu_test thumb` — 8/8 passed; `build/ci/cpu_test decode` — 7/7 passed; `build/ci/cpu_test alignment` — 8/8 passed.
- Existing `build/ci/cpu_test budget` — 9/9 passed; tracer and lifecycle regressions also passed during task integration.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(arithmetic|thumb|decode|alignment|nc\.(pc_refill|shift_carry)|pos\.clean)$' --no-tests=error --output-on-failure` — 7/7 passed, including both negative controls and the clean companion.
- `cmake --workflow --preset ci` — all 306 inventoried checks completed: 305 passed, one optional owner-local signature check skipped because `GBL_OWNER_LOGO_FILE` was unset; the CI inventory post-check passed. The measured test critical path was 95.02 seconds.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan` followed by the same seven focused CPU CTest selectors — 7/7 passed under macOS ASan/UBSan.
- The branch mutation failed the named ARM BL visible-PC/refill assertion. The RRX result mutation failed the named helper assertion. Both clean companions passed; the mutation driver treats compilation failure as an error, not negative evidence.

## Task Commits

Each task's RED and GREEN or implementation commit is atomic:

1. **Task 1 RED:** `555a62a` — add failing arithmetic execution witnesses.
2. **Task 1 GREEN:** `2ddef0e` — execute ARM arithmetic and defined shifts.
3. **Task 2 RED:** `75f9264` — add failing Thumb and transfer witnesses.
4. **Task 2 GREEN:** `5a36836` — execute Thumb branches and bus transfers.
5. **Task 3:** `236b875` — gate CPU semantics with mutation controls.

## Files Created/Modified

- `src/cpu_helpers.h` — private arithmetic and shift helpers.
- `src/cpu.c` — supported arithmetic, control-flow, transfer decoding, and single-unit cost/preflight behavior.
- `include/gamebloke/gamebloke.h` — public reason values distinguishing undefined from valid unsupported instructions.
- `tests/cpu_test.c` — arithmetic, Thumb, decode, alignment, and budget witnesses.
- `tests/gates/core_mutation.cmake` — scratch CPU builds, branch-target and RRX mutations, and clean companions; helper header copied to bus, loader, and CPU scratch builds.
- `tests/core.cmake`, `tests/inventory/ci.txt`, and `tests/inventory/asan.txt` — CPU selectors and controls registered in both lanes.

## Decisions and Scope

Supported execution is limited to the ARMv4T instruction families named in this plan. Reserved encodings stop as undefined; valid but excluded families and unsupported operands stop as unsupported. The core keeps one decode/preflight/bus path, with no added timing qualification or hardware-accuracy claim. Full ISA coverage remains Phase 5 work. No ROM, BIOS, or private-content execution was used.

## Deviations from Plan

### Authorized correctness addition

The new undefined-instruction reason required a distinct public run reason as well, so callers can tell valid unsupported instructions from reserved encodings. The extra public-header edit was authorized by the parent; no persistent field or hardware claim was added.

**Files:** `include/gamebloke/gamebloke.h` and the already-planned `src/cpu.c`.

### Verification control refinement

The first RRX carry mutant was ineffective for the selected input because bits 0 and 1 were both set. It was replaced with a mutation to the RRX result shift, which the named helper assertion detects. The resulting control and its clean companion both pass.

**File:** `tests/gates/core_mutation.cmake`.

## Issues Encountered

The initial mutation-control run exposed the ineffective RRX input/mutant pairing; the control was corrected and then passed in both CI and ASan/UBSan. No implementation issues remain from this plan.

## Known Limits

This is a deliberately bounded ARM/Thumb execution slice. Block transfers, multiply, halfword/signed transfers, writeback/post-index modes, PSR transfers, Thumb BL pairs, and other unimplemented instruction families remain unsupported. Tests establish deterministic software behavior for these vectors, not physical hardware qualification or broad game compatibility.

## Known Stubs

- `include/gamebloke/gamebloke.h:232` — The pre-existing bus API documents a placeholder master-cycle cost. Plan 04-03 uses that project policy for deterministic budgeting and does not establish hardware timing accuracy. This entry is recorded in `.planning/WINDOWS.md`.

## Next Phase Readiness

The arithmetic, control-flow, transfer, and semantic-control seams are ready for later CPU expansion. The parent retains ownership of phase state and requirement roll-up; no phase-level completion is asserted here.

---
*Plan: 04-03*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary file exists and names all eight implementation/test/configuration files.
- All five atomic task commits are ancestors of the current plan head.
- CI and macOS ASan/UBSan focused controls passed; the canonical CI inventory post-check passed.
