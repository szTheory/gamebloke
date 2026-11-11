---
phase: 04-cpu-execution-core-and-headless-runner
plan: "06"
subsystem: cpu/inspection
tags: [C17, IWRAM, mailbox, CPU, CMake, CTest, ASan, UBSan]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: "The bus-backed ARM subset, checked IWRAM writes, and canonical machine state from plans 04-01 through 04-05."
provides:
  - "Byte-bounded, read-only physical IWRAM peek with explicit refusal postconditions."
  - "Complete versioned mailbox observations that distinguish absent, pending, pass, fail, and malformed data."
affects: [headless runner, CPU tests, Phase 4 runner integration]

# Actuals (#2632), chars/4 over the realized diff; refreshed before close-out.
actuals:
  tokens: 10692
  tasks: 2
  commits: 3
commits: 3
plan_head_before: 797d9c854a5bf39a576e5d1e410776942870b2e5
plan_head_after: e0bf5d1d66b1091dfaf91e1f19877523c0562fa5

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Inspection copies from canonical RAM without using the timing-bearing bus."
    - "Mailbox PASS/FAIL requires magic, version, terminal state, and a final completion marker."

key-files:
  created:
    - src/inspect.c
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-06-SUMMARY.md
  modified:
    - include/gamebloke/gamebloke.h
    - tests/cpu_test.c
    - CMakeLists.txt
    - tests/core.cmake
    - tests/gates/core_mutation.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Peek accepts byte spans, including unaligned spans, only inside physical IWRAM; zero-length reads may use the one-past-end address."
  - "The mailbox is five little-endian words at 0x03007FC0, and guest code writes the completion marker last."
  - "A completed FAIL with case ID zero is malformed because it cannot identify the failing case."
  - "The current core has no trace API or trace state; inspection does not add one."

patterns-established:
  - "Compare every canonical field and owned byte plus same-instance identity after inspection; a digest is secondary evidence."
  - "Scratch mutation controls compile the same inspection source and prove that a planted machine-time effect fails the neutrality assertion."

requirements-completed: [] # Parent owns requirement roll-up; this is partial RUN-01/RUN-02 evidence.
coverage:
  - id: D1
    description: "A real guest store sequence produces only a complete versioned mailbox verdict, with state-neutral IWRAM observation and explicit refusal behavior."
    verification:
      - kind: unit
        ref: "cpu.peek; cpu.mailbox"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci; inventory.junit"
        status: pass
    human_judgment: false
  - id: D2
    description: "Configured and scratch builds include the same inspection source; a time-mutating peek is caught by the semantic neutrality assertion."
    verification:
      - kind: integration
        ref: "cpu.nc.peek_effect; cpu.pos.clean"
        status: pass
      - kind: integration
        ref: "Fresh build/asan focused CPU CTest selectors"
        status: pass
    human_judgment: false

# Metrics
duration: 21min
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 6: Neutral IWRAM Peek and Versioned Mailbox Summary

**A real guest-written mailbox can be read without changing CPU, bus, memory, or ownership state, and incomplete writes never count as a verdict.**

## Performance

- **Duration:** About 21 minutes.
- **Started:** 2026-11-11T06:12:30Z.
- **Completed:** 2026-11-11T06:33:00Z.
- **Tasks:** 2/2.
- **Implementation and test files:** 8 (one created, seven modified).
- **Canonical CI critical path:** 76.55 seconds for 317 inventoried tests; 316 passed and the allowed `signature.owner_local` check was skipped. `inventory.junit` passed with the same count. Configure took about 0.1 seconds and the build had no work, so the workflow did not duplicate setup or compilation.

## Accomplishments

- Added `gbl_machine_peek`, a const-machine copy from physical IWRAM (`0x03000000..0x03007fff`). It validates the complete source span, caller buffer extent, and overlap with the machine object or its owned ROM before copying. Zero-length reads accept any in-range address and the one-past-end address without accessing the output pointer.
- Added the five-word little-endian mailbox at `0x03007fc0`: magic, version, state, case ID, and final completion marker. Reads return absent, pending, pass, fail, or malformed. A running state and partial terminal marker stay pending; a failure without a case ID is malformed.
- Added an owned ARM guest program that writes the mailbox through supported LDR/STR instructions. With a 22-cycle budget the terminal state is visible but the marker is absent and the result remains pending; the final 2-cycle store completes the PASS exactly at cycle 24. A separate guest program produces a stable FAIL case ID.
- Compared the full populated canonical machine state, digest, ROM and allocator identity, caller output guards, and an adjacent trace canary around valid and refused reads. The read path does not call the timing-bearing bus and introduces no persistent machine fields.
- Registered `cpu.peek` and `cpu.mailbox` in CI and ASan inventories. The scratch `cpu.nc.peek_effect` mutant advances master time and is rejected by the full-state assertion; `cpu.pos.clean` exercises both selectors with clean scratch sources.

## Verification

- **TDD RED:** The focused `peek` TAP run failed its named pending-mailbox assertion against a nonfunctional seam (exit 1). `gsd_run check tdd-red-evidence build/phase04-tracer/04-06-red-peek.json --raw` returned `RED_EVIDENCE_OK`. A separate diagnostic confirmed the real guest loaded, reached the PASS-state store at cycle 22, and left the final marker word zero; the failure was the intended missing inspection behavior, not compilation or discovery.
- **TDD GREEN:** Strict C17 compilation with warnings-as-errors followed by `cpu_test.peek` and `cpu_test.mailbox` passed all 9 assertions. The task 1 tracer rerun passed after its GREEN commit.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(cpu\.(peek|mailbox|nc\.peek_effect)|core\.no_host_includes)$' --no-tests=error --output-on-failure` — 4/4 passed, including the scratch mutant/clean companion and the freestanding core include gate.
- Fresh local sanitizer configuration: `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --test-dir build/asan -R '^cpu\.(peek|mailbox|nc\.peek_effect)$' --no-tests=error --output-on-failure` — 3/3 passed with AppleClang 21 ASan/UBSan. After the core include correction, the same focused selectors were rebuilt and rerun: 3/3 passed.
- `cmake --workflow --preset ci` — all 317 inventory entries accounted for; 316 passed and one permitted owner-local signature test skipped because `GBL_OWNER_LOGO_FILE` was unset. The existing owner-authorized local refresh against both supplied inputs is separately recorded in `build/phase04-tracer/owner-local-refresh.log`; this public-lane skip is not a new owner-data gate.
- `git diff --check` — passed.

## Task Commits

1. **Task 1 RED:** `310b800` — failing guest mailbox and neutral peek witnesses.
2. **Task 1 GREEN:** `715450b` — bounded IWRAM peek and complete mailbox reader.
3. **Task 2:** `e0bf5d1` — registered inspection selectors, scratch mutation, and lane inventories.

**Plan metadata:** Summary commit is recorded separately after this document's self-check.

## Files Created/Modified

- `src/inspect.c` — bounded pointer/span validation, read-only IWRAM copy, mailbox parsing.
- `include/gamebloke/gamebloke.h` — status/result contracts, mailbox constants and states, API documentation.
- `tests/cpu_test.c` — guest stores, mailbox cases, read/refusal boundaries, full-state and caller-buffer checks.
- `CMakeLists.txt` — shared source list for production and synthetic configured cores.
- `tests/core.cmake` and `tests/gates/core_mutation.cmake` — selectors, scratch compilation, time-effect mutant and clean companion.
- `tests/inventory/ci.txt` and `tests/inventory/asan.txt` — sorted lane entries.

## Decisions Made

The peek is a byte operation, so unaligned spans are valid and no CPU or bus alignment policy is implied. The zero-length one-past-end case is accepted because it accesses no IWRAM byte. Outputs may not alias machine storage or the owned ROM allocation. Mailbox results are only complete when all protocol fields agree and the final marker is present; zero-filled IWRAM is absent, not PASS. These choices implement the existing Phase 4 boundary and do not establish hardware timing or compatibility.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Keep the core peek free of host headers**
- **Found during:** Task 2 canonical CI integration.
- **Issue:** The first canonical workflow rejected `<string.h>` in `src/inspect.c` because the project requires freestanding core sources.
- **Fix:** Replaced `memcpy` with a small bounded byte-copy helper after all validation and overlap checks.
- **Files modified:** `src/inspect.c`.
- **Verification:** `core.no_host_includes`, focused CI/ASan selectors, and the rerun canonical workflow all passed.
- **Committed in:** `e0bf5d1`.

**Total deviations:** 1 auto-fixed (Rule 3). The change preserves the planned interface and behavior.

## Known Stubs

- `include/gamebloke/gamebloke.h:341` retains the pre-existing project-policy placeholder for one master cycle per bus access. It predates this plan and is already tracked in the phase ledger; Plan 04-06 does not change its timing policy.

## Outstanding Planning Flags

- `RUN-02` remains partial evidence; the flagged spec-less edge-probe taxonomy is unresolved and is not completed by these concrete tests.
- This plan does not claim hardware accuracy, BIOS/HLE behavior, broad compatibility, private ROM execution, fixture rights, or release qualification. No new dependency was added.

## Next Phase Readiness

The headless runner can consume a neutral peek and a complete versioned guest verdict. Parent workflow owns requirement, roadmap, and state roll-up; this summary does not mark a full requirement or phase complete.

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Plan: 04-06*
*Completed: 2026-11-11*

## Self-Check: PASSED

- `src/inspect.c` and all seven modified implementation/test files exist.
- The three task commits (`310b800`, `715450b`, `e0bf5d1`) are ancestors of `plan_head_after`.
- Task verification, the fresh sanitizer focused run, the core include gate, and final canonical CI plus `inventory.junit` pass as recorded.
