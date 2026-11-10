---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 03
subsystem: testing
tags: [cmake, string-json, agb-profile, timing-provenance, unmodeled-list, doc-sync, bios-provider-grammar, negative-controls]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: strict JSON loader, gbl_contract_read, mutation helpers (02-01); dispute registry and gbl_registry_read (02-02)
provides:
  - profiles/agb.json, the versioned agb profile (authority) with eleven source-labeled timing constants and nine unmodeled entries
  - docs/profile-agb.md, the narrative document with a synced `## Unmodeled` table and the SP/AGS naming position
  - cmake/CheckProfile.cmake validator (structure, labels, arithmetic, dispute cross-reference, doc sync)
  - gbl_bios_provider_valid in cmake/CheckContract.cmake (the R1 grammar, reused by the result validator in plan 06)
  - tests/gates/profile_case.cmake driver and 22 registered profile tests
affects: [02-04, 02-06, 02-07, phase-04-runner-emission]

actuals:
  tokens: 11300
  tasks: 3
  commits: 3
plan_head_before: dc1a93187de7386cb0d1aae02db0699213fd1518
plan_head_after: 4830eca1411664675106c126fc0faafd72c3c914

tech-stack:
  added: []
  patterns:
    - "One profile validator run per authority file with a doc-sync pass: the JSON owns the ids and version, the Markdown table is parsed (file(READ), heading-delimited) and compared both ways"
    - "Invariants recomputed with math(EXPR) over operands that are available integers; one finding per broken invariant"
    - "BIOS provider grammar as whole-string anchored regexes with HEX40/HEX64 built by string(REPEAT)"
    - "Doc edits in controls go through gbl_text_mutate or an asserting helper, so a stale anchor fails with `mutate: no-match`"

key-files:
  created:
    - profiles/agb.json
    - docs/profile-agb.md
    - cmake/CheckProfile.cmake
    - tests/gates/profile_case.cmake
  modified:
    - cmake/CheckContract.cmake
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "The profile carries dots_per_line (308, REF) in addition to the RESEARCH shape, so all five timing invariants are checkable."
  - "An hblank_flag_edge_cycle entry with no `dispute` reports dispute-unknown (`no dispute`): an UNK constant must point at the registry entry that explains it. The plan catalogue did not list this case; it is the timing-side twin of the unmodeled hblank-flag-edge dispute rule."
  - "A numeric timing value of the wrong type reports missing-field; a timing value that is a number but not a plain integer reports timing-arithmetic."
  - "In docs/profile-agb.md the SP compatibility statement is labeled UNK (community claim, W8), the backlight statement REF, and the AGS-001/AGS-101 and AGB-001/OXY-001 labels SRC at MEDIUM confidence, as the plan directs; RESEARCH's REF tag on the compatibility row is corrected in the doc only."

patterns-established:
  - "Profile and vocabulary text cites GBATEK by section name only; no source prose is quoted"

requirements-completed: [PROF-01, PROF-02]

duration: 25min
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 03: agb profile and validator Summary

**A versioned `agb` profile whose eleven timing constants each carry one source label (HBlank flag edge is null and UNK, pointing at the dispute registry), nine honest unmodeled entries synced to a narrative doc, and a CMake validator with the R1 BIOS-provider grammar and a control for every reason code.**

## Performance

- **Duration:** about 25 min
- **Completed:** 2026-11-10
- **Tasks:** 3 (one tracer, two TDD-style control tasks)
- **Files:** 8 (4 created, 4 edited)

## Accomplishments

- `profiles/agb.json` (`gamebloke.profile/1`, `profile_version` 1): id and machine `agb`, boot `direct-start`, bios_provider `none`, cart `rom-only`. Timing constants: master_clock_hz 16777216, cycles_per_dot 4, dots_visible 240, dots_hblank 68, dots_per_line 308, cycles_per_line 1232, lines_visible 160, lines_vblank 68, lines_per_frame 228, cycles_per_frame 280896 (all REF with GBATEK section names), and hblank_flag_edge_cycle null / UNK / `disp-hblank-flag-edge`. The numbers 960, 1006, 1007, 1008 occur only in the hblank-flag-edge `reason` string. No `agb-micro` profile exists.
- Unmodeled entries: sp-micro-differences (details name the 0x04000800 Internal Memory Control register and the Micro's missing cartridge switch), prefetch, hblank-flag-edge, ppu, audio, dma, timers, irq-controller, backup-media.
- `docs/profile-agb.md`: authority statement, `profile_version: 1` line, sections for machine, boot, BIOS provider, cartridge, timing table, SP and AGS naming, and the `## Unmodeled` table with nine id rows. It does not state that the SP is identical to the AGB.
- `cmake -DPROFILE=profiles/agb.json -DDOC=docs/profile-agb.md -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckProfile.cmake` prints `profile: ok (agb v1, 11 timing constants, 9 unmodeled)`.
- Reason codes: usage, bad-schema, bad-version, missing-field, bad-machine, bad-boot, bad-bios-provider, bad-cart, missing-source, bad-source-label, timing-arithmetic, hblank-edge-hardcoded, unmodeled-missing, unmodeled-bad-entry, dispute-unknown, doc-no-section, doc-missing-id, doc-extra-id, doc-version-mismatch.
- `gbl_bios_provider_valid(<out> <value>)` accepts exactly `none`, `project-stub`, `hle:<name>(,<name>)*`, `replacement:<name>@<40 hex>`, `user-official@<64 hex>`; no ordering or duplicate rule inside `hle:`.

## Tests added (22)

profile.lint; profile.nc.bad_bios_provider, profile.nc.bad_boot, profile.nc.bad_cart, profile.nc.bad_machine, profile.nc.bad_schema, profile.nc.bad_source_label, profile.nc.bad_version, profile.nc.dispute_unknown, profile.nc.doc_extra_id, profile.nc.doc_missing_id, profile.nc.doc_no_section, profile.nc.doc_version_mismatch, profile.nc.hblank_edge_hardcoded, profile.nc.missing_field, profile.nc.missing_source, profile.nc.timing_arithmetic, profile.nc.unmodeled_bad_entry, profile.nc.unmodeled_missing, profile.nc.usage; profile.pos.coordinated_change, profile.pos.copy.

## Final lane counts

- ci: `inventory: expected 176, ran 174, skipped 2 (allowed 2)`
- hygiene: `inventory: expected 106, ran 104, skipped 2 (allowed 2)`
- asan: unchanged (`tests/inventory/asan.txt` untouched)
- `json-real: 5 documents` (profiles/agb.json joined the loader's real-document set through the existing glob)
- `sh scripts/hygiene.sh --tree` printed `hygiene: ok (125 files)`; `signature-scan.sh --tree` printed `signature: ok (125 files)`.
- Per-task counts matched the plan: after Task 1 ci 158 / hygiene 88 (3 profile tests); after Task 2 ci 171 / hygiene 101 (16 profile tests); after Task 3 ci 176 / hygiene 106 (22 profile tests).

## Control proofs (scratch copies, not committed)

- Task 2 mutation observation: turning the `hblank-edge-hardcoded` finding into a no-op in a scratch copy of cmake/CheckProfile.cmake made profile.nc.hblank_edge_hardcoded fail with `expect-fail: gate PASSED but must fail (negative control broken)`.
- The bad_bios_provider control logged one `expect-fail: ok` line per variant (7).
- Floor spot check: the real profile and 21 driver cases pass under the Kitware 3.25.3 binary (digest verified earlier). Plan 07 owns the full floor workflow run.

## Task Commits

1. **Task 1: Tracer: profile, doc, validator, one control and one positive** - `f3d748a` (feat)
2. **Task 2: Structure, label, arithmetic, and BIOS controls** - `50024ad` (test)
3. **Task 3: Dispute and doc-sync controls, coordinated-change positive** - `4830eca` (test)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above. Tracer gate: the Task 1 verify command (3 profile tests, ci 158, hygiene scan) was green before any expansion task started.

## Deviations from Plan

None - plan executed exactly as written. Two small additions inside the validator's stated scope are noted under key-decisions (dispute required on the hblank timing entry; wrong-type numeric timing value handling). One housekeeping note: while adding Task 2's inventory lines a blank line was briefly inserted by a helper script and removed before the commit; no committed file contains it.

**Total deviations:** 0.

## Issues Encountered

None.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model (T-02-08 mitigated by per-constant source and cite checks, the null HBlank edge, and pinned unmodeled ids; T-02-09 by the four doc-sync codes with controls including the empty-section case; T-02-10 by the anchored grammar and the seven-shape bad_bios_provider control).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-04. `gbl_bios_provider_valid` is available to the result validator (plan 06), and `profiles/agb.json` is the file the result and manifest checks cite by id and `profile_version`.

## Self-Check: PASSED

profiles/agb.json, docs/profile-agb.md, cmake/CheckProfile.cmake, and tests/gates/profile_case.cmake exist; commits f3d748a, 50024ad, and 4830eca are ancestors of HEAD; `git rev-list --count` from the recorded base measured 3; ci 176/174/2 and hygiene 106/104/2 and both tree scans passed; all acceptance criteria re-ran green.
