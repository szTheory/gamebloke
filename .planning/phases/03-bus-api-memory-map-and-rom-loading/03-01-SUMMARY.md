---
phase: 03-bus-api-memory-map-and-rom-loading
plan: "01"
subsystem: bus
tags: [c17, bus-api, ewram, deterministic-state, cmake, ctest]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides:
      - Host-independent C17 core target and in-tree C test harness
      - CI and ASan test registration with sorted lane inventories
provides:
  - Caller-owned machine initialization, reset, and disposal with explicit zero-fill
  - Checked one-cycle EWRAM reads/writes and bus-owned master-clock advancement
  - Timing and deterministic-state tests plus the internal machine/bus contract guide
affects: [phase-03-memory-map, phase-03-rom-loader, phase-04-cpu]

actuals:
  tokens: 9996
  tasks: 3
  commits: 4
plan_head_before: cad82cedaead5b0a66fd21777daf9f8040a9e31b
plan_head_after: 077d7eb85261f3d8869d0091a3c07ae3bb7c10d9
commits: 4

tech-stack:
  added: []
  patterns:
    - "Raw bus transfers use bytewise little-endian access and leave CPU alignment behavior to the CPU layer"
    - "Bus requests preflight clock overflow before memory or latch effects, then commit time once"
    - "Canonical state checks compare fields, arrays, and ROM content rather than padding or pointer identity"

key-files:
  created:
    - src/machine.c
    - src/bus.c
    - tests/bus_test.c
    - docs/bus-api.md
  modified:
    - include/gamebloke/gamebloke.h
    - CMakeLists.txt
    - tests/version_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Each successful raw access costs one project-policy master cycle and returns the caller's requested sequence class; clock overflow refuses atomically."
  - "Reset zeroes canonical machine state while preserving ROM and allocator ownership; disposal detaches resources, and the host synchronizes each instance."

patterns-established:
  - "Fresh and reset machine comparisons enumerate canonical state and exclude struct padding and pointer identity."
  - "The internal bus documents unsupported regions and provisional timing without claiming hardware accuracy."

requirements-completed: [BUS-01, BUS-03]

coverage:
  - id: D1
    description: "core.version performs a real little-endian EWRAM write/read through gamebloke_core and observes exact cycle cost, returned class, and clock."
    requirement: BUS-01
    verification:
      - kind: unit
        ref: 'ctest --test-dir build/ci -R "^core\.version$" --no-tests=error --output-on-failure'
        status: pass
    human_judgment: false
  - id: D2
    description: "Focused tests cover EWRAM boundaries, width and latch behavior, exact timing, invalid arguments, clock overflow atomicity, reset zero-fill, and independent machine storage."
    requirement: BUS-03
    verification:
      - kind: unit
        ref: 'ctest --test-dir build/ci -R "^(bus\.timing|machine\.state)$" --no-tests=error --output-on-failure'
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci: 265 expected, 263 ran, 2 allowed skips; inventory post-step passed"
        status: pass
    human_judgment: false
  - id: D3
    description: "docs/bus-api.md records field ownership, lifecycle and refusal postconditions, host synchronization, costs, raw transfer behavior, and the current implementation boundary."
    verification: []
    human_judgment: true
    rationale: "The guide is a prose contract; CI checks the implementation and inventories but does not assert that the documentation is complete."

duration: 26min
completed: 2026-11-10
status: complete
---

# Phase 03 Plan 01: Timed EWRAM Bus and Machine State Summary

**Caller-owned machine lifecycle and a one-cycle EWRAM bus with failure-atomic clock overflow handling.**

## Performance

- **Duration:** 26 min
- **Started:** 2026-11-10T15:35:44Z
- **Completed:** 2026-11-10T16:01:49Z
- **Tasks:** 3
- **Files modified:** 10

## Accomplishments

- Added the caller-owned machine layout, allocator descriptor, bounded bus result enums, and explicit init/reset/dispose lifecycle. The core now reads and writes mirrored EWRAM through a single timing-bearing bus path.
- Extended `core.version` into a linked EWRAM round-trip witness and added `bus.timing` and `machine.state` checks for exact costs/classes, boundary decoding, invalid arguments, overflow refusal, reset zero-fill, and independent instances.
- Documented ownership, refusal behavior, synchronization, placeholder timing, raw little-endian transfers, latch policy, and the limited EWRAM scope in `docs/bus-api.md`.

## Task Commits

1. **Task 1: A fresh machine performs one timed EWRAM round trip** - `b8d6a1b` (test-first smoke witness), `dac4934` (implementation)
2. **Task 2: Give callers focused timing and initialization evidence** - `f734020` (tests and lane inventories)
3. **Task 3: Close state and overflow edge contracts** - `077d7eb` (checked preflight, lifecycle edge handling, tests, and guide)

## Verification Evidence

- The Task 1 RED witness ran against a temporary stand-in returning `INVALID_ARGUMENT`; `core.version` failed on the expected machine and bus assertions (exit 1). The temporary files were removed before the implementation commit.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^core\.version$' --no-tests=error --output-on-failure` passed, followed by `core.no_host_includes` passing for the new production sources.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(bus\.timing|machine\.state)$' --no-tests=error --output-on-failure` passed both focused tests. The selectors printed `bus: timing ok` and `machine: state ok`; an unknown selector returned exit 2 with its usage error.
- The final `cmake --workflow --preset ci` passed all 265 tests. The post-step reported 265 expected, 263 run, and 2 allowed skips (`signature.logo.real_digest` and `signature.owner_local`).

## Files Created/Modified

- `src/machine.c` - Explicit caller-owned machine initialization, reset, and disposal.
- `src/bus.c` - Checked EWRAM access, open-bus behavior, and the sole ordinary access/internal-cycle clock authority.
- `tests/bus_test.c` - Timing, state, overflow, reset, allocator validation, and instance independence checks.
- `docs/bus-api.md` - Internal machine/bus contract and implementation boundary.
- `include/gamebloke/gamebloke.h` - Machine, allocator, result, and API declarations.
- `CMakeLists.txt`, `tests/core.cmake`, `tests/version_test.c` - Real core linkage and test registration.
- `tests/inventory/ci.txt`, `tests/inventory/asan.txt` - Sorted matching inventory entries for the new tests.

## Decisions Made

- Each successful raw request uses a one-cycle project policy while preserving the requested access class; it makes no WAITCNT, prefetch, or hardware-accuracy claim.
- Overflow is detected before memory or latch effects. Reset preserves ROM and allocator ownership; disposal clears and releases detached resources.
- Raw bus addresses are interpreted as little-endian consecutive bytes. CPU misalignment handling remains outside this bus layer.

## Deviations from Plan

None - plan executed as written.

## Issues Encountered

The first Task 2 configure rejected the `machine.state` CI inventory row as unsorted. The row was moved to byte order; configure/build and both focused tests then passed.

## Known Stubs

None. The one-cycle cost is an intentional documented project policy. The loaded-ROM reset invariant is not demonstrated in this plan and remains assigned to the loader witness in plan 03-03.

## Threat Flags

None outside the plan threat model. Width, class, address-span, clock-overflow, and instance-isolation cases are covered here.

## User Setup Required

None - no external service configuration or dependency install is required.

## Next Phase Readiness

The bus and lifecycle seam is ready for the remaining Phase 3 profile, loader, and memory-map work. Reset-with-installed-ROM equivalence remains unverified until plan 03-03 adds a real loader witness. Phase 3 remains in progress with five plans still open.

## Self-Check: PASSED

- The summary and all four created files exist on disk.
- All four task commits are ancestors of HEAD; the plan ledger measures four commits from `cad82ced` through `077d7eb`.
- `gsd-tools uat classify-coverage` reports three deliverables and no schema errors.
- The canonical CI workflow passed with 265 expected tests, 263 run, and 2 allowed skips; the inventory post-step passed.

