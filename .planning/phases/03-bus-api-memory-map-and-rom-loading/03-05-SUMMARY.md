---
phase: 03-bus-api-memory-map-and-rom-loading
plan: "05"
subsystem: loader
tags: [c17, gba, rom-header, diagnostics, mutation-testing, rights-boundary]
requires:
  - phase: 03-bus-api-memory-map-and-rom-loading
    provides: "Copied-ROM ownership, transactional loading, machine lifecycle, and semantic bus-test patterns"
provides:
  - "Bounded advisory diagnostics for logo-window zero/nonzero, header checksum, and game-code convention"
  - "By-value diagnostic access plus a synthetic-only byte-span comparison helper"
  - "Five semantic loader mutation controls with matching clean runs in CI and ASan inventories"
affects: [phase-03, phase-04-cpu]
actuals:
  tokens: 6347
  tasks: 2
  commits: 4
commits: 4
plan_head_before: 67a3039f067458322ca2158b6f4aabe613a6c0f7
plan_head_after: 7e2cbe0826e69d186a64add07042cc607b7907b5
tech-stack:
  added: []
  patterns: ["Parse only the copied candidate before atomic commit", "Scratch-compile semantic loader mutants with clean companions"]
key-files:
  created: []
  modified:
    - include/gamebloke/gamebloke.h
    - src/loader.c
    - src/machine.c
    - tests/loader_test.c
    - tests/state_check.h
    - docs/bus-api.md
    - tests/gates/core_mutation.cmake
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - "Header findings are advisory for every size-valid direct-start image and commit only with a successful copied-ROM load."
  - "Canonical logo stays UNCHECKED/OWNER_PENDING; synthetic equality and mismatch exercise comparison mechanics only."
  - "LOAD-02 remains PARTIAL and globally Pending; BUS-03 remains Pending for phase integration and final verification."
requirements-completed: [LOAD-01, LOAD-03]
coverage:
  - id: D1
    description: "Fixed-bound header findings load successfully and report checksum/game-code/window enums; reset preserves committed diagnostics."
    requirement: LOAD-02
    verification:
      - kind: unit
        ref: "loader.diagnostics"
        status: pass
    human_judgment: false
  - id: D2
    description: "Synthetic spans exercise equal/different comparison while the canonical logo remains unchecked and owner-pending."
    verification:
      - kind: unit
        ref: "loader.diagnostics"
        status: pass
    human_judgment: true
    rationale: "The real canonical-logo comparison remains gated on the owner's digest; synthetic data cannot satisfy it."
  - id: D3
    description: "Refusal and allocation-failure mutants, caller-byte mutation, and false logo claims fail real assertions; clean companions pass."
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
    human_judgment: false
duration: 23min
started: 2026-11-10T18:00:00Z
completed: 2026-11-10T18:23:13Z
status: complete
---

# Phase 3 Plan 5: Advisory Header Diagnostics Summary

**The loader now reports bounded advisory header findings from its owned copy, while canonical logo verification stays owner-pending.**

## Performance

- **Duration:** approximately 23 minutes
- **Started:** 2026-11-10T18:00:00Z
- **Completed:** 2026-11-10T18:23:13Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments

- Added by-value loader diagnostics for the zero/nonzero header window, modulo-256 checksum, and four-byte ASCII game-code convention. A valid-size image always loads; diagnostics are committed only on success, survive reset, and are cleared at initialization/disposal.
- Added a generic byte-span comparison helper and synthetic equality/mismatch cases. The canonical result remains `UNCHECKED` with `OWNER_PENDING`, including for an all-zero window; no real logo data or digest was read, computed, or added.
- Added five loader fault controls for premature allocation, refusal-state mutation, allocation-failure mutation, caller-buffer mutation, and a false canonical-logo match. Each scratch mutant must compile and fail a semantic assertion, and clean companions must pass.

## Task Commits

1. **Task 1 RED: specify advisory diagnostics and comparison** — `b7c9909` (`test`)
2. **Task 1 GREEN: parse and preserve advisory diagnostics** — `30007e0` (`feat`)
3. **Rule 1 fix: correct checksum subtraction** — `0eb1004` (`fix`)
4. **Task 2: add loader mutation controls and documentation** — `7e2cbe0` (`test`)

## Files Created/Modified

- `include/gamebloke/gamebloke.h` — bounded diagnostic enums, structure, accessor, and comparison API.
- `src/loader.c` — candidate-only bounded header parsing, checksum, game-code and zero-window findings, and comparison mechanics.
- `src/machine.c` — explicit zero/not-loaded initialization and disposal state; reset leaves successful diagnostics intact.
- `tests/loader_test.c`, `tests/state_check.h` — fixed checksum vectors, synthetic header cases, reset/refusal/allocation-failure preservation, and canonical-state equality/digest coverage.
- `tests/gates/core_mutation.cmake`, `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — scratch semantic mutants, clean companions, and sorted lane registration.
- `docs/bus-api.md` — offsets, enum meanings, advisory behavior, community-source note, synthetic-only evidence, and explicit partial status.

## Verification Evidence

- **Task 1 RED:** The real `loader.transaction` selector and `loader_test diagnostics` executable ran and failed with `check failed` assertions for the unimplemented comparison and header findings; the failures were semantic, not compile errors or crashes.
- **Task 1 GREEN / tracer gate:** After `30007e0`, `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.transaction$' --no-tests=error --output-on-failure && build/ci/loader_test diagnostics` passed. The direct selector printed `loader: diagnostics ok`.
- **Task 2 targeted:** `ctest --test-dir build/ci -R '^loader\.' --no-tests=error --output-on-failure` passed all 9 selected tests, including all five semantic mutants and `loader.pos.clean`.
- **Full local CI workflow:** `cmake --workflow --preset ci` passed all 289 registered tests and the inventory post-step. 287 tests passed; the two pre-existing owner-gated signature checks skipped with their existing reasons: `logo digest owner-pending` and `GBL_OWNER_LOGO_FILE not set`. Test runtime was 59.38 seconds; the CI post-step took 0.09 seconds. The new ASan inventory entries were checked by the inventory rules; no separate ASan workflow was run.
- The all-zero checksum vector is `0xE7`; adding one at `0xA0` makes it `0xE6`. The ordinary synthetic `AB19` code with a one-byte `0xA0` contribution uses `0xF9`. This corrected an initial subtraction-direction bug before the full workflow.

## Decisions Made

- Header offsets are bounded by the validated minimum `0xC0`-byte image. The checksum starts at zero, subtracts bytes `0xA0`–`0xBC` and `0x19` modulo 256, then compares `0xBD`.
- An all-zero `[0x04, 0xA0)` window is labeled `ZERO` as a project convention. It does not count as a canonical match. Only ASCII uppercase letters or digits across `[0xAC, 0xB0)` are ordinary game-code bytes; the code itself is not retained.
- The GBATEK 3.06 cartridge-header section is cited as a community reference, not hardware qualification.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected the header checksum subtraction direction**
- **Found during:** Task 2 review
- **Issue:** The initial implementation added `0x19` before subtracting header bytes, contrary to the plan's specified modulo-256 subtraction.
- **Fix:** Subtract all checksum bytes and `0x19` from zero; add fixed `0xE7` / `0xE6` vectors and fixed ordinary/mismatch cases.
- **Files modified:** `src/loader.c`, `tests/loader_test.c`, `docs/bus-api.md`
- **Verification:** All 9 loader tests and the full canonical workflow passed.
- **Committed in:** `0eb1004`

**2. [Owner constraint] Did not read the owner logo-signature file**
- **Found during:** Task 1 required reads
- **Issue:** The plan's `read_first` list includes `tests/signatures/logo.sha256`, while the explicit owner instruction prohibits retrieving a real logo digest.
- **Fix:** Left that file and any owner-local input untouched; verified its tracked path had no diff without reading its contents.
- **Files modified:** None.

**Total deviations:** 2 (one correctness fix and one owner-gate read constraint).
**Impact on plan:** The checksum behavior now follows the plan; synthetic data remains limited to mechanic tests, and the real-logo acceptance gap stays open.

## Issues Encountered

- The sandbox initially denied writes to `.git`. The authorized scoped escalation succeeded for the task commits and the repo-local plan ledger. Existing user changes in `.planning/config.json`, `.planning/state.json`, `AGENTS.md`, and `.planning/research/.cache/` remained unstaged.
- `state.update-progress` declined because GSD classifies the active phase scope as unscoped/incomplete. The ROADMAP records 5/6 Phase 3 plans In Progress, and the existing global 33% progress bar was left unchanged.
- `state.advance-plan` advanced the plan number but kept the old `03-05 next` label; the next-plan label and activity text were corrected to `03-06 next` while Phase 3 remains executing.

## User Setup Required

None for this plan. LOAD-02 stays **PARTIAL/Pending** until the owner supplies the canonical logo digest; this plan did not read or create one. BUS-03 stays Pending until phase integration and final verification.

## Next Phase Readiness

Ready for plan 03-06. Phase 3 remains in progress; no phase completion, requirement promotion, or transition occurred.

---
*Phase: 03-bus-api-memory-map-and-rom-loading*
*Completed: 2026-11-10*

## Self-Check: PASSED

- The summary and all 10 planned implementation, test, and documentation files exist.
- All four task commits are ancestors of the current Phase 3 branch head; the plan ledger measures exactly four commits before the metadata commit.
- `git diff --check` passes, and `tests/signatures/logo.sha256` is unchanged.
- The GSD evaluation-scope check resolves all four plan commits with no missing or unreachable commits.
