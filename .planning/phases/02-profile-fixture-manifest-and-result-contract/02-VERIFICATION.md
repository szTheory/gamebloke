---
phase: 02-profile-fixture-manifest-and-result-contract
verified: 2026-11-10T02:30:00Z
status: human_needed
score: 4/5 must-haves verified
covered_files:
  - ".gitignore"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-01-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-01-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-02-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-02-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-03-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-03-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-04-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-04-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-05-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-05-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-06-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-06-SUMMARY.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-07-PLAN.md"
  - ".planning/phases/02-profile-fixture-manifest-and-result-contract/02-07-SUMMARY.md"
  - "cmake/CheckContract.cmake"
  - "cmake/CheckManifest.cmake"
  - "cmake/CheckProfile.cmake"
  - "cmake/CheckRegistry.cmake"
  - "cmake/CheckResult.cmake"
  - "cmake/GblJson.cmake"
  - "contracts/evidence.json"
  - "contracts/examples/manifest.json"
  - "contracts/examples/result-pass.json"
  - "docs/evidence-contract.md"
  - "docs/profile-agb.md"
  - "docs/verify.md"
  - "pins.txt"
  - "profiles/agb.json"
  - "registry/disputes.json"
  - "scripts/fetch-pin.sh"
  - "scripts/linux-lane.sh"
  - "tests/fixtures/manifest.json"
  - "tests/gates.cmake"
  - "tests/gates/contract_case.cmake"
  - "tests/gates/fetch_case.cmake"
  - "tests/gates/json_case.cmake"
  - "tests/gates/manifest_case.cmake"
  - "tests/gates/pins_case.cmake"
  - "tests/gates/profile_case.cmake"
  - "tests/gates/registry_case.cmake"
  - "tests/gates/result_case.cmake"
  - "tests/inventory/ci.txt"
  - "tests/inventory/hygiene.txt"
covered_digest: "v4:sha256:3c0bf717712b16e4303252976bfb8d18ea05e4385d056d3d1a608999e93a2643"
behavior_unverified: 0
overrides_applied: 0
gaps: []
human_verification:
  - test: "Decide and record where the SC5 producer half lives. ROADMAP Phase 2 SC5 says a fixture whose expected value points at a registry entry 'produces a result labeled disputed'. Phase 2 delivers only the validator-level rule (a result for such a fixture is rejected unless it lists the dispute id and carries the `disputed` label). No producer exists because no runner emits results until Phase 4. Add the producer half to the Phase 4 roadmap (a success-criterion clause or a named requirement) so it is not lost."
    expected: "ROADMAP Phase 4 (or REQUIREMENTS) names 'the runner emits a result listing the dispute id and labeled disputed for a fixture whose manifest row carries a dispute', or the owner/orchestrator records an explicit acceptance that SC5 is validator-level only."
    why_human: "The carry-forward is recorded only in 02-06-SUMMARY.md and STATE.md. Phase 4's ROADMAP success criteria, requirement list (CPU-03..05, PROF-03, RUN-01..04) and REQUIREMENTS.md contain no dispute or label clause, and EVID-06 is already marked Complete. Moving a roadmap criterion across phases is a project-record decision, not something a code check can settle."
  - test: "Read the live CI run of the phase PR after publication (gh run view <id>): all lanes green and `CI required` success on the hosted runners."
    expected: "hygiene, ci, asan lanes and `CI required` all succeed; the fetch.* controls pass on the ubuntu-26.04 runner."
    why_human: "Needs a real GitHub run. The branch is local and unpushed (the ruleset on main accepts only PRs), so this cannot exist before publication. Owner: orchestrator at publication."
  - test: "RESEARCH A1: confirm the GitHub ubuntu-26.04 runner image has curl (first hosted run of the phase PR)."
    expected: "fetch.* controls run and pass. If curl is absent, add one `apt-get install curl` line to .github/workflows/ci.yml in the phase PR."
    why_human: "Runner image contents cannot be observed locally. The Linux container lane installs curl itself, so it does not answer A1. Owner: orchestrator."
---

# Phase 2: Profile, Fixture Manifest, and Result Contract Verification Report

**Phase Goal:** Results and fixtures have a defined, machine-checked vocabulary: a versioned `agb` profile, a claim tuple on every result, manifest rows with provenance, a SHA-256 pinned fetch, a nine-state result taxonomy, and a dispute registry. Nothing can claim more than its evidence supports.
**Verified:** 2026-11-10
**Status:** human_needed
**Re-verification:** No, initial verification

No code gap was found. The status is `human_needed` because of three items that are decisions or hosted-run evidence, not defects: one roadmap-anchoring decision (SC5 producer half) and two pending hosted-CI observations that exist only after publication.

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Versioned `agb` profile (machine, boot, BIOS provider, ROM-only cart, sourced timing constants, unmodeled list with SP/Micro, prefetch, HBlank edge); a schema check in the verify command rejects a profile missing any | VERIFIED | `profiles/agb.json` has all fields; `profile.lint` is in the ci inventory. Thirteen mutations of a scratch copy were each rejected with a stated reason: missing machine, boot, bios_provider, cart, timing, unmodeled, a timing `source`, each of the three required unmodeled ids (`sp-micro-differences`, `prefetch`, `hblank-flag-edge`), a hard-coded HBlank edge number (`hblank-edge-hardcoded`), a bad BIOS provider, and a non-`direct-start` boot. The real profile prints `profile: ok (agb v1, 11 timing constants, 9 unmodeled)`. |
| 2 | Manifest linter accepts a complete row; rejects, each with a reason, a missing field, a branch-pinned row, an unknown provenance class; registered as `manifest.lint` in `cmake --workflow --preset ci` (VERIF-01 clause) | VERIFIED | Scratch runs against `contracts/examples/manifest.json`: complete row `manifest: ok (1 rows)`; missing `license` is `missing-field`; `commit: "main"` and a branch-only row are `unpinned-revision`; class `vibes` is `bad-provenance-class`. `tests/gates.cmake` registers `manifest.lint` (MODE=all: manifest, pins, registry, vocabulary) and `tests/inventory/ci.txt` lists it. It ran inside the canonical command (below). |
| 3 | Pinned fetch by SHA-256 from `pins.txt` lands in an untracked location; digest mismatch exits non-zero loudly; `git ls-files` lists no third-party binary | VERIFIED | Scratch run with a `file://` pin under `GBL_FETCH_ALLOW_FILE=1`: matching digest gives `fetch-pin: ok`, exit 0, file at `.fixtures/ok/payload.bin`, and `git check-ignore` confirms it is ignored. Wrong digest gives `fetch-pin: digest-mismatch: ... pinned 000..., got 4f02...`, exit 1, nothing placed. Without the test variable `file://` is `bad-url`. `git ls-files` shows no `.gba/.agb/.bin/.sav/.srm/.elf/archive` file and nothing under `.fixtures`; `fetch.untracked` and `fetch.nc.tracked`/`not_ignored` are inventoried. Limit: `pins.txt` has zero rows, so the real https path has never fetched a real fixture (by design; Phase 5 and 6 add rows). |
| 4 | Result schema defines the nine states plus the claim tuple; validator rejects a BIOS provider outside R1 and a PASS for a fixture that did not run | VERIFIED | `contracts/evidence.json` holds exactly PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT, CANCELLED, MISSING, SKIPPED, REFUSED and the six claim fields. Scratch results: nine bad providers (`nintendo`, `hle:`, `hle:swi,`, `replacement:foo`, `replacement:foo@abc`, `user-official@xyz`, `USER`, empty, trailing-space `project-stub`) are `bad-bios-provider`; six valid R1 forms (`none`, `project-stub`, `hle:swi`, `hle:swi,cpuset`, `replacement:openbios@<40hex>`, `user-official@<64hex>`) are `ok`; `fixture.ran=false` with PASS is `pass-without-run`; lowercase `pass` and `WIN` are `unknown-status`; a dropped claim field is `missing-field`. Contract validator rejects a weakened PASS rule (`bad-state-rule: PASS: ran must be true`), a dropped state (`state-set`), and a semicolon in a vocabulary item (CR-01 fix holds). |
| 5 | Dispute registry holds the seeded entries; a fixture whose expected value points at a registry entry produces a result labeled disputed | VERIFIED at validator level; producer half UNCERTAIN (see Human Verification 1) | `registry/disputes.json` has six entries: `disp-dma-latch`, `disp-gbatek-errata`, `disp-hblank-flag-edge`, `disp-mgba-suite-22`, `-24`, `-27`; the validator pins those six ids (`registry.nc.seed_missing`). Scratch runs with a manifest row whose `expected.dispute` is `disp-dma-latch`: a result without the id is `undisclosed-dispute`; id without the `disputed` label is `undisclosed-dispute`; label without an id is `label-without-dispute`; an unknown id is `unknown-dispute`; id plus label is `ok`. No code emits a result: Phase 2 has no runner, so nothing yet "produces" a labeled result. The deferral to Phase 4 is real, but Phase 4's ROADMAP text does not carry it. |

**Score:** 4/5 truths fully verified; SC5 verified at the validator level only. 0 present-but-behavior-unverified. 0 overrides.

### Source-label honesty (profile and registry)

Checked against primary sources read in this session (read-only network):

- GBATEK current text: 308 dots, 1232 cycles per line, 280,896 per frame, 16*1024*1024 Hz system clock, "drawing time is only 960 cycles ... H-Blank flag is 0 for a total of 1006 cycles", EWRAM wait setting that locks up the Micro, and the Micro lacking the cartridge switch (CGB-mode section) all match. The profile's REF labels and `hblank_flag_edge_cycle: null / UNK` with a dispute id are honest. The profile carries no HBlank edge number; setting one is rejected by `hblank-edge-hardcoded`.
- mGBA suite issues #22, #24, #27 exist, are open, and their titles match the registry summaries; suite commit `e6942030d25f` resolves. All three hardware claims are labeled SRC with UNK noted, not promoted.
- mGBA 0.10.5 `video.h`: `VIDEO_HDRAW_LENGTH = 1008` matches; labeled INF for the flag-edge reading, as the entry says. IN-04's relabel of the DMA GBATEK source from REF to INF is correct for a "found no statement" inference.
- `nba-emu/hw-test` commit `fbc99140e06f` resolves on Codeberg and `dma/latch/source/main.c` matches the description (separate per-channel latch comments, 16-bit write of LSW or MSW). The NanoBoyAdvance commit `2da64afb5863` page returns 200 on Codeberg, but I could not clone to re-read the 1,007-cycle line (Codeberg clone timed out), so that single value is not independently re-verified here.
- Info: these two registry `ref` strings do not name Codeberg as the host, and the GitHub mirror does not contain those commits, so a reader following `nba-emu/...` to GitHub will not find them. Not a label dishonesty; a small provenance-clarity item.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `profiles/agb.json` | Versioned profile | VERIFIED | Exists, substantive, validated by `cmake/CheckProfile.cmake` and `docs/profile-agb.md` doc-sync. |
| `contracts/evidence.json` | Nine states, labels, classes, claim fields as data | VERIFIED | Validated by `CheckContract.cmake`; read by registry, manifest, result validators. |
| `registry/disputes.json` | Six seeded disputes | VERIFIED | Validated by `CheckRegistry.cmake`. |
| `tests/fixtures/manifest.json`, `contracts/examples/manifest.json` | Real manifest (zero rows) and complete example row | VERIFIED | Empty by design; `manifest.pos.complete_row` proves the linter is not vacuous. |
| `pins.txt` | Header-validated pins file | VERIFIED | Header and zero rows; `pins.nc.*` controls inventoried. |
| `scripts/fetch-pin.sh` | Verify-before-place fetch | VERIFIED | Behavior exercised in scratch run above. |
| `cmake/CheckManifest.cmake`, `CheckProfile.cmake`, `CheckResult.cmake`, `CheckRegistry.cmake`, `CheckContract.cmake`, `GblJson.cmake` | Validators | VERIFIED | Wired into `tests/gates.cmake` and ci inventory. |
| `docs/evidence-contract.md`, `docs/profile-agb.md`, `docs/verify.md` | Contract guide, profile doc, reason codes | VERIFIED | Present; profile doc is machine-checked against the JSON. |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| `tests/gates.cmake` `manifest.lint` | `CheckManifest.cmake` MODE=all | gbl_add_test | WIRED (ran in ci) |
| `profile.lint`, `registry.lint`, `contract.lint`, `result.example` | their validators | gbl_add_test | WIRED |
| Validators | `contracts/evidence.json` | gbl_contract_read | WIRED |
| `CheckResult.cmake` | manifest rows and registry | `gbl_manifest_read` / `gbl_registry_read` | WIRED (dispute disclosure exercised above) |
| `fetch-pin.sh` | `pins.txt`, `.gitignore` `/.fixtures/` | row lookup, git check-ignore gate | WIRED |
| Inventory | ci and hygiene lanes | `tests/inventory/{ci,hygiene}.txt` | WIRED; configure-time compare rejects drift |
| `.github/workflows/ci.yml` | `cmake --workflow --preset ci` | run step | WIRED; unchanged by this phase |

### Behavioral Spot-Checks and Canonical Command

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Canonical command | `cmake --workflow --preset ci` (CMake 4.4.3, fresh `build/ci`) | exit 0, 100% passed, `inventory: expected 263, ran 261, skipped 2 (allowed 2)`, 35 s wall | PASS |
| Hygiene lane | `cmake --workflow --preset hygiene` | exit 0, 193/191/2 | PASS |
| Static scans | `check-traps.sh`, `hygiene.sh --tree`, `signature-scan.sh --tree` | traps ok (17), hygiene ok, signature ok | PASS |
| Skips | the two skipped tests | `signature.logo.real_digest` (Phase 1 owner-pending digest) and `signature.owner_local` (`GBL_OWNER_LOGO_FILE` unset); allowed 2, visible | as claimed |
| Scratch negative/positive controls | see truths 1 to 5 | all rejections carried the expected reason code | PASS |

The CMake 3.25.3 floor run and the aarch64 Linux container lane are recorded in `02-EVIDENCE.md` and `02-REVIEW-FIX.md` at 263/261/2. I did not re-run them; they are taken as recorded evidence, labeled observed locally by the executor.

### Probe Execution

Step 7c: no probe declared by the phase plans; skipped.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| PROF-01 | 02-03, 02-07 | SATISFIED | SC1 evidence above. |
| PROF-02 | 02-01, 02-03, 02-06, 02-07 | SATISFIED | Claim tuple in vocabulary; R1 provider grammar enforced (SC4). |
| EVID-01 | 02-04, 02-07 | SATISFIED | Manifest row fields (source, pinned revision, license and notices, recipe, toolchain identity and digest, BIOS requirement, provenance class, permitted use) all required by the linter. |
| EVID-02 | 02-04, 02-07 | SATISFIED | SC2 evidence. |
| EVID-03 | 02-05, 02-07 | SATISFIED | SC3 evidence; no third-party binary tracked. Real https path untested against a real fixture until Phase 5/6. |
| EVID-05 | 02-01, 02-06, 02-07 | SATISFIED | SC4 evidence; a non-run fixture cannot be PASS. |
| EVID-06 | 02-02, 02-06, 02-07 | SATISFIED at validator level | SC5; producer half pending decision (Human Verification 1). |
| VERIF-01 (manifest-lint clause) | 02-04 | SATISFIED for the clause | `manifest.lint` in the canonical ci command. VERIF-01 overall stays Pending in REQUIREMENTS.md, consistent with its Phase 1 partial status. |

All IDs in the seven PLAN `requirements:` fields appear in REQUIREMENTS.md. No orphaned requirement: every REQUIREMENTS.md row mapped to Phase 2 (PROF-01, PROF-02, EVID-01, EVID-02, EVID-03, EVID-05, EVID-06) is claimed by at least one plan. EVID-04 (Phase 1, owner-pending logo digest) and PROF-03 (Phase 4) are correctly not claimed.

### Anti-Patterns Found

None blocking. No `TBD`, `FIXME`, `XXX`, `TODO`, or `HACK` in any Phase 2 source, doc, or data file. No stub returns in validators; each reason code has a control. Code review findings CR-01, WR-01 to WR-09 and IN-04 are fixed per `02-REVIEW-FIX.md`; I re-observed the CR-01 and PASS-rule behaviors directly. IN-01, IN-02, IN-03 and IN-05 were left out of scope by instruction (advisory, not blocking).

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|--------------|----------|
| none | The SC5 producer half is NOT deferred under the Step 9b rule: no later phase's goal or success criteria text mentions disputes or a disputed label. It is listed as a human decision instead. | n/a | grep of ROADMAP.md Phase 3 to Phase 6 for dispute and label text: no match |

### Known open items, classified

- (a) Live CI of the phase PR and RESEARCH A1 (curl on the hosted runner): pending, orchestrator-owned, not observable before publication. Human Verification 2 and 3. Not a code gap.
- (b) Real-logo digest (`signature.logo.real_digest` skip): Phase 1 owner gate (EVID-04). Not a Phase 2 truth. Visible as one of the two allowed skips.
- (c) Zero `pins.txt` rows and zero real manifest rows: by design; the positive controls prove the linter and fetch path are exercised.
- (d) Release-asset (redirecting) URLs cannot be pinned: documented in `pins.txt`, `docs/evidence-contract.md`, and `scripts/fetch-pin.sh`; refused as `bad-status`, enforced by `fetch.nc.bad_status`.

### Gaps Summary

No failed truth, missing artifact, or unwired link. Phase 2 delivers a machine-checked vocabulary: the profile, claim tuple, manifest linter and pins linter, SHA-256 pinned fetch, nine-state result validator and dispute registry all exist, are wired into the canonical command, and reject their negative cases with stated reasons. The one honest shortfall is SC5's wording ("produces a result labeled disputed"): only the validator-level disclosure rule exists, and the producer obligation has been moved to Phase 4 in the SUMMARY and STATE but not in the Phase 4 ROADMAP text. Anchoring it there (an orchestrator edit to ROADMAP or REQUIREMENTS, no code) clears the item; after that and a green hosted CI run, the phase is `passed`.

---

_Verified: 2026-11-10_
_Verifier: Claude (gsd-verifier)_
