---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 07
subsystem: docs
tags: [docs, evidence-contract, reason-codes, cmake-floor, linux-lane, validation-signoff]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: every validator, gate, control, and authority file from plans 01 to 06
provides:
  - docs/evidence-contract.md, the contract guide for profile, manifest, pins, fetch, results, and disputes
  - docs/verify.md reason codes for every Phase 2 family, an Adding a fixture section, and the filled manifest.lint extension point
  - 02-EVIDENCE.md with the CMake 3.25.3 floor run, the Linux container run, timings, and the publication-time pending items
  - 02-VALIDATION.md signed off for the automated rows
affects: [phase-03, phase-04-runner-result-emission, phase-05, phase-06, orchestrator-publication]

requirements-completed: [PROF-01, PROF-02, EVID-01, EVID-02, EVID-03, EVID-05, EVID-06]

actuals:
  tokens: 9500
  tasks: 3
  commits: 3
plan_head_before: 7169bf9b5f5efe84c260758979876975645c944b
plan_head_after: f0f9938b76d4a4c4157b872780f4da7c6d29fbeb

tech-stack:
  added: []
  patterns:
    - "Reason-code lists in docs/verify.md are checked against the validator header comments, which stay the authority"
    - "Floor and Linux proofs are recorded in an evidence file in the 01-EVIDENCE.md style, with no local paths and with unproven items listed as pending with an owner"

key-files:
  created:
    - docs/evidence-contract.md
    - .planning/phases/02-profile-fixture-manifest-and-result-contract/02-EVIDENCE.md
  modified:
    - docs/verify.md
    - CMakeLists.txt
    - .planning/phases/02-profile-fixture-manifest-and-result-contract/02-VALIDATION.md

key-decisions:
  - "pins.txt has no separate gate: it is linted by manifest.lint (MODE=all), so the contract guide names that gate rather than inventing a pins.lint."
  - "The floor run reused a Kitware tarball already present outside the repository, re-hashed it against the published list, and extracted it fresh before running, so the verified bytes are the ones that ran."
  - "No workflow is near the 90-second flag (ci 34 s, hygiene 25 s), so no reduction step was applied and ci.yml was not touched; the escalation stays with the orchestrator for hosted-runner timing."

patterns-established:
  - "Pending-at-publication section in the evidence file: items that only a hosted run can answer (live CI, RESEARCH A1) are listed there with the orchestrator as owner, never ticked"

duration: about 25 min (start time not recorded; estimate)
completed: 2026-11-10
status: complete

coverage:
  - id: D1
    description: "docs/evidence-contract.md explains every authority file with its schema id, the claim tuple, the R1 grammar, the nine states, result rules with dispute disclosure, manifest row fields mapped to EVID-01, the pins row format, fetch-pin usage and exit codes, and states that a matching hash proves identity, not redistribution rights"
    requirement: "EVID-01"
    verification:
      - kind: command
        ref: "grep -q 'gamebloke.result/1' docs/evidence-contract.md && grep -q 'redistribution rights' docs/evidence-contract.md; grep -c '^## ' docs/evidence-contract.md prints 9"
        status: pass
    human_judgment: false
  - id: D2
    description: "docs/verify.md lists the reason codes of every Phase 2 family and the test-helper codes, the manifest.lint extension point, the Adding a fixture steps, and the hygiene lane note; every code in the eight validator headers and fetch-pin.sh appears"
    requirement: "EVID-02"
    verification:
      - kind: command
        ref: "Task 1 automated loop over representative codes, plus a header-to-doc diff of every code (no missing codes); sh scripts/hygiene.sh --dir docs; hygiene.tree and signature.tree"
        status: pass
    human_judgment: false
  - id: D3
    description: "The full suite passes on the CMake 3.25.3 floor and in the Linux container with the same ci summary line"
    requirement: "VERIF-01"
    verification:
      - kind: integration
        ref: "<cmake 3.25.3> --workflow --preset ci: exit 0, inventory: expected 252, ran 250, skipped 2 (allowed 2); sh scripts/linux-lane.sh ci gcc: exit 0, same line (see 02-EVIDENCE.md)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Default-toolchain workflows pass with final counts and validation is signed off for the automated rows"
    requirement: "VERIF-01"
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci (252/250/2) and --preset hygiene (182/180/2); check-traps, hygiene --tree, signature-scan --tree, git ls-files .fixtures empty, json-real: 8 documents"
        status: pass
    human_judgment: false
  - id: D5
    description: "Live CI of the phase PR on hosted runners and RESEARCH A1 (curl present on the ubuntu-26.04 runner)"
    requirement: "VERIF-01"
    verification: []
    human_judgment: true
    rationale: "Only a hosted run of the phase PR can show either; both are listed pending in 02-EVIDENCE.md and 02-VALIDATION.md with the orchestrator as owner."
---

# Phase 2 Plan 07: Close-out Summary

**The Phase 2 contracts are documented in a new `docs/evidence-contract.md` and a reason-code section covering every validator family in `docs/verify.md`, and the finished phase is proven on a Kitware CMake 3.25.3 floor binary (SHA-256 `771548ed...234e`, verified against Kitware's list), in the Linux container, and on the default toolchain, with `02-VALIDATION.md` signed off for what was verified automatically.**

## Performance

- **Duration:** about 25 min (the start time was not recorded; this is an estimate)
- **Completed:** 2026-11-10
- **Tasks:** 3
- **Files:** 5 (2 created, 3 edited)

## Accomplishments

- `docs/evidence-contract.md` (9 sections) covers the authority-file table with schema ids and checking gates, the six-field claim tuple, the five R1 BIOS provider forms, the nine-state rule table ("a fixture that did not run is never PASS"), result documents with dispute disclosure and `limits`, the registry and its six seeds, manifest rows mapped to the EVID-01 items, pins and `scripts/fetch-pin.sh` usage with exit codes, and the closing statement that a matching hash proves identity, not redistribution rights. It links `docs/profile-agb.md` and its sync check.
- `docs/verify.md` gains one bullet per family (`json`, `contract`, `registry`, `profile`, `manifest`, `pins`, `fetch-pin`, `fixture-dir`, `result`) plus the helper codes `mutate: no-match` and `no-echo: leaked`, the loader's trailing-content and header-line rules, an "Adding a fixture" section, the hygiene lane note, and the replaced "Extension points" paragraph. I diffed every code printed by the eight validator headers and `scripts/fetch-pin.sh` against the doc: no missing codes (the only names the diff flagged were field names in the header tables, not reason codes).
- `CMakeLists.txt` changed comment lines only (every changed line starts with `#`).
- `02-EVIDENCE.md` records the floor run, Linux container run, timings, and pending items.
- `02-VALIDATION.md` is `status: validated`, `nyquist_compliant: true`, `wave_0_complete: true`; per-task rows are green with their test families; the Manual-Only table keeps live CI and A1 pending (owner: orchestrator) and records the floor and Linux rows as done.

## Final lane counts

- ci: `inventory: expected 252, ran 250, skipped 2 (allowed 2)` (default CMake, floor CMake 3.25.3, and the Linux container all print this line)
- hygiene: `inventory: expected 182, ran 180, skipped 2 (allowed 2)`
- asan: unchanged, `inventory: expected 16, ran 16, skipped 0 (allowed 0)` (`git diff --quiet 3ea10ec -- tests/inventory/asan.txt` exits 0; the lane was not re-run in this plan)
- Phase total of 132 new tests by family: json 12, contract 10, registry 12, profile 22, manifest 22, pins 8, fetch 19, result 27 (counted from `tests/inventory/ci.txt`).
- `json.pos.real_docs` reports `json-real: 8 documents`.

## Floor and Linux results

- **CMake 3.25.3 floor:** pass. `<cmake 3.25.3> --workflow --preset ci` after `rm -rf build/ci`: exit 0, `100% tests passed, 0 tests failed out of 252`, the 252/250/2 line, 31 s. `json.nc.trailing_content`, `json.pos.real_docs`, `json.nc.no_echo_guard`, and `json.pos.strict_clean` all passed on the floor, so the B1 guard and the I1 parse-success test hold on 3.25.3. No source fix was needed. The binary came from the official tarball, re-hashed against `cmake-3.25.3-SHA-256.txt` and freshly extracted outside the repository; no PyPI cmake.
- **Linux container:** pass. `sh scripts/linux-lane.sh ci gcc` on an aarch64 Docker host (native, not emulated), CMake 4.2.3, GCC 15.2.0: exit 0, the same summary line, about 54 s. The fetch controls ran in the container with curl installed by the lane's apt line. The x86_64 proof is the hosted CI run.
- **Timings (I5):** default-CMake ci workflow 34 s, hygiene workflow 25 s, both far below the 90 s flag and the 3-minute job timeouts. Slowest Phase 2 tests: `manifest.nc.missing_field` 1.78 s, `json.nc.trailing_content` 1.04 s, `result.nc.missing_field` 0.70 s, `profile.nc.missing_field` 0.49 s, `profile.nc.unmodeled_missing` 0.44 s. `ci.yml` was not edited; the orchestrator may still raise `timeout-minutes` in the phase PR if hosted-runner times need it.

## Task Commits

1. **Task 1: Contract guide and verify-guide updates for every Phase 2 family** - `99585b0` (docs)
2. **Task 2: CMake 3.25.3 floor run, Linux container run, and timings recorded as evidence** - `0d13f3e` (docs)
3. **Task 3: Final full workflows and validation sign-off** - `f0f9938` (docs)

## Deviations from Plan

None - plan executed exactly as written.

## Pending at publication (owner: orchestrator)

- Live CI run of the phase PR (all lanes green, `CI required` success): not claimed; needs a hosted run.
- RESEARCH A1: curl on the GitHub `ubuntu-26.04` runner: not claimed; the Linux container run installs curl itself and does not answer it.
- Neither the floor run nor the Linux proof is pending; both ran and are recorded in `02-EVIDENCE.md`.
- VERIF-01 is not marked complete here; the orchestrator reconciles it with the verifier.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path, or trust-boundary file access was added; the only download (the Kitware tarball) was SHA-256 verified and used locally outside the repository.

## Self-Check: PASSED
