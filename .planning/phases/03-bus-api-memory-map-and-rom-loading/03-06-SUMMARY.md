---
phase: "03-bus-api-memory-map-and-rom-loading"
plan: "06"
subsystem: testing
tags: [C, CMake, CTest, ASan, UBSan, bus, loader, integration]
requires:
  - phase: "03-01 through 03-05"
    provides: Deterministic machine, bus map and timing policy, host-owned loader, and header diagnostics.
provides:
  - A linked consumer-style integration selector for loaded bus access, independent interleaving, reset, refusal, and disposal.
  - Reproducible current CI, hygiene, tree-scan, and sanitizer evidence with inventory summaries and explicit owner gates.
affects: [phase-03-review, future-core-consumers, verification]
actuals:
  tokens: 10169
  tasks: 2
  commits: 2
tech-stack:
  added: []
  patterns:
    - Linked integration checks use separate host allocators and in-memory synthetic images.
    - Verification evidence distinguishes selected project behavior from hardware, installed-consumer, and rights claims.
key-files:
  created:
    - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md
  modified:
    - tests/loader_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - docs/bus-api.md
    - docs/verify.md
    - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-VALIDATION.md
key-decisions:
  - "Use only synthesized in-memory images; keep the real canonical-logo digest owner-gated and LOAD-02 partial."
  - "Keep the measured full evidence lanes: the existing semantic controls dominate runtime, with no duplicated product build or evidence-supported safe reduction."
patterns-established:
  - "Integration witness: compare semantic bus results and canonical machine state across isolated and interleaved runs."
  - "Evidence reports exact inventory totals, permitted skips, compiler identity, lane timings, and unverified claims."
requirements-completed: [BUS-01, LOAD-01, LOAD-03]
coverage:
  - id: D1
    description: "A linked C consumer loads and exercises independent machines through bus access, refused replacement, reset, and disposal."
    requirement: "LOAD-01"
    verification:
      - kind: integration
        ref: "loader.integration; final CI and ASan inventories"
        status: pass
    human_judgment: false
  - id: D2
    description: "The canonical-logo comparison result remains owner-gated; synthetic comparisons do not satisfy the real-logo requirement."
    requirement: "LOAD-02"
    verification:
      - kind: integration
        ref: ".planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md#remaining-gates-and-evidence-limits"
        status: pass
    human_judgment: true
    rationale: "Only the owner can supply the approved digest from a file they hold; LOAD-02 remains PARTIAL until the real mismatch result is verified."
  - id: D3
    description: "The current ROM-tail/open-bus behavior is documented as project policy without a hardware claim."
    requirement: "BUS-02"
    verification:
      - kind: integration
        ref: "loader.integration; synthetic alias and latch checks"
        status: pass
    human_judgment: true
    rationale: "The tests exercise deterministic project behavior but cannot establish physical hardware behavior or close the unclassified map probe."
# Dependency graph measurement at summary creation; the separate metadata commit follows.
plan_head_before: 3ab19359fbdc874146f36c444b1eba5234831a1f
plan_head_after: 76ec7c62e484db5307feef297ad9c4d383d92fa7
duration: "~25min (estimated from task commit timestamps; execution start was not instrumented)"
completed: 2026-11-10
status: complete
---

# Phase 03 Plan 06: Integrated Bus and Loader Evidence Summary

**A linked consumer-style test now exercises the assembled loaded-machine lifecycle, with measured CI and sanitizer evidence that preserves the open logo and hardware gates.**

## Performance

- **Duration:** About 25 minutes; estimated from task commit timestamps because the execution start was not separately instrumented.
- **Started:** Before 2026-11-10T13:41:07-05:00 (Task 1 commit; exact start unavailable).
- **Completed:** 2026-11-10T14:01:46-05:00.
- **Tasks:** 2.
- **Files modified:** 9 including this summary (8 task artifacts plus this summary).

## Accomplishments

- Added `loader.integration`, exercising owned-copy loading, ROM aliases, mirrored RAM, mode-sensitive video access, internal cycles, refusal and allocation-failure atomicity, diagnostics, replacement/disposal, reset, and independent interleaving through the linked core API.
- Updated the CI and ASan inventories and developer verification guide with focused selectors, profile reasons, scratch-control failure distinctions, and host callback/storage postconditions.
- Recorded final CI, hygiene, direct tree-scan, and ASan evidence, including actual inventory counts, compiler identity, durations, owner skips, and a measured no-change critical-path review.
- Kept `LOAD-02` PARTIAL and the BUS-02 ROM-tail behavior explicitly unverified; synthetic fixtures were used and no real digest or rights-bearing bytes were opened or generated.

## Task Commits

1. **Task 1: A consumer follows the complete loaded-machine lifecycle** — `a4bafa1` (`test`).
2. **Task 2: Record current verification and the exact remaining acceptance gap** — `76ec7c6` (`docs`).

The measured task-commit count is 2, from plan base `3ab19359fbdc874146f36c444b1eba5234831a1f` through Task 2 commit `76ec7c62e484db5307feef297ad9c4d383d92fa7`. The separate plan metadata commit follows this summary.

## Files Created/Modified

- `tests/loader_test.c` — added the consumer-style loaded integration selector.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — registered and enumerated the selector.
- `docs/bus-api.md`, `docs/verify.md` — documented lifecycle evidence, commands, callback rules, and evidence limits.
- `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md` — recorded measured verification and owner gates.
- `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-VALIDATION.md` — marked executed rows from current results while retaining draft/incomplete sign-off.

## Decisions Made

- Use separate synthetic inputs and host allocator contexts to compare isolated and interleaved machine behavior without ROM, BIOS, or owner digest fixtures.
- Retain the existing full CI and hygiene workflows. Their control labels dominate measured time, but each runs only one configure/build pair and the product build was already current; no safe evidence-preserving reduction was indicated.
- Keep phase-wide requirement transitions and source review with the orchestrator. This plan completes its evidence work without marking the overall phase validated.

## Deviations from Plan

**Task 1 TDD annotation:** No separate red-phase commit was made. The task adds an integration witness over behavior implemented by Plans 01–05 and does not change production behavior; once registered, the focused selector passed. No artificial production mutation was introduced to force a failing run.

No other deviations. The local ASan command used the specified installed-Clang override because the preset's `clang-21` executable is absent; preset and CI compiler mappings were unchanged.

## Issues Encountered

The final validation table initially retained “documentation rerun pending” after the reruns completed. It was corrected to the current passing state before commit. The final direct hygiene/signature scans both passed with 177 files, and `git diff --check` was clean.

## User Setup Required

No local service setup is required. The existing external owner action remains: provide an approved canonical-logo digest through the established review workflow. `LOAD-02` stays partial until then.

## Next Phase Readiness

Plan 06's tasks and required local evidence are complete. The phase itself remains in progress for orchestrator-owned review and bookkeeping. The owner digest and future hardware evidence are unresolved; this summary makes no release, hardware-accuracy, installed-consumer, stable-ABI, or broad compatibility claim.

---
*Phase: 03-bus-api-memory-map-and-rom-loading*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary, evidence, and validation files exist.
- Both task commits are ancestors of the current branch head.
- `git diff --check` reported no whitespace errors.
