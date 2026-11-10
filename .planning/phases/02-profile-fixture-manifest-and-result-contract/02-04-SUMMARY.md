---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 04
subsystem: testing
tags: [cmake, string-json, fixture-manifest, pins, provenance, negative-controls, manifest-lint]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: strict JSON loader, vocabulary reader, mutation helpers (02-01); dispute registry and gbl_registry_read (02-02); agb profile and validator patterns (02-03)
provides:
  - tests/fixtures/manifest.json, the real fixture manifest with zero rows (R-4)
  - pins.txt, the pinned-fetch list with its required format header and no rows
  - contracts/examples/manifest.json, one complete illustrative row (mutation base for controls and the result validator)
  - cmake/CheckManifest.cmake with gbl_manifest_read, gbl_pins_read, and MODE=manifest|pins|all
  - manifest.lint registered in tests/gates.cmake, so it runs inside `cmake --workflow --preset ci`
  - tests/gates/manifest_case.cmake and tests/gates/pins_case.cmake drivers with 30 registered tests
affects: [02-05, 02-06, 02-07, phase-05-fixture-rows, phase-06-fixture-rows]

actuals:
  tokens: 14100
  tasks: 3
  commits: 3
plan_head_before: ee2f6c336d588ff5f11104bc9604aa154ddd75f0
plan_head_after: 4dc94a644b721bb8299ab8bebad5f39bfee41a6c

tech-stack:
  added: []
  patterns:
    - "Row checks in one helper that reports through _gbl_row_* variables; the reader publishes per-row <prefix>_ROW_<id>_* variables only for a well-formed, unique id"
    - "Pins are scanned line by line with string(FIND) on newlines and field-split with one anchored regex, never through list handling, so a malformed row cannot split on a semicolon or bracket"
    - "MODE=all reads vocabulary, registry, manifest, and pins, then cross-checks fetch.pin_dest against pins.txt"
    - "Control variants are generated from field-path lists (remove, null, empty per path) so every required field has a rejecting control"

key-files:
  created:
    - tests/fixtures/manifest.json
    - pins.txt
    - contracts/examples/manifest.json
    - cmake/CheckManifest.cmake
    - tests/gates/manifest_case.cmake
    - tests/gates/pins_case.cmake
  modified:
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "Field names follow the plan and RESEARCH shape (source.kind/url/commit/path/sha256, license.*, build.recipe/toolchain.*, bios_requirement, expected.provenance_class/dispute, permitted_use, optional fetch.pin_dest). Renaming later needs a gamebloke.manifest/2 bump (costly reversibility, A6)."
  - "pins: bad-url applies the same URL rule as manifest: bad-url (https:// only, no whitespace, quote, or backslash). The plan's control for a pins URL holding a double quote needs this; such a row reports bad-row and bad-url together."
  - "A pins row with a forbidden character still goes through field checks when it splits into three fields, so the targeted field code is reported next to bad-row."
  - "A non-string license.redistributable (boolean true) is bad-redistributable, not wrong-type, as the plan's code catalogue states; an empty string there is missing-field."
  - "An in-tree row with a fetch member is pin-mismatch in MODE=all (it has no fetchable source)."

patterns-established:
  - "Example rows use reserved example domains (example.invalid) so no row implies a real fetch"
  - "Synthesized digests in controls come from string(REPEAT) of a hex fragment"

requirements-completed: [EVID-01, EVID-02, VERIF-01]

duration: 35min
completed: 2026-11-10
status: complete

coverage:
  - id: D1
    description: "A manifest row declares id, source, license, build, bios_requirement, expected, and permitted_use; a complete row is accepted and each missing or null field is rejected with `manifest: missing-field`"
    requirement: "EVID-01"
    verification:
      - kind: unit
        ref: "ctest -R '^manifest\\.(pos\\.complete_row|nc\\.missing_field)$' (hygiene and ci lanes)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Unpinned git revisions (branch, HEAD, short SHA, uppercase hex, tag only), unknown provenance classes, and dispute and class mismatches are rejected with stated reason codes"
    requirement: "EVID-02"
    verification:
      - kind: unit
        ref: "ctest -R '^manifest\\.nc\\.(unpinned_revision|bad_provenance_class|dispute_unknown|dispute_class_mismatch)$'"
        status: pass
    human_judgment: false
  - id: D3
    description: "pins.txt is linted (required header, https only, digest, dest, duplicate dest) and cross-checked against manifest rows in MODE=all"
    requirement: "EVID-02"
    verification:
      - kind: unit
        ref: "ctest -R '^(pins\\.|manifest\\.nc\\.pin_mismatch)'"
        status: pass
    human_judgment: false
  - id: D4
    description: "manifest.lint runs inside `cmake --workflow --preset ci` over the real manifest, pins file, registry, and vocabulary"
    requirement: "VERIF-01"
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci (inventory: expected 206, ran 204, skipped 2 (allowed 2); manifest.lint Passed)"
        status: pass
    human_judgment: false
---

# Phase 2 Plan 04: Fixture manifest and pins linter Summary

**A manifest and pins linter (`cmake/CheckManifest.cmake`) with registry and pin cross-checks, an empty real manifest, a header-only `pins.txt`, one committed complete example row, and `manifest.lint` running in the canonical ci command with a control for every manifest and pins reason code.**

## Performance

- **Duration:** about 35 min
- **Completed:** 2026-11-10
- **Tasks:** 3 (one tracer, two TDD-style control tasks)
- **Files:** 9 (6 created, 3 edited)

## Accomplishments

- `tests/fixtures/manifest.json` is `gamebloke.manifest/1` with `"fixtures": []`; `pins.txt` is the required header line plus comment lines (single raw file at a 40-hex commit or a release asset, https only, never a generated archive). `contracts/examples/manifest.json` holds one complete git row `example-row` (reserved `example.invalid` URL, class `suite-self-checking`, dispute null, toolchain digest null, `permitted_use` `local-only`).
- `gbl_manifest_read` reports bad-schema, missing-field, wrong-type, bad-id, duplicate-id, bad-source-kind, unpinned-revision, missing-digest, bad-url, bad-path, bad-provenance-class, bad-bios-requirement, bad-permitted-use, bad-redistributable, dispute-unknown, and dispute-class-mismatch, and sets `<prefix>_IDS`, `<prefix>_COUNT`, and the per-row `_CLASS|_DISPUTE|_KIND|_COMMIT|_SHA256|_PIN_DEST` variables. `gbl_pins_read` reports missing-file, missing-header (checked first; a failing file reports no row findings), bad-row, bad-digest, bad-url, bad-dest, and duplicate-dest.
- `MODE=all` adds `pin-mismatch` (pin_dest absent from pins, url digest differs from its pin, git pinned URL lacks the commit, in-tree row with `fetch`). Success lines: `manifest: ok (0 rows)` and `pins: ok (0 rows)` for the real files; `manifest: ok (1 rows)` for the example copy; `manifest: ok (3 rows)` plus `pins: ok (1 rows)` for a git, url, and in-tree set.
- `manifest.lint` is registered once, at the Phase 1 extension point in `tests/gates.cmake`, with `${CMAKE_SOURCE_DIR}/` prefixed file arguments, and passes inside both full workflows.

## Tests added (30)

manifest.lint; manifest.nc.{bad_bios_requirement, bad_id, bad_path, bad_permitted_use, bad_provenance_class, bad_redistributable, bad_schema, bad_source_kind, bad_url, dispute_class_mismatch, dispute_unknown, duplicate_id, missing_digest, missing_field, pin_mismatch, unpinned_revision, usage, wrong_type}; manifest.pos.{complete_row, empty, source_kinds}; pins.nc.{bad_dest, bad_digest, bad_row, bad_url, duplicate_dest, missing_file, missing_header}; pins.pos.rows.

## Final lane counts

- ci: `inventory: expected 206, ran 204, skipped 2 (allowed 2)`
- hygiene: `inventory: expected 136, ran 134, skipped 2 (allowed 2)`
- asan: unchanged (`tests/inventory/asan.txt` untouched)
- `json-real: 7 documents` (tests/fixtures/manifest.json and contracts/examples/manifest.json joined the loader's real-document set through the existing globs)
- `sh scripts/hygiene.sh --tree` printed `hygiene: ok (132 files)`; `signature-scan.sh --tree` printed `signature: ok (132 files)`.
- Per-task `ctest -N` totals matched the plan: after Task 1 ci 180 and 3 manifest tests; after Task 2 ci 199 and 22 manifest tests; after Task 3 the 8 pins tests and the final inventory lines above. (`ctest -N` reports one test more than the inventory's `expected`, which is why 207 total matches `expected 206`.)
- Counts in the acceptance criteria: `grep -c '^manifest\.' tests/inventory/ci.txt` is 22; `grep -c '^pins\.' tests/inventory/ci.txt` is 8.

## Control proofs (scratch copies, not committed)

- Task 2 mutation observation: removing the provenance-class membership check from a scratch copy of `cmake/CheckManifest.cmake` (the row's class accepted without comparison) made `manifest.nc.bad_provenance_class` fail with `expect-fail: gate PASSED but must fail (negative control broken)` and `manifest: ok (1 rows)`.
- The `manifest.nc.unpinned_revision` log carries six `expect-fail: ok: /manifest: unpinned-revision/` lines (main, HEAD, master, a 7-hex SHA, uppercase 40-hex, and a tag-only row).
- The `manifest.nc.missing_field` log carries 48 `expect-fail: ok: /manifest: missing-field/` lines: 18 removals (one per field path, including `expected.dispute`), 16 nulls, 11 empty strings, the `license.notices: []` variant, an empty `fetch` object, and a removed `fixtures`.
- Floor spot check: all 21 manifest driver cases, the 8 pins driver cases, and the real-file `MODE=all` lint pass under the Kitware 3.25.3 binary (digest verified earlier). Plan 07 owns the full floor workflow run.

## Task Commits

1. **Task 1: Tracer: manifest, pins, example row, linter, manifest.lint, one control and one positive** - `8386374` (feat)
2. **Task 2: Manifest controls for every reason code, empty and source-kind positives** - `2074907` (test)
3. **Task 3: Pins controls for every reason code including the required header** - `4dc94a6` (test)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above. Tracer gate: the Task 1 verify command (3 manifest tests, ci 180, `manifest.lint` registered) was green before any expansion task started.

## Deviations from Plan

None - plan executed exactly as written. Extra control variants beyond the plan's behavior lists (a `fixtures` object removed, an empty `fetch` object, a non-string notices item, a string `source`, and an in-tree-with-fetch `pin_dest` case) were added inside the stated reason codes; they add coverage and change no planned behavior.

**Total deviations:** 0.

## Issues Encountered

None. A first draft of the Task 2 driver wrote doubled backslashes in quoted JSON snippets (an escaping slip in the generating script); it was corrected before the first run, and no committed file contains it.

## Known Stubs

None. `tests/fixtures/manifest.json` has zero rows and `pins.txt` has no rows by design (R-4, D-15); Phases 5 and 6 add the first rows, and `manifest.pos.complete_row` and `manifest.pos.source_kinds` prove a populated manifest is accepted.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model. T-02-11 is mitigated by the 40-lowercase-hex commit rule and the six-shape control; T-02-12 by https-only URLs in both linters with http, file, ssh, and quote controls; T-02-13 by the allow-list charset and per-component rule with traversal, absolute, and double-slash controls; T-02-14 by the printed row counts, the populated positives, and `pins: missing-header`; T-02-15 by both dispute-class-mismatch directions and dispute-unknown; T-02-16 by `pin-mismatch` on a git row whose pinned URL lacks its commit.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-05. `contracts/examples/manifest.json` is the committed mutation base for the result validator (plan 06), and `gbl_manifest_read` and `gbl_pins_read` are available to later gates. The pin-dest path rule is repeated at fetch time by plan 05.

## Self-Check: PASSED

tests/fixtures/manifest.json, pins.txt, contracts/examples/manifest.json, cmake/CheckManifest.cmake, tests/gates/manifest_case.cmake, and tests/gates/pins_case.cmake exist; commits 8386374, 2074907, and 4dc94a6 are ancestors of HEAD; `git rev-list --count` from the recorded base measured 3; ci 206/204/2 and hygiene 136/134/2 and both tree scans passed; all acceptance criteria re-ran green.
