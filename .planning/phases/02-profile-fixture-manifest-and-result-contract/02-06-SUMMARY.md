---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 06
subsystem: testing
tags: [cmake-script, json-validation, result-contract, claim-tuple, bios-provider, dispute-disclosure, negative-controls]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: GblJson loader, contracts/evidence.json and gbl_contract_read/gbl_bios_provider_valid (01, 03), registry (02), manifest reader (04), mutation and ExpectFail helpers
provides:
  - cmake/CheckResult.cmake, the gamebloke.result/1 validator (claim tuple, per-state rules from the vocabulary, R1 BIOS grammar, dispute disclosure, limits)
  - contracts/examples/result-pass.json, an illustrative PASS for the example manifest row, checked in the canonical command as result.example
  - tests/gates/result_case.cmake driver with 27 registered tests
affects: [02-07, phase-04-runner-result-emission]

requirements-completed: [PROF-02, EVID-05, EVID-06]

actuals:
  tokens: 10600
  tasks: 3
  commits: 3
plan_head_before: f65fc9a5c8b6718db30feca3050bfc5a6e081c92
plan_head_after: 561d7ce47c131ed3151b03fc4a60449d587fa9c7

tech-stack:
  added: []
  patterns:
    - "State rules are read from the vocabulary rows (gbl_contract_read); the only state name spelled in CheckResult.cmake is PASS, for its two named reason codes"
    - "Controls mutate a sandbox copy of the committed example result (and, for dispute cases, a sandbox copy of the example manifest) and assert a non-zero exit plus the reason code"

key-files:
  created:
    - cmake/CheckResult.cmake
    - contracts/examples/result-pass.json
    - tests/gates/result_case.cmake
  modified:
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "`disputed` is the one hard-coded label in CheckResult.cmake (I2, see 02-01): its meaning is the disclosure rule, so contracts/evidence.json carries no result_labels list; a new label would arrive with its rule and a gamebloke.result/2 bump."
  - "ROADMAP SC5 is delivered at validator level in Phase 2: a result pointing at a dispute entry must list the id and carry the disputed label or it fails with undisclosed-dispute. The producer half (the runner emitting a result labeled disputed for such a fixture) moves to Phase 4, when the runner emits results."
  - "Non-array disputes or labels, and a null claim field, are reported as missing-field rather than as new reason codes, so the documented code set stays exactly the plan's list."

patterns-established:
  - "gbl__result_digest classifies a digest slot as ABSENT, NULL, HEX, TEXT, or OTHER once, and the state rules, the claim cart check, and the claim-mismatch comparison all read that classification"

duration: about 20 min (start time not recorded; estimate)
completed: 2026-11-10
status: complete

coverage:
  - id: D1
    description: "A result document is checked for its claim tuple (machine, boot, BIOS provider, cart, input timeline, evidence) with the BIOS provider restricted to the five R1 forms; M1, an empty hle list, a branch-named replacement, a short user-official digest, and a trailing space are rejected"
    requirement: "PROF-02"
    verification:
      - kind: unit
        ref: "ctest -R '^result\\.(nc\\.(bad_bios_provider|bad_machine|bad_boot|bad_cart|bad_input_timeline|bad_evidence|claim_mismatch|missing_field)|pos\\.bios_provider_forms)$'"
        status: pass
    human_judgment: false
  - id: D2
    description: "A fixture that did not run is never PASS: PASS without a run, PASS without a 64-hex digest, a ran value that contradicts the state row, an unknown status, a missing reason, and a missing refusal code or postcondition each fail with their own reason code; all nine states have an accepted minimal document"
    requirement: "EVID-05"
    verification:
      - kind: unit
        ref: "ctest -R '^result\\.(nc\\.(pass_without_run|pass_without_digest|ran_state_mismatch|missing_digest|unknown_status|missing_reason|missing_refusal)|pos\\.all_states)$'"
        status: pass
    human_judgment: false
  - id: D3
    description: "Validator-level rule for ROADMAP SC5: a result whose fixture or evidence points at a manifest row with a registry dispute must list the dispute id and carry the disputed label (undisclosed-dispute, label-without-dispute, unknown-dispute), and the honest form is accepted"
    requirement: "EVID-06"
    verification:
      - kind: unit
        ref: "ctest -R '^result\\.(nc\\.(undisclosed_dispute|label_without_dispute|unknown_dispute|unknown_fixture|bad_label)|pos\\.disputed)$'"
        status: pass
    human_judgment: false
  - id: D4
    description: "The committed illustrative PASS result validates against the example manifest, the real registry, and the real vocabulary inside the canonical command"
    requirement: "PROF-02"
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci (inventory: expected 252, ran 250, skipped 2 (allowed 2)); result.example Passed"
        status: pass
    human_judgment: false
  - id: D5
    description: "The producer half of SC5: the runner emits a result labeled disputed for a fixture whose expected value points at a dispute entry"
    requirement: "EVID-06"
    verification: []
    human_judgment: true
    rationale: "Not built in Phase 2: no runner emits results yet. Phase 4 owns emission; this plan delivers only the validator rule that such a result must be labeled."
---

# Phase 2 Plan 06: Result contract validator Summary

**A `cmake/CheckResult.cmake` validator for `gamebloke.result/1` documents that takes its per-state rules from `contracts/evidence.json`, checks the claim tuple with the R1 BIOS grammar, and refuses a result that points at a disputed manifest row without listing the dispute and carrying the `disputed` label, backed by 27 registered tests and an illustrative PASS example checked in the canonical command.**

## Performance

- **Duration:** about 20 min (the start time was not recorded; this is an estimate)
- **Completed:** 2026-11-10
- **Tasks:** 3 (one tracer, two TDD-style control tasks)
- **Files:** 6 (3 created, 3 edited)

## Accomplishments

- `contracts/examples/result-pass.json` is the illustrative PASS for `example-row`: fixture `{id, ran true, rom_sha256}` with a synthesized 64-hex digest, status PASS, the six-field claim, empty `disputes` and `labels`, and one `limits` string saying no fixture ran. `cmake -DRESULT=contracts/examples/result-pass.json -DMANIFEST=contracts/examples/manifest.json -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckResult.cmake` prints `result: ok (PASS)`.
- `cmake/CheckResult.cmake` implements all 22 reason codes after `usage`: `bad-schema`, `missing-field`, `unknown-status`, `pass-without-run`, `pass-without-digest`, `ran-state-mismatch`, `missing-digest`, `missing-reason`, `missing-refusal`, `bad-machine`, `bad-boot`, `bad-bios-provider`, `bad-cart`, `bad-input-timeline`, `bad-evidence`, `claim-mismatch`, `unknown-fixture`, `unknown-dispute`, `undisclosed-dispute`, `label-without-dispute`, `bad-label`, `empty-limits`. It reports every finding before finishing and ignores unknown top-level keys; `semantic` and `time` rules are left to Phase 4 and the header says so.
- State rules (ran, digest, reason, refusal) come from the vocabulary rows. `grep -E -c 'GUEST_STOP|HARNESS_FAIL|CANCELLED' cmake/CheckResult.cmake` prints 0, and the only state spelled in the file is PASS.
- Dispute disclosure reads `gbl_manifest_read`'s per-row class and dispute for the result's fixture and each evidence fixture, so a manifest row that points at a registry entry forces the result to list that id and carry `disputed`.
- `result.pos.all_states` builds a minimal accepted document for each of the nine states (ran-false rows with both digests null, reasons for the two reason states, a refusal object for the refusal state); its log carries nine `expect-pass: ok` lines and one `result: ok (<STATE>)` line per state.

## Tests added (27)

result.example; result.nc.{bad_bios_provider, bad_boot, bad_cart, bad_evidence, bad_input_timeline, bad_label, bad_machine, bad_schema, claim_mismatch, empty_limits, label_without_dispute, missing_digest, missing_field, missing_reason, missing_refusal, pass_without_digest, pass_without_run, ran_state_mismatch, undisclosed_dispute, unknown_dispute, unknown_fixture, unknown_status, usage}; result.pos.{all_states, bios_provider_forms, disputed}.

## Final lane counts

- ci: `inventory: expected 252, ran 250, skipped 2 (allowed 2)`
- hygiene: `inventory: expected 182, ran 180, skipped 2 (allowed 2)`
- asan: unchanged (`grep -c '^result\.' tests/inventory/asan.txt` prints 0); `grep -c '^result\.' tests/inventory/ci.txt` prints 27.
- Per-task `ctest -N` totals matched the plan: ci 229 after Task 1 and 247 after Task 2 (the final ci total is 253 tests, which is `expected 252` plus the one test the inventory does not count, as in 02-04 and 02-05); the hygiene result filter reported 3, 21, then 27.
- `json.pos.real_docs` reports `json-real: 8 documents` (result-pass.json joined the set). `sh scripts/hygiene.sh --tree` printed `hygiene: ok (140 files)` and `signature-scan.sh --tree` printed `signature: ok (140 files)` before this SUMMARY was added.

## Control proofs (scratch copies, not committed)

- Every result reason code was disabled in turn in a scratch copy of `CheckResult.cmake` (the finding call replaced by a no-op) and the matching control driver was rerun against that copy: the control for each of the 22 codes failed with `expect-fail: gate PASSED but must fail`. The same driver with nothing disabled passed, so the harness itself is sound.
- A finer check disabled only the manifest-row disclosure finding (`${n} is not in disputes`) and left the label rule in place: `result.nc.undisclosed_dispute` still failed, because its first variant relies on the row-driven rule alone.
- Rejecting the `disputed` label in a scratch copy made `result.pos.disputed` fail, so the positive proves the honest form is accepted rather than every dispute being refused.
- Log acceptance: `result.nc.bad_bios_provider` carries five `expect-fail: ok: /result: bad-bios-provider/` lines and `result.pos.bios_provider_forms` five `expect-pass: ok` lines; `result.nc.missing_field` carries 16 `expect-fail: ok` lines (seven top-level keys, `fixture.id`, `fixture.rom_sha256`, six claim fields, and `fixture.ran` as a string).
- Task 2 oracle observation: a scratch copy of `contracts/evidence.json` with the PASS row changed to `ran: either` made the contract validator fail with `contract: bad-state-rule: PASS: ran must be true`, so the data this validator reads is still guarded by the independent oracle (not committed).
- Floor spot check: the real example and all 26 distinct result case drivers ran green under the Kitware 3.25.3 binary (digest verified earlier). Plan 07 owns the full floor workflow run.

## Task Commits

1. **Task 1: Tracer: example PASS result and the full result validator in the ci command, with the PASS-without-run control and the all-states positive** - `9efe110` (feat)
2. **Task 2: Result controls for the state, claim, and limits codes, plus the R1 positive** - `9466007` (test)
3. **Task 3: Dispute-disclosure and reference controls, the disputed positive, and the full workflows** - `561d7ce` (test)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above. Tracer gate: the Task 1 verify command (3 result tests, ci 229) was green and re-run before any expansion task started.

## Deviations from Plan

None - plan executed exactly as written. Extra control variants sit inside the stated reason codes (for example the listed-dispute-without-label variant inside `undisclosed_dispute`, and a null-digest and a 63-hex variant inside `pass_without_digest`).

## Issues Encountered

None.

## Known Stubs

None. The example result is intentionally illustrative and says so in its `note` and `limits`; no runner emits results until Phase 4.

## Threat Flags

None. No new network endpoint, auth path, or trust-boundary access beyond the plan's threat model. T-02-25 is covered by the pass_without_run, pass_without_digest, and ran_state_mismatch controls; T-02-26 by undisclosed_dispute, label_without_dispute, unknown_dispute, and the disputed positive; T-02-27 by claim_mismatch and bad_evidence; T-02-28 by the five-variant bad_bios_provider control and the five-form positive; T-02-SC-06 because no registry package was installed.

## Pending items

- **ROADMAP SC5 producer half moves to Phase 4.** Phase 2 delivers the validator-level rule ("a result pointing at a dispute entry is labeled disputed"); the runner emitting such a result has no implementation until Phase 4 emits results.
- GUEST_STOP carries no reason requirement (R-1) and `semantic`/`time` are not validated; both arrive with Phase 4 and PROF-03.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-07. That plan documents the contract guide and the verify-guide reason codes for `result`, runs the CMake 3.25.3 floor workflow, and gathers the Linux lane evidence.

## Self-Check: PASSED

cmake/CheckResult.cmake, contracts/examples/result-pass.json, and tests/gates/result_case.cmake exist; commits 9efe110, 9466007, and 561d7ce are ancestors of HEAD; `git rev-list --count` from the recorded base measured 3; ci 252/250/2 and hygiene 182/180/2 and both tree scans passed; acceptance criteria for all three tasks re-ran green.
