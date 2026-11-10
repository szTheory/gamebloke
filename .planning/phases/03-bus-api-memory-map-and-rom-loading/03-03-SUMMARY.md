---
phase: 03-bus-api-memory-map-and-rom-loading
plan: "03"
subsystem: loader
tags: [c17, rom-loading, transactional-ownership, validation, ctest]

requires:
  - phase: 03
    provides:
      - Caller-owned canonical machine lifecycle and cold reset/dispose behavior
      - Versioned deterministic initialization policy
  - phase: 02
    provides:
      - Host-independent C17 core and in-tree CTest harness
provides:
  - Bounded copy-on-load ROM entry point with distinct refusal and resource results
  - Field-wise atomicity and allocator lifecycle tests on fresh and loaded instances
  - Documented host allocation, lifetime, replacement, and refusal contract
affects: [phase-03-memory-map, phase-03-load-integration, phase-05-header-diagnostics]

actuals:
  tokens: 7617
  tasks: 3
  commits: 4
plan_head_before: fe35b6f4f2e16201f168ca397be0f82da2df26ee
plan_head_after: f56c14b20fcc1301d7f5868b1848baf933dd95c6
commits: 4

tech-stack:
  added: []
  patterns:
    - "Validate ordered size bounds before allocation or image reads, then copy into host-owned candidate storage."
    - "Compare canonical fields, arrays, ROM bytes, and caller bytes; use a streaming digest only as secondary evidence."
    - "Commit and cold-reset the replacement before releasing the prior ROM allocation."

key-files:
  created:
    - src/loader.c
    - tests/loader_test.c
    - tests/state_check.h
  modified:
    - include/gamebloke/gamebloke.h
    - CMakeLists.txt
    - tests/version_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - docs/bus-api.md

key-decisions:
  - "For an initialized machine, size refusals have stable precedence: zero, below 0xC0, above 32 MiB, then odd; valid-length null inputs and missing callbacks are invalid arguments."
  - "The core has no default allocator: trusted host callbacks provision fresh disjoint storage and release the prior owned span only after commit and reset."
  - "Loader status and reason return by value; no caller diagnostic buffer is written on refusal. Header diagnostics remain in the later header plan."

patterns-established:
  - "Canonical equality enumerates current machine fields and arrays, compares ROM contents rather than pointer identity, and excludes padding and allocator bookkeeping."
  - "Replacement release callbacks may observe the committed cold machine read-only, but must not mutate or reenter it."

# BUS-03 is intentionally left phase-pending until integrated evidence and the final verifier.
requirements-completed: [LOAD-01, LOAD-03]

coverage:
  - id: D1
    description: "A synthetic image of exactly 0xC0 bytes and one of exactly 32 MiB load into distinct owned storage; size refusals follow the documented order."
    requirement: LOAD-01
    verification:
      - kind: unit
        ref: "tests/version_test.c#core.version; tests/loader_test.c#loader.lengths"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every refusal and allocation failure preserves canonical fields, arrays, prior ROM contents, and caller input on fresh and loaded non-default machines."
    requirement: LOAD-03
    verification:
      - kind: unit
        ref: "tests/loader_test.c#loader.transaction"
        status: pass
    human_judgment: false
  - id: D3
    description: "Replacement commits and cold-resets the candidate before releasing the prior ROM; repeated reset preserves ROM and matches an independently loaded instance."
    requirement: BUS-03
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
    human_judgment: true
    rationale: "This plan proves loaded-reset equivalence; BUS-03 remains phase-pending until integrated phase evidence and the final verifier cover its full scope."
  - id: D4
    description: "The internal guide records size precedence, source and ROM lifetimes, allocator obligations, failure atomicity, and the no-file boundary."
    verification: []
    human_judgment: true
    rationale: "The guide is a prose contract; automated tests do not establish that its wording is complete."

duration: 20min
completed: 2026-11-10
status: complete
---

# Phase 03 Plan 03: Transactional ROM Loading Summary

**Bounded synthetic ROM loads now own a copy, preserve all canonical state on refusal, and cold-reset before releasing replaced storage.**

## Performance

- **Duration:** 20 min
- **Started:** 2026-11-10T16:53:09Z
- **Completed:** 2026-11-10T17:13:34Z
- **Tasks:** 3
- **Files modified:** 10

## Accomplishments

- Added `gbl_load_rom`, `gbl_rom_data`, and `gbl_rom_size`, plus distinct by-value load statuses and reasons. The core uses only the host-supplied allocator and a bounded byte-copy loop.
- Proved the exact lower and upper bounds, precedence for overlapping malformed lengths, null-input handling, allocator failure, caller-buffer isolation, and field-wise refusal atomicity on fresh and already-loaded instances.
- Proved successful replacement and repeated reset/disposal ownership. The old-allocation callback observes the new ROM and zeroed mutable machine state before release; the guide now documents those host obligations.

## Task Commits

1. **Task 1: Copy a synthetic ROM through the real host-owned lifecycle** - `2e24f09` (failing test), `014ffd5` (implementation)
2. **Task 2: Prove boundary precedence and full refusal atomicity** - `334bdfa` (tests and inventories)
3. **Task 3: Make replacement and reset ownership observable** - `f56c14b` (release-order test and contract guide)

The four plan commits are measured from `fe35b6f4` through `f56c14b2`. The planning metadata commit is created separately after state and roadmap updates.

## Verification Evidence

- **Task 1 RED:** The real `core.version` target ran against a fail-closed placeholder and failed the planned load-success and ownership assertions. Its fresh JUnit report was accepted by `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK` (`target_test_failed`); the run had one test, one assertion failure, zero skipped, and no report errors.
- **Task 1 GREEN:** `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^core\.version$' --no-tests=error --output-on-failure` passed. The `core.no_host_includes` gate also passed, and the tracer verification was rerun successfully after the implementation commit.
- **Task 2:** `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.(lengths|transaction)$' --no-tests=error --output-on-failure` passed both selectors. The unsupported selector returned the expected usage exit code 2.
- **Task 3 / overall:** `cmake --workflow --preset ci` passed all 274 registered tests and the inventory post-step. The two existing owner-gated signature checks (`signature.logo.real_digest` and `signature.owner_local`) remained the only skips, with their recorded reasons.
- An extra ASan-lane attempt stopped at configure because the configured `clang-21` compiler is not installed in this environment. The required task verifications and canonical CI workflow passed; no compiler or package was installed.

## Files Created/Modified

- `src/loader.c` - Ordered validation, host-allocated copy, atomic install/reset/release, and const ROM accessors.
- `include/gamebloke/gamebloke.h` - Loader result/status/reason and accessor declarations.
- `tests/version_test.c` - Linked 0xC0-byte synthetic copy and disposal witness.
- `tests/loader_test.c` - Length, refusal, resource failure, ownership, replacement, reset, and disposal checks.
- `tests/state_check.h` - Canonical field/array/ROM equality and secondary deterministic digest helpers.
- `CMakeLists.txt`, `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` - Core source and test registration in both lanes.
- `docs/bus-api.md` - Loader size precedence, callback contract, source lifetime, refusal postconditions, and no-file boundary.

## Decisions Made

- Length validation occurs after the machine validity check and before input-pointer or allocator validation: zero, short, too large, odd, then valid-size argument checks.
- Allocation failure has its own resource status. All refusals and failed allocations leave machine contents and caller bytes untouched.
- Successful replacement installs the owned candidate and cold-resets mutable state before releasing the previous allocation. No hardware-accuracy, broad-compatibility, or canonical-logo claim is made.

## Deviations from Plan

None - the planned loader behavior, tests, documentation, and task boundaries were completed. No dependencies or ROM/BIOS/logo files were added.

## Issues Encountered

- The first RED attempt exposed a null dereference in the new test's failure path. The test was guarded so an absent ROM reports assertion failures without crashing; the next run produced valid, classifier-approved RED evidence.
- The extra ASan attempt could not configure because `clang-21` is unavailable. This did not affect the required CI verification.

## User Setup Required

None. The existing logo-digest gate remains owner-pending; this plan used generated in-memory synthetic bytes only.

## Next Phase Readiness

- The loader ownership seam and refusal evidence are ready for plan 03-04's installed-ROM bus decode.
- Phase 3 remains in progress. BUS-03 stays globally pending until integrated evidence and final verification; the owner-pending canonical-logo gate remains unchanged.
- Header diagnostics and canonical logo comparison remain assigned to the later loader-header plan.

---
*Phase: 03-bus-api-memory-map-and-rom-loading*
*Completed: 2026-11-10*

## Self-Check: PASSED

- The summary, loader implementation, loader test, and canonical-state helper exist on disk.
- All four plan commits are ancestors of the current Phase 3 branch head.
- `gsd-tools check evaluation-scope --plan 03-03 --commits-only` resolved all four plan commits with no unreachable or missing commits.
- The coverage classifier reported no schema errors; LOAD-01 and LOAD-03 are backed by passing test selectors, while BUS-03 remains phase-pending.
