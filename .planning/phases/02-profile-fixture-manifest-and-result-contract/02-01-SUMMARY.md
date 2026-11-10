---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 01
subsystem: testing
tags: [cmake, string-json, validators, evidence-contract, negative-controls]

requires:
  - phase: 01-verification-harness
    provides: gbl_add_test, sorted lane inventories, ExpectFail wrapper, hygiene and signature scans
provides:
  - strict whole-file JSON loader (comment, trailing comma, trailing content, duplicate key) for every Phase 2 authority file
  - contracts/evidence.json with the nine result states as data, source labels, provenance classes, and claim fields
  - independent oracle validator pinning those sets and the locked state rules
  - mutation and no-echo helpers, two case drivers, and 22 registered gate/control tests
affects: [02-02, 02-03, 02-04, 02-05, 02-06, 02-07, phase-04-runner-emission]

actuals:
  tokens: 11000
  tasks: 3
  commits: 3
plan_head_before: 0071006f9b65d64d109e2ce90547de4584faa7b9
plan_head_after: c4ae224096442ad66b3b65ecc47b8352216036e7

tech-stack:
  added: []
  patterns:
    - "Strict loader: string(JSON) parse plus a structural depth walk, never a bracket-class regex"
    - "Findings are `<family>: <code>: <path-or-field>` only; a marker token proves no content echo"
    - "Every mutation helper fails with `mutate: no-match` when it changed nothing"
    - "Authority vocabulary as data plus an independent pinned oracle in the validator"

key-files:
  created:
    - cmake/GblJson.cmake
    - cmake/CheckContract.cmake
    - contracts/evidence.json
    - tests/gates/json_mutate.cmake
    - tests/gates/json_case.cmake
    - tests/gates/contract_case.cmake
  modified:
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "result_labels is not in contracts/evidence.json (I2): the only result label is `disputed`, and its meaning is a rule (a result carries it exactly when it discloses a registry dispute, D-12), not a vocabulary entry. A one-member list in data would let someone add a label no validator gives semantics to, so plan 06 hard-codes `disputed` as the single allowed label (`result: bad-label` otherwise) and a future label arrives with its rule and a gamebloke.result/2 bump."
  - "GUEST_STOP needs no reason (orchestrator resolution R-1); only the locked rows PASS, MISSING, SKIPPED, REFUSED are pinned in the oracle."
  - "Whitespace is removed from the string-stripped text before the root walk and region checks; this shortens the walk and keeps trailing-content, trailing-comma, and duplicate-key logic equivalent."

patterns-established:
  - "Case drivers take -DCASE/-DSRC/-DBIN, sandbox under BIN/nc/<family>_<case>, and mutate COPIES of real inputs"
  - "Internal child cases (mutate_probe, no_echo_probe) run only under their parent control and are never registered"

requirements-completed: [EVID-05, PROF-02]

duration: 45min
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 01: Strict loader, vocabulary contract, and contract validator Summary

**Strict string(JSON) loader with a structural root walk, the nine-state evidence vocabulary as data, and an oracle validator, all wired into the canonical ci and hygiene workflows with 22 controls.**

## Performance

- **Duration:** about 45 min
- **Completed:** 2026-11-10
- **Tasks:** 3 (one tracer, two TDD-style control tasks)
- **Files modified:** 9 (6 created, 3 edited)

## Accomplishments

- `cmake -DFILE=<path> -P cmake/GblJson.cmake` loads a JSON file and rejects absent, empty, whitespace-only, non-object, commented, trailing-comma, trailing-content, and duplicate-key input with a stable `json: <code>` line; findings never carry file content or the parser error text.
- `contracts/evidence.json` holds the nine states with `ran`/`digest`/`reason` rules (and `refusal` on REFUSED), the five source labels, five provenance classes, and six claim fields. `cmake/CheckContract.cmake` pins the same sets and the locked rules, so weakening "a fixture that did not run is never PASS" in the data fails `contract.lint` with `contract: bad-state-rule`.
- Every json and contract reason code, plus `mutate: no-match` and `no-echo: leaked`, has a control that fails for exactly its reason; the loader is proven on every real committed JSON document in both directions.

## Tests added (22)

- contract.lint
- contract.nc.bad_schema, contract.nc.bad_state_rule, contract.nc.claim_set, contract.nc.class_set, contract.nc.label_set, contract.nc.missing_field, contract.nc.state_set, contract.nc.usage
- contract.pos.copy
- json.nc.comment, json.nc.duplicate_key, json.nc.mutate_no_match, json.nc.no_echo_guard, json.nc.not_json, json.nc.not_object, json.nc.trailing_comma, json.nc.trailing_content, json.nc.unreadable, json.nc.usage
- json.pos.real_docs, json.pos.strict_clean

## Final lane counts

- ci: `inventory: expected 142, ran 140, skipped 2 (allowed 2)`; `ctest -N` total 143
- hygiene: `inventory: expected 72, ran 70, skipped 2 (allowed 2)`; `ctest -N` total 73
- asan: unchanged at 16/16/0 (`tests/inventory/asan.txt` untouched). The asan preset could not be configured on this host because `clang-21` is not installed, so the lane was not re-run locally; this is an environment gap unrelated to the plan, and the Linux lane covers it.
- `json-real: 3 documents` (contracts/evidence.json, CMakePresets.json, .github/rulesets/main.json)
- `sh scripts/hygiene.sh --tree` printed `hygiene: ok (116 files)`; `signature-scan.sh --tree` printed `signature: ok (116 files)`.

## Control proofs (scratch copies, not committed)

- Neutralizing the duplicate-key comparison in a scratch copy of cmake/GblJson.cmake made json.nc.duplicate_key fail with `expect-fail: gate PASSED but must fail`.
- Replacing the root walk with the RESEARCH Pattern 1 trailing-content regex in a scratch copy made json.nc.trailing_content fail on its first variant (a trailing `}`), confirming the control catches the under-counting regex.
- json.nc.trailing_content logs 28 `expect-fail: ok: /json: trailing-content/` lines (7 tails x the tiny base plus three real documents) and 8 `no-echo: ok` lines.
- Bonus floor spot check: all 12 json cases and 9 contract cases also pass under a Kitware 3.25.3 binary (SHA-256 verified earlier as 771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e). Plan 07 still owns the full `--workflow --preset ci` floor run.

## Task Commits

1. **Task 1: Tracer: strict loader, vocabulary file, contract validator** - `4a8f6cd` (feat)
2. **Task 2: Loader controls (json.*)** - `d1c85e9` (test)
3. **Task 3: Contract controls and full workflows** - `c4ae224` (test)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above, in particular the I2 note on omitting `result_labels`.

## Deviations from Plan

None - plan executed exactly as written. Two minor implementation choices stayed inside the plan's latitude: the GUEST_STOP-removal, tenth-row, and case-variant `state_set` mutations use `gbl_text_mutate` (the plan allowed SET of a rebuilt array), and `gbl_text_mutate` also fails with `mutate: no-match` when find equals replace.

**Total deviations:** 0.

## Issues Encountered

- A first scratch experiment for the Pattern 1 observation used a perl substitution that mangled the scratch copy; it was redone with a clean Python replacement in a scratch directory. No tracked file was affected.
- The asan lane cannot be configured locally (no `clang-21`); see lane counts above.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths, or trust-boundary file access beyond the plan's threat model (T-02-01 through T-02-04 are mitigated and controlled).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-02 (dispute registry): `gbl_json_load`, the typed accessors, `gbl_finding`/`gbl_findings_finish`, the mutation helpers, and the glob-based `json.pos.real_docs` (4 documents after 02-02) are in place. The tests/gates.cmake evidence section and both inventories take later plans' lines in byte order.

## Self-Check: PASSED

All six created files exist, the three task commits are ancestors of HEAD, and the plan-level verification (ci 142/140/2, hygiene 72/70/2, clean tree scans) passed.
