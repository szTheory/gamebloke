---
phase: 04-cpu-execution-core-and-headless-runner
plan: "04"
subsystem: cpu/testing
tags: [C17, ARM7TDMI, banked-registers, exceptions, CMake, ASan, UBSan]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: Bus-backed machine, CPU state lifecycle, and bounded interpreter from plans 04-01 through 04-03.
provides:
  - Validated CPSR mode changes and banked ARM7TDMI register access.
  - Centralized exception entry with explicit current/next/fault address context and named BIOS-none stops.
  - CI and ASan mode, exception, no-BIOS, and semantic mutation witnesses.
affects: [Phase 4 CPU expansion, Phase 5 full ISA, headless runner integration]

# Actuals (#2632), measured over the eight implementation/test files (diff chars / 4).
actuals:
  tokens: 16074
  tasks: 3
  commits: 5
commits: 5
plan_head_before: 79c19d6ec394fbaf7c79bfb3ac284ffbd5fa55db
plan_head_after: 5af568c4ebc0b946417ecc971950cf4a155ee585

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Physical register banks are selected through one mode-aware read/write path.
    - Exception address context is explicit; the entry table assigns LR bases, SPSR, vector, and stop reason.
    - Exception state is included in lifecycle reset, field equality, and fixed-order digest checks.

key-files:
  created: []
  modified:
    - include/gamebloke/gamebloke.h
    - src/cpu.c
    - tests/cpu_test.c
    - tests/state_check.h
    - tests/gates/core_mutation.cmake
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Use named current, next, and faulting addresses for exception return-link calculations instead of a generic PC offset."
  - "Treat BIOS-less exception entry as a terminal guest stop before handler fetch; do not add BIOS emulation or an interrupt controller."
  - "Masked IRQ/FIQ injection returns an explicit status without state or cycle changes."

patterns-established:
  - "All seven processor modes are tested by executing register reads against distinct bank sentinels; User/System share the same bank."
  - "Negative controls compile disposable copies of canonical CPU code and must fail named semantic assertions; clean companions must pass."

# Parent owns cross-plan requirement roll-up; this slice does not declare full requirements complete.
requirements-completed: []
coverage:
  - id: D1
    description: "All processor modes select the expected shared/private banks; User/System SPSR access is explicitly absent."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test modes; cpu.modes; cpu.nc.bank_alias"
        status: pass
    human_judgment: false
  - id: D2
    description: "Exception entry saves CPSR and explicit address context, selects the expected LR/vector/masks, and stops before BIOS-less handler fetch."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test exceptions; build/ci/cpu_test no_bios"
        status: pass
      - kind: integration
        ref: "cpu.exceptions; cpu.no_bios; cpu.nc.exception_lr; cpu.nc.vector_fetch"
        status: pass
    human_judgment: false
  - id: D3
    description: "Mode, exception, and no-BIOS witnesses and their mutation controls run in CI and ASan lanes."
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
      - kind: integration
        ref: "cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang; focused CPU CTest selectors"
        status: pass
    human_judgment: false

duration: 33min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 4: Banked Modes and BIOS-None Exception Entry Summary

**ARM/Thumb exception entry now preserves banked mode state, uses explicit LR address bases, and stops before an absent BIOS vector can execute.**

## Performance

- **Duration:** 33m 27s.
- **Started:** 2026-11-11T04:58:21Z
- **Completed:** 2026-11-11T05:31:48Z
- **Tasks:** 3
- **Implementation/test files modified:** 8.
- **Canonical CI critical path:** 74.09 seconds for 312 inventoried tests; 311 ran and passed, and one allowed owner-local check was skipped. The post-run inventory check passed.

## Accomplishments

- Added validated `gbl_cpu_set_cpsr`, `gbl_cpu_get_spsr`, and `gbl_cpu_enter_exception` interfaces. User/System share r8-r14; FIQ has private r8-r14; IRQ/SVC/ABT/UND have private r13-r14; and five exception modes have private SPSRs. Invalid modes, absent SPSRs, invalid contexts, and null inputs refuse without state changes.
- Centralized reset, undefined, SWI, prefetch abort, data abort, IRQ, FIQ, and coprocessor entry. Entry snapshots the old CPSR and explicit current/next/fault addresses, selects literal LR bases, sets masks/state/vector, clears stale pipeline slots, charges one project internal cycle, and stops before fetching a handler.
- Added ARM/Thumb SWI and undefined plus ARM coprocessor guest witnesses, direct injected evidence for reset/abort/IRQ/FIQ, masked-request postconditions, first-entry and same-mode overwrite checks, and `last_exception` lifecycle/equality/digest coverage.
- Registered `cpu.modes`, `cpu.exceptions`, and `cpu.no_bios`, with independent private-bank, IRQ LR, and vector-continuation mutants plus clean companions in CI and ASan inventories.

## Verification

- Task 1 RED evidence was checked as `RED_EVIDENCE_OK`; its test-only commit demonstrated that real ARM SWI entry was missing. After implementation, `cmake --build --preset ci && build/ci/cpu_test exceptions` passed, and the tracer feedback rerun passed.
- Task 2 RED evidence for masked IRQ/FIQ requests was checked as `RED_EVIDENCE_OK`. `cmake --build --preset ci`, `build/ci/cpu_test modes`, `build/ci/cpu_test exceptions`, and `build/ci/cpu_test no_bios` all passed after the implementation.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(modes|exceptions|no_bios|nc\.(bank_alias|exception_lr|vector_fetch))$' --no-tests=error --output-on-failure` — 6/6 passed. Each mutant failed its named semantic assertion and each clean companion passed.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --test-dir build/asan -R '^cpu\.(modes|exceptions|no_bios|nc\.(bank_alias|exception_lr|vector_fetch))$' --no-tests=error --output-on-failure` — fresh macOS AppleClang ASan/UBSan build succeeded; 6/6 focused tests passed in 6.12 seconds.
- `cmake --workflow --preset ci` — all 312 inventoried tests passed as 311 run passes plus one allowed skip (`signature.owner_local`, because `GBL_OWNER_LOGO_FILE` was unset); `inventory.junit` passed with the same count. Configure completed, the build was already current, and the test critical path was 74.09 seconds.

## Task Commits

1. **Task 1 RED:** `5323842` — add failing ARM SWI exception witness.
2. **Task 1 GREEN:** `0bd35f3` — enter SWI exceptions before absent BIOS fetch.
3. **Task 2 RED:** `831f12e` — reject masked IRQ and FIQ exception requests.
4. **Task 2 GREEN:** `7239084` — honor exception masks and verify mode banks.
5. **Task 3:** `5af568c` — gate CPU modes, exceptions, and vector stops.

## Files Created/Modified

- `include/gamebloke/gamebloke.h` — exception context and kind types, status/stop reasons, and internal CPU entry interfaces.
- `src/cpu.c` — mode validation, bank access, precise exception entry, masks, and BIOS-none stop behavior.
- `tests/cpu_test.c` — mode, SPSR, exception, refusal, lifecycle, and no-BIOS behavior witnesses.
- `tests/state_check.h` — canonical comparison and digest coverage for exception snapshots.
- `tests/gates/core_mutation.cmake` — bank alias, IRQ LR, and vector continuation mutants with semantic failure assertions.
- `tests/core.cmake`, `tests/inventory/ci.txt`, and `tests/inventory/asan.txt` — focused CPU selectors and controls registered in both lanes.

## Decisions and Scope

Exception entry uses explicitly named `current_address`, `next_address`, and `fault_address` values: SWI/undefined/coprocessor use current plus instruction width, prefetch abort uses fault plus four, data abort uses fault plus eight, IRQ/FIQ use next plus four, and reset uses LR zero as a project policy. Reset, abort, IRQ, and FIQ evidence uses direct API injection; no guest IRQ controller or hardware trigger was added. Valid but unsupported instruction families remain separate from reserved undefined encodings. Full MOVS/SUBS exception-return behavior remains Phase 5 work.

No BIOS/HLE, private ROM execution, rights change, dependency, or hardware-accuracy claim was introduced. The cycle assertion establishes the declared project budget only.

## Deviations from Plan

### Auto-fixed control issue

**1. [Rule 1 - Bug] Make the vector mutant reach a fetch**
- **Found during:** Task 3.
- **Issue:** The first vector mutant cleared the stop latch, but the CPU run loop still returned immediately after exception entry, so the mutant did not attempt a vector fetch.
- **Fix:** The mutant now also continues the exception dispatch loop, and the no-BIOS witness leaves enough budget to expose a vector refill while still requiring the normal result to stop at three cycles with unchanged handler bytes.
- **Files modified:** `tests/gates/core_mutation.cmake`, `tests/cpu_test.c`.
- **Verification:** `cpu.nc.vector_fetch` passed in the focused CI and ASan runs by detecting the deliberately continued vector fetch; the clean companion passed.
- **Committed in:** `5af568c`.

**Total deviations:** 1 auto-fixed (Rule 1). The change strengthens the planned negative control and does not expand product scope.

## Known Stubs

No new stub was introduced. The scan found the existing master-cycle placeholder in `include/gamebloke/gamebloke.h:293`; it predates this plan and is already recorded in `.planning/WINDOWS.md`.

## Outstanding Planning Flags

- `PROF-03` remains unresolved/unclassified from the phase's spec-less probe; these concrete CPU witnesses do not settle its taxonomy.
- Two descriptor-less standing-policy restrictions remain flagged-unverified: do not repurpose private/rights-bearing inputs or use synthetic metadata to grant rights, and do not inflate hardware, BIOS, or compatibility claims. No acceptance or human sign-off was fabricated.

## Next Phase Readiness

The mode and exception contracts and their semantic gates are ready for later CPU expansion. This slice does not establish physical hardware accuracy, broad compatibility, BIOS behavior, or rights to any test content. Parent workflow owns phase-state and requirement roll-up.

---
*Plan: 04-04*
*Completed: 2026-11-11*

## Self-Check: PASSED

- Summary file exists, names all eight implementation/test files, and records measured plan commits.
- All five task commits are ancestors of the verified plan head.
- Focused CI, focused ASan/UBSan, canonical CI, and the post-run inventory check passed with only the documented allowed skip.
