---
phase: 03-bus-api-memory-map-and-rom-loading
plan: "04"
subsystem: bus
tags: [c17, gba, memory-map, video, rom-loading, mutation-testing]
requires:
  - phase: 03-bus-api-memory-map-and-rom-loading
    provides: "Plan 03-03 deterministic machine state and copied-ROM loading"
provides:
  - "Per-lane decoding for the documented bus windows, mirrors, and three ROM aliases"
  - "Mode-aware palette/VRAM/OAM byte stores through the timed bus"
  - "Semantic fault controls for five bus fault families, each with clean evidence"
affects: [phase-03-bus, phase-04-cpu]
actuals:
  tokens: 14346
  tasks: 3
  commits: 6
commits: 6
plan_head_before: ef95f7f5aa770ead4ca260f634db5f567b31b76a
plan_head_after: b6370dc946a287d25938e95fb103bee58a940bc7
tech-stack:
  added: []
  patterns: ["per-lane raw bus decode", "scratch-source semantic mutation controls"]
key-files:
  created: [tests/gates/core_mutation.cmake]
  modified: [src/bus.c, include/gamebloke/gamebloke.h, tests/bus_test.c, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt, docs/bus-api.md]
key-decisions:
  - "ROM-tail reads use the tested current-latch project policy; shorter-cartridge hardware behavior remains unverified."
  - "Only DISPCNT mode bits 0-2 are modeled; invalid mode threshold behavior is explicitly project policy."
  - "GBATEK 3.06 is cited as community reference evidence, not as hardware qualification."
patterns-established:
  - "Each raw access lane resolves its own region and physical mirror before applying width behavior."
  - "Mutation controls compile disposable source copies against real semantic tests and require clean companions."
requirements-completed: [BUS-01]
coverage:
  - id: D1
    description: "Timed bus decoding covers the documented regions and mirrors, loaded ROM aliases, and bounded current-latch tail policy."
    verification:
      - kind: unit
        ref: "ctest --test-dir build/ci -R '^bus\\.(decode|video)$' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
  - id: D2
    description: "DISPCNT mode and physical video byte-store rules are exercised through the bus."
    verification:
      - kind: unit
        ref: "ctest --test-dir build/ci -R '^bus\\.' --no-tests=error --output-on-failure"
        status: pass
    human_judgment: false
  - id: D3
    description: "Five deliberate bus fault families fail real assertions while their clean companions pass."
    verification:
      - kind: unit
        ref: "ctest --test-dir build/ci -R '^(bus\\.nc\\.|bus\\.pos\\.)' --no-tests=error --output-on-failure"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci"
        status: pass
    human_judgment: false
duration: 38min
started: 2026-11-10T17:19:48Z
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 4: Bus Decode, Video Stores, and Mutation Controls Summary

**The timed bus now decodes bounded ROM aliases and mapped memory lanes, applies mode-sensitive video byte stores, and proves five fault families with scratch-only mutants.**

## Performance

- **Duration:** approximately 38 minutes
- **Started:** 2026-11-10T17:19:48Z
- **Completed:** 2026-11-10T17:57:20Z
- **Tasks:** 3
- **Files modified:** 8 implementation, test, and guide files

## Accomplishments

- Added table-driven decode and video tests for full address identity, per-lane region selection, raw widths, physical mirrors, three 32 MiB ROM aliases, actual image extent, and fallback outcomes.
- Implemented the documented memory windows, with bounded ROM-tail reads using the current latch as an explicitly unverified project policy. Added a minimal DISPCNT mode subset and physical-offset-aware palette/VRAM/OAM byte-store rules.
- Added in-memory synthetic fault controls for EWRAM, IWRAM, and VRAM masks; BG/OBJ classification; OAM byte writes; and clock overflow. Every mutant was paired with a clean compilation and semantic pass.
- Expanded `docs/bus-api.md` with region, width, mirror, and policy behavior, and separated GBATEK source observations from Gamebloke policy choices.

## Task Commits

1. **Task 1: ROM tracer and decode cases** — `1a86637` (RED), `b7e546b` (RED expansion), `8225d8a` (GREEN).
2. **Task 2: Video mode and byte-store behavior** — `b406f50` (RED), `73e9cfe` (GREEN).
3. **Task 3: Semantic mutation controls** — `b6370dc` (test).

## Files Created/Modified

- `src/bus.c` — per-lane mapping, ROM alias bounds, fallback selection, mode state, and video byte stores.
- `include/gamebloke/gamebloke.h` — public bus outcomes for the modeled I/O subset and invalid-mode policy.
- `tests/bus_test.c` — fail-first decode/video cases, source citations, mirror and overflow assertions.
- `tests/core.cmake` — registered the five fault controls and clean positive companion.
- `tests/gates/core_mutation.cmake` — scratch-only mutation compilation and semantic checking.
- `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — sorted expected-test registrations.
- `docs/bus-api.md` — region/width/mirror policy and explicit limits.

## Decisions Made

- Each byte lane decodes the complete 32-bit address independently. ROM aliases use the address-window offset and actual loaded image extent without modulo wrapping.
- ROM-tail bytes return the corresponding current-latch lane as project policy. This is not a claim about shorter-cartridge hardware behavior; that remains an unresolved evidence item.
- Palette and BG VRAM byte writes duplicate into their physical halfword; physical OBJ VRAM and OAM byte writes are ignored. Mode 6–7 use the documented project threshold policy, with no hardware-accuracy claim.
- GBATEK 3.06 section names are attached to the relevant test cases as community-source evidence. They are not treated as Nintendo documentation or qualification.
- Backup space remains unmapped because this core does not yet implement a save device.

## Deviations from Plan

None. Existing timing tests were updated where newly mapped IWRAM changed the old unmapped-address surrogate. Control-driver label and output-match issues found during wiring were corrected before the focused controls and canonical workflow passed.

## Issues Encountered

The GSD `state.update-progress` handler declined because it classified the current phase scope as “unscoped.” The project milestone progress remains 2/6 phases (33%); the ROADMAP separately records Phase 3 at 4/6 plans. STATE remains in Phase 3, with Plan 03-05 next. No phase completion or requirement promotion was made.

## Verification

- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^bus\\.' --no-tests=error --output-on-failure` — passed the bus decode, timing, and video tests.
- `cmake --build --preset ci && ctest --test-dir build/ci -R '^(bus\\.nc\\.|bus\\.pos\\.)' --no-tests=error --output-on-failure` — all six registered controls passed; the video control independently checked reversed BG/OBJ classification and an OAM byte-write mutant.
- `cmake --workflow --preset ci` — canonical workflow passed. The inventory contained 282 tests; 280 ran and passed, with the two existing owner-gated signature tests intentionally skipped. The inventory JUnit check passed.

## Next Phase Readiness

The bus decode and video behavior are ready for CPU integration. Unsupported I/O devices, BIOS protection, CPU rotation and sequencing, prefetch, and backup devices remain outside this slice. `BUS-02` and `BUS-03` stay **Pending** in global requirements until integrated phase evidence and final verification support completion. ROM-tail hardware behavior remains explicitly unverified.

---
*Phase: 03-bus-api-memory-map-and-rom-loading*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary file exists at the planned path.
- All six task commits are ancestors of the current phase branch head.
- `git diff --check` passed; the user's existing AGENTS, config, state, and research-cache changes remain unstaged.
