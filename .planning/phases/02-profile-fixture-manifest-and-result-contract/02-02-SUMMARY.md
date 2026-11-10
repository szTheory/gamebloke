---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 02
subsystem: testing
tags: [cmake, string-json, dispute-registry, negative-controls, evidence-labels]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: strict JSON loader, gbl_contract_read, mutation helpers, contracts/evidence.json (plan 02-01)
provides:
  - registry/disputes.json with six seeded open disputes, every source labeled from the evidence vocabulary
  - cmake/CheckRegistry.cmake validator and the gbl_registry_read library function later validators include
  - tests/gates/registry_case.cmake driver and 12 registered registry tests
affects: [02-03, 02-04, 02-06, phase-04-runner-emission]

actuals:
  tokens: 9000
  tasks: 2
  commits: 2
plan_head_before: d544718e1c8c0b2db6ec3b7c8b7eaeee0fce994a
plan_head_after: 2c0e75aee5b0ece48d6a11713c1559622fcd0db7

tech-stack:
  added: []
  patterns:
    - "Library plus script-mode entry: gbl_registry_read(<prefix> <file> <vocab-prefix>) reads labels from a prior gbl_contract_read, so no vocabulary is repeated"
    - "Pinned seed ids give one seed-missing finding per absent id, so deleting a seed is never a smaller passing registry"
    - "A control per reason code mutates a copy of the real registry; the extra-entry positive proves the check is not a frozen-file comparison"

key-files:
  created:
    - registry/disputes.json
    - cmake/CheckRegistry.cmake
    - tests/gates/registry_case.cmake
  modified:
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "The errata entry lists twelve topics taken from the primary mGBA forum post, not RESEARCH's merged list: the post has separate lines for Game Boy Player rumble and cartridge variable-strength rumble, which RESEARCH folded into one PWM rumble item (eleven)."
  - "Issue reports and the errata post are labeled SRC (existence of the report only); the hardware claims inside are recorded as UNK in each note, never as REF."
  - "The #24 report is a Game Boy Player (GameCube accessory), #22 is flash-cart consoles, #27 is open_agb_firm (3DS GBA mode); each note says why the platform does not settle the hardware claim."

patterns-established:
  - "Registry text cites section, path, commit, and issue number only; summaries are in the author's own words with no quoted source prose or code"

requirements-completed: [EVID-06]

duration: 25min
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 02: Dispute registry and validator Summary

**Six seeded open disputes in registry/disputes.json, checked by a CMake validator that pins the seed ids and resolves labels from the evidence vocabulary, with a control for every reason code.**

## Performance

- **Duration:** about 25 min
- **Completed:** 2026-11-10
- **Tasks:** 2 (one tracer, one TDD-style control task)
- **Files modified:** 9 (3 created, 3 edited; no other files)

## Accomplishments

- `registry/disputes.json` (`gamebloke.disputes/1`) holds disp-dma-latch, disp-gbatek-errata, disp-hblank-flag-edge, disp-mgba-suite-22, disp-mgba-suite-24, disp-mgba-suite-27. Each has a summary, at least one source with one of REF/SRC/IMPL/INF/UNK, status `open`, and a non-empty `affects` list. The HBlank entry records 1,006 (REF), 1,007 (IMPL), and 1,008 (INF, inferred) and says 960 is the drawing time, not the flag edge.
- `cmake -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckRegistry.cmake` prints `registry: ok (6 entries)`. Reason codes: usage, bad-schema, missing-field, bad-id, duplicate-id, bad-status, no-sources, bad-source-label, seed-missing.
- `gbl_registry_read` sets `<prefix>_IDS` and `<prefix>_COUNT` for the profile, manifest, and result validators to resolve dispute ids.

## Tests added (12)

registry.lint; registry.nc.bad_id, registry.nc.bad_schema, registry.nc.bad_source_label, registry.nc.bad_status, registry.nc.duplicate_id, registry.nc.missing_field, registry.nc.no_sources, registry.nc.seed_missing, registry.nc.usage; registry.pos.copy, registry.pos.extra_entry.

## Final lane counts

- ci: `inventory: expected 154, ran 152, skipped 2 (allowed 2)`
- hygiene: `inventory: expected 84, ran 82, skipped 2 (allowed 2)`
- asan: unchanged (`tests/inventory/asan.txt` untouched; the lane cannot be configured on this host, as noted in 02-01)
- `json-real: 4 documents` (registry/disputes.json is now guarded by the loader in both directions)
- `sh scripts/hygiene.sh --tree` printed `hygiene: ok (120 files)`; `signature-scan.sh --tree` printed `signature: ok (120 files)`.

## Control proofs (scratch copies, not committed)

- Removing the seed pins from a scratch copy of cmake/CheckRegistry.cmake made registry.nc.seed_missing fail with `expect-fail: gate PASSED but must fail`.
- For each of bad-schema, missing-field, duplicate-id, bad-id, bad-status, bad-source-label, no-sources, and seed-missing, turning that finding into a no-op in a scratch copy made its control fail.
- Floor spot check: all 11 driver cases and the real registry pass under the Kitware 3.25.3 binary (digest verified earlier). Plan 07 owns the full floor workflow run.
- The duplicate-id variant also reports `seed-missing` for the overwritten seed; the control asserts only the `registry: duplicate-id` line, as the plan allows.

## Task Commits

1. **Task 1: Tracer: seeded registry and validator** - `318db55` (feat)
2. **Task 2: Registry controls for every remaining reason code** - `2c0e75a` (test)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above. The tracer gate passed: the Task 1 verify command (registry tests, 3 registry tests, 146 ci tests, hygiene scan) was green before any expansion, and the full workflows re-ran green after Task 2.

## Deviations from Plan

None - plan executed exactly as written. One note on a plan input: the errata list was enumerated from the primary forum post (twelve lines) as directed, and the entry names the twelve topics in the author's own words; RESEARCH's merged list has eleven.

**Total deviations:** 0.

## Issues Encountered

None. The forum post is now marked closed but still readable; it was fetched once to a scratch directory outside the repository and nothing from it was copied verbatim.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model (T-02-05 through T-02-07 are mitigated: seed ids pinned with a per-id control, every source labeled and hardware claims from reports recorded as UNK, registry text cites by section, path, commit, and issue number only).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-03. `gbl_registry_read` is available for the profile, manifest, and result validators; the evidence section of tests/gates.cmake and both inventories take later plans' lines in byte order.

## Self-Check: PASSED

registry/disputes.json, cmake/CheckRegistry.cmake, and tests/gates/registry_case.cmake exist; commits 318db55 and 2c0e75a are ancestors of HEAD; `git rev-list --count` from the recorded base measured 2; ci 154/152/2 and hygiene 84/82/2 and both tree scans passed.
