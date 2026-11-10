---
phase: 01-ci-and-verification-foundation
verified: 2026-11-09T22:00:00Z
status: passed
score: 5/5 roadmap criteria verified (human items resolved or owner-deferred; 1 clause deferred to Phase 2)
covered_files:
  - ".githooks/pre-commit"
  - ".github/rulesets/main.json"
  - ".github/workflows/ci.yml"
  - ".planning/phases/01-ci-and-verification-foundation/01-01-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-01-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-02-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-02-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-03-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-03-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-04-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-04-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-05-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-05-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-06-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-06-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-07-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-07-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-08-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-08-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-09-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-09-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-10-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-10-SUMMARY.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-11-PLAN.md"
  - ".planning/phases/01-ci-and-verification-foundation/01-11-SUMMARY.md"
  - "CMakeLists.txt"
  - "CMakePresets.json"
  - "cmake/CiRequired.cmake"
  - "cmake/ExpectFail.cmake"
  - "cmake/GblTests.cmake"
  - "cmake/InventoryCompare.cmake"
  - "docs/verify.md"
  - "runner/main.c"
  - "scripts/check-junit.sh"
  - "scripts/check-traps.sh"
  - "scripts/check-workflow.sh"
  - "scripts/classify-changes.sh"
  - "scripts/hygiene.sh"
  - "scripts/lib/signature-common.sh"
  - "scripts/linux-lane.sh"
  - "scripts/signature-digest.sh"
  - "scripts/signature-scan.sh"
  - "src/version.c"
  - "tests/inventory/asan.txt"
  - "tests/inventory/ci.txt"
  - "tests/inventory/hygiene.txt"
  - "tests/sanitizer.cmake"
  - "tests/signatures/allowlist.txt"
  - "tests/signatures/bios.sha256"
  - "tests/signatures/logo.sha256"
  - "tests/ub_control.c"
covered_digest: "v4:sha256:c87436753f9a85c820cca5dc29cbadbaf04f4ef6ec3a1a25973009704ef712eb"
behavior_unverified: 0
overrides_applied: 0
deferred:
  - truth: "VERIF-01 clause: the canonical command \"lints the manifest\""
    addressed_in: "Phase 2"
    evidence: "ROADMAP Phase 2 success criterion 2: the manifest linter is registered in `cmake --workflow --preset ci` at the `manifest.lint` extension point left by Phase 1, \"which completes the 'lints the manifest' clause of VERIF-01\". The extension point is recorded in CMakeLists.txt (tests/gates.cmake comment) and docs/verify.md; no manifest exists yet to lint (EVID-02 is Phase 2)."
human_verification:
  - test: "Owner supplies the real Nintendo-logo window digest (EVID-04, real-logo rule)"
    expected: "Owner runs `sh scripts/signature-digest.sh logo <file>` on a GBA-header-bearing file they already hold, outside the repository, and commits the printed `<64 hex> nintendo-logo-window` line through a PR, replacing the single `owner-pending nintendo-logo-window` row in tests/signatures/logo.sha256. Then `signature.logo.real_digest` reports PASS and the inventory summary shows `skipped 1 (allowed 1)`."
    why_human: "Rights-bearing material: the agent must not read, copy, or hash an owner-held file (AGENTS.md rights gate). The owner replied `pending` (01-EVIDENCE.md, Real logo digest). Until then the real-logo window rule cannot match a real file; only the synthesized controls prove the rule path."
  - test: "Publish the unpushed review-fix branch through one PR and confirm `CI required` passes on GitHub"
    expected: "Branch `gsd/phase-01-ci-and-verification-foundation` (origin/main plus code-review fixes CR-01, WR-01..08) is pushed and squash-merged through the ruleset, no admin bypass; the PR's `CI required` and the following main push run are `success`; a `.planning/`-only PR still reports heavy=false with `CI required` success under the new `F `/`R ` classifier markers."
    why_human: "External publication is an owner gate (AGENTS.md). The branch changes .github/workflows/ci.yml and every gate script, and GitHub has not run them. I validated the new `gh api --jq` marker expression with a read-only GET against live PRs #1 and #3 (heavy=true and heavy=false as expected) and ran the full local workflow on the branch, but no live workflow run exists for the fixed code."
---

# Phase 1: CI and Verification Foundation - Verification Report

**Phase Goal:** From the first code commit, one command gives the same verdict locally and as the required GitHub check. It builds a skeleton core and runner, proves every expected test ran, runs sanitizers, and guards the public repository against rights and hygiene leaks. Every gate has a negative control.
**Verified:** 2026-11-09 (branch `gsd/phase-01-ci-and-verification-foundation`, local, unpushed; compared to origin/main 408c4ec)
**Status:** human_needed
**Re-verification:** No - initial verification

## Verdict

All five roadmap success criteria are met by evidence I observed: fresh local runs of the committed code, mutation experiments against a scratch copy, and read-only GitHub API queries. Nothing in the roadmap contract FAILED. The phase is not `passed` because two items cannot be closed by machine: the real Nintendo-logo digest is owner-pending (EVID-04 stays open, correctly "Pending" in REQUIREMENTS.md), and the review fixes have never been run by GitHub. One VERIF-01 clause (manifest lint) is deferred to Phase 2 by an explicit roadmap criterion.

## Goal Achievement

### Observable Truths (roadmap success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `cmake --workflow --preset ci` passes locally on macOS arm64 against a skeleton C17 core and runner (CMake 3.25 floor, preset schema 6, Ninja); `CI required` runs that same preset on PRs and `main`, Linux GCC and macOS arm64, every action SHA-pinned | VERIFIED | Fresh `rm -rf build/ci && cmake --workflow --preset ci` on arm64 (CMake 4.4.3): exit 0, `inventory: expected 120, ran 118, skipped 2 (allowed 2)`. CMake 3.25.3 (Kitware tarball, SHA-256 matched the value in 01-EVIDENCE.md) on a clean copy of the branch: exit 0, `100% tests passed, 0 tests failed out of 120`, same summary line. `CMakePresets.json`: version 6, `cmakeMinimumRequired` 3.25.0, generator Ninja; `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.25)`, `CMAKE_C_STANDARD 17`, extensions off. Live: `.github/workflows/ci.yml` `build` job runs only `cmake --workflow --preset ci` on a matrix `ubuntu-26.04` (gcc, `GBL_EXPECT_C_ID=GNU`) and `macos-26` (AppleClang). PR #1 run 42737836505 logs show `GNU 15.2.0`, `Image: macos-26-arm64`, `AppleClang 21.0.0`, `100% tests passed out of 93`. Triggers: `pull_request` and `push: [main]`. The check named `CI required` is job `ci-required` (`name: CI required`, `if: always()`, `needs` lists every other job). Every `uses:` is a 40-hex SHA with a version comment (grep for non-SHA `uses:` returned none; `ci.actions.pinned` and `ci.yml.policy` gate it). Main push runs 42737946832, 42738703843, 42739825144: all `success`. Ruleset 26908822: `enforcement: active`, 0 bypass actors, rules `pull_request` and `required_status_checks` with context `CI required` (integration 15368). |
| 2 | A `.planning/`-only PR skips the heavy lanes and still reports `CI required` passed; a source PR runs every lane | VERIFIED | Live, origin/main workflow: PR #2 run 42738583708 and PR #3 run 42739573559 (both `.planning/` only): `changes`, `title`, `hygiene` success, `build` and `asan` skipped, `CI required` success. PR #1 runs 42737527955 and 42737836505 (source): hygiene, title, changes, build (ubuntu GNU), build (macos AppleClang), asan, `CI required` all success. No trigger-level path filter (a skipped workflow would leave the required check Pending). Classifier `scripts/classify-changes.sh` fails closed (empty or failed listing, any non-`.planning/` path, short listing, over cap all print `heavy=true`). Branch-only change (WR-05 markers) not yet live: see Human Verification 2; I ran the new `gh api --paginate ... --jq` expression read-only against live PR #3 (`heavy=false`, 7 entries) and PR #1 (`heavy=true`). |
| 3 | ASan and UBSan builds of the core and runner run in CI; a deliberate UB negative control (shift by 32) is caught by the UBSan lane | VERIFIED | `asan` preset: clang-21, `-fsanitize=address,undefined -fno-sanitize-recover=all`; `tests/inventory/asan.txt` lists `core.version`, `runner.*`, `core.*` tests plus `sanitizer.ubsan.shift32` and `sanitizer.asan.heap_overflow`, so the core and runner build and run under the sanitizers. Live PR #1 run 42737836505, job `asan`: `runtime error: shift exponent 32 is too large for 32-bit type 'unsigned int'` and `expect-fail: ok`, plus an AddressSanitizer heap-buffer-overflow trace from `asan_control`. Local, branch code: `sh scripts/linux-lane.sh asan` (Docker, aarch64 ubuntu:26.04, clang 21.1.8) exit 0, `100% tests passed out of 16`, `inventory: expected 16, ran 16, skipped 0 (allowed 0)`, shift-by-32 message present. Control wrapper proven to fail when a gate passes: `cmake -DCMD=/usr/bin/true -DREGEX="runtime error" -P cmake/ExpectFail.cmake` exits 1 with `negative control broken`; wrong-reason output also exits 1. |
| 4 | The CTest plus junit inventory check fails when an expected test is removed or silently skipped, and lists every skipped test with its reason | VERIFIED | Mutation on a scratch copy (my own run): deleting the `core.version` registration made `cmake --workflow --preset ci` exit 1 with `inventory: configure-mismatch` at configure. Controls `inventory.nc.{missing,extra,duplicate,failed,disabled,skipped_not_allowed,skipped_no_reason,no_junit,empty_junit,unsorted,configure_mismatch,label_substring,add_test_*}` plus `inventory.pos.skip_reported` all passed in the local run. Skips listed with reason in the post step output: `SKIPPED signature.logo.real_digest: ... logo digest owner-pending ...` and `SKIPPED signature.owner_local: GBL_OWNER_LOGO_FILE not set`, then `inventory: expected 120, ran 118, skipped 2 (allowed 2)`. Live logs on GitHub print the same summary shape (`expected 93, ran 91, skipped 2`). |
| 5 | Hygiene fails on planted credentials, non-noreply emails, absolute home paths; signature scan fails on planted logo, known-BIOS, out-of-allowlist ROM/save signatures; controls synthesized at test time, no real logo or BIOS bytes committed; a passing control fails the verify command | VERIFIED (with the EVID-04 real-logo caveat below) | My mutation runs on a scratch copy, each exit 8 (verify command failed) except the baseline: planted `/Users/<name>/...` path fails `hygiene.tree` (`home-path`); planted non-noreply address on a non-reserved domain fails `hygiene.tree` (`email`); planted `GITHUB_TOKEN=<36 chars>` fails `hygiene.tree` (`credential`); a force-added `.gba` file fails `signature.tree` (`rom-extension` and `binary-not-allowlisted`); a force-added NUL-bearing `.dat` fails `signature.tree` (`binary-not-allowlisted`). Neutering `scripts/hygiene.sh` to `exit 0` fails `hygiene.nc.credential`, `.email`, `.home_path` and more; neutering `scripts/signature-scan.sh` fails `signature.nc.logo`, `.bios`, `.rom_ext`, `.save_ext` and `signature.digests.wellformed`. So a gate that stops detecting is itself caught by its control. Unmutated baseline (clean copy) exits 0. No logo or BIOS bytes are committed: `tests/signatures/logo.sha256` holds the `owner-pending` marker; `bios.sha256` holds SHA-256 digests only; `git ls-files '*.gba' '*.bin' '*.sav'` is empty; the controls build stand-in blobs under `build/` at test time. |

**Score:** 5/5 roadmap criteria verified. 0 behavior-unverified truths (the signal/cleanup invariants of CR-01 have named passing tests, `hygiene.signal` and `signature.signal`, in the 120-test run).

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|--------------|----------|
| 1 | VERIF-01 clause "lints the manifest" | Phase 2 | ROADMAP Phase 2 criterion 2 registers the manifest linter at the `manifest.lint` extension point left by Phase 1, "which completes the 'lints the manifest' clause of VERIF-01". The extension point is a recorded comment in CMakeLists.txt and docs/verify.md (line ~124); there is no manifest in the tree to lint, and EVID-02 (the linter) is mapped to Phase 2. The match is specific and explicit, so this is deferred, not a Phase 1 gap. |

### Required Artifacts

`verify.artifacts` over all 11 PLAN files: 43 of 45 declared artifacts pass. The two flagged are literal-pattern mismatches, not stubs:

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `scripts/signature-scan.sh` | pattern `signature: logo-window` | VERIFIED (pattern is dynamic) | Code is `printf 'signature: %s: '` fed by `report_matches ... logo-window`; behavior proven by `signature.nc.logo` and the force-add mutation runs. |
| `scripts/classify-changes.sh` | pattern `classify: short listing` | VERIFIED (pattern is dynamic) | Code is `verdict true "short listing (...)"` printing `classify: $2`; proven by `ci.classify.cases`. |

All core artifacts exist and are substantive: `CMakeLists.txt`, `CMakePresets.json`, `cmake/{GblTests,InventoryCompare,ExpectFail,CheckCoreIncludes,CheckSignatureDigests,CiRequired,RulesetCheck}.cmake`, `scripts/{hygiene,signature-scan,signature-digest,check-junit,classify-changes,check-title,check-action-pins,check-workflow,check-traps,apply-repo-settings,linux-lane}.sh`, `tests/inventory/{ci,asan,hygiene}.txt`, `.github/workflows/ci.yml`, `.github/rulesets/main.json`, `.githooks/pre-commit`, `docs/verify.md`.

### Key Link Verification

`verify.key-links` over all 11 plans: every declared link verified (2/2, 2/2, 2/2, 2/2, 2/2, 4/4, 2/2, 2/2, 1/1, 1/1, 1/1). Additional wiring I confirmed by reading and running: the `ci` workflow preset steps are configure, build, test (excludes `^post$`), ci-post (includes `^post$`, `noTestsAction: error`); `ci.yml` `build` job calls exactly that workflow preset; `CI required` aggregates `needs: [changes, hygiene, title, build, asan]` through `cmake/CiRequired.cmake`.

### Data-Flow Trace (Level 4)

Not applicable in the UI sense. The one data-dependent chain is the inventory: `gbl_add_test` registrations feed configure-time comparison with `tests/inventory/<lane>.txt`, and CTest junit feeds `scripts/check-junit.sh` in the post step. Both are exercised by real runs and by the removal mutation above (a removed test changes the comparison result).

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full canonical command, arm64 | `cmake --workflow --preset ci` after `rm -rf build/ci` | exit 0, 120 expected, 118 ran, 2 skipped (allowed 2) | PASS |
| CMake 3.25 floor | `cmake-3.25.3 --workflow --preset ci` on clean branch copy | exit 0, 120/118/2 | PASS |
| Linux ASan + UBSan lane | `sh scripts/linux-lane.sh asan` | exit 0, 16/16/0 | PASS |
| Test removal detected | delete `core.version` registration in scratch copy | exit 1, `inventory: configure-mismatch` | PASS (control fires) |
| Planted home path, email, credential | append to tracked doc in scratch copy | exit 8, `hygiene.tree` Failed | PASS (control fires) |
| Force-added ROM-named and binary files | `git add -f` in scratch copy | exit 8, `signature.tree` Failed | PASS (control fires) |
| Neutered hygiene or signature script | replace with `exit 0` | exit 8, many `*.nc.*` Failed | PASS (meta-control fires) |
| ExpectFail on a passing gate | `cmake -DCMD=/usr/bin/true ... -P cmake/ExpectFail.cmake` | exit 1, `negative control broken` | PASS |
| Live `gh api` marker expression | read-only GET, PR #3 and PR #1 files piped to classifier | `heavy=false` and `heavy=true` | PASS |

### Probe Execution

SKIPPED: no `scripts/*/tests/probe-*.sh` exist and no plan declares one.

### Requirements Coverage

All seven phase IDs appear in PLAN frontmatter and in REQUIREMENTS.md; there are no orphaned Phase 1 requirements (VERIF-04 is mapped to Phase 5, not this phase).

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| VERIF-01 | 01-01, 01-02, 01-06, 01-07, 01-09 | One canonical command, identical to the required CI check | SATISFIED for the Phase 1 contract; manifest-lint clause DEFERRED to Phase 2 | Command builds, tests, scans signatures and hygiene, checks the inventory; CI calls exactly the same preset (`ci.yml.policy` enforces preset-only heavy lanes). "Lints the manifest" cannot exist before the manifest (Phase 2, ROADMAP criterion 2). REQUIREMENTS.md already marks VERIF-01 `[x]`/Complete: that overstates it until Phase 2 lands the linter (see Warnings). |
| VERIF-02 | 01-02 to 01-08 | Every gate has a negative control; verify fails if a control passes | SATISFIED | Per-gate `*.nc.*` controls; `nc.meta.passing_gate_fails_wrapper`; my neutering mutations fail the command. |
| VERIF-03 | 01-01, 01-02, 01-04 | CTest + junit inventory fails on missing/silent skip; skips reported with reason | SATISFIED | Criterion 4 evidence. |
| VERIF-05 | 01-06 to 01-10 | Single required `CI required` check on PRs and `main`, Linux GCC and macOS arm64, SHA-pinned; `.planning/`-only skips heavy | SATISFIED | Criteria 1 and 2 evidence, live runs and ruleset. |
| VERIF-06 | 01-05 | ASan + UBSan builds of core and runner in CI | SATISFIED | Criterion 3 evidence, live run and local Linux container run. |
| VERIF-07 | 01-03 | Hygiene check fails on credentials, non-noreply email, home paths | SATISFIED | Criterion 5 evidence. |
| EVID-04 | 01-04, 01-11 | Signature scan fails on Nintendo-logo bytes, known BIOS, ROM/save outside allowlist | PARTIAL, owner-gated | BIOS (two real digests), ROM/save extension, unlisted-binary rules and the logo-window rule are all live and control-proven. The real logo digest is `owner-pending`: the logo-window rule has no real digest to match, so a committed real logo window is caught only by the extension and binary rules, not by the logo rule. REQUIREMENTS.md correctly leaves EVID-04 `Pending`; do not mark it Complete until the digest lands. |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (non-.planning tracked files) | - | `TBD`/`FIXME`/`XXX`/`TODO`/`HACK`/`PLACEHOLDER` scan | none | No debt markers found. |
| `scripts/lib/signature-common.sh` | 78 | binary detection is `grep -Iq ''`, which in the C locale treats only NUL-bearing files as binary | Info | A 300-byte random file with no NUL byte and no ROM extension passed `signature.tree` in my scratch run. Real ROMs, BIOS images and logo-bearing images contain NULs and are also covered by the extension rule, the 16384-byte BIOS digest rule, and the logo-window rule (once the digest exists), so this does not defeat the roadmap criterion. Worth a hardening note for the binary rule. |
| `scripts/hygiene.sh` | - | allowed email set includes `@example.(com|org|net)` | Info | By design (reserved domains); my first email mutation used `example.com` and correctly passed. A different domain failed as required. |
| `.gitignore` | - | ignored ROM/save extensions are skipped by `--exclude-standard` | Info | By design: an ignored file is not committed. A force-added one is caught (verified). |

### Human Verification Required

#### 1. Real Nintendo-logo window digest (EVID-04)

**Test:** Owner runs `sh scripts/signature-digest.sh logo <file>` on a GBA-header-bearing file they already hold, outside the repository, and commits the printed `<64 hex> nintendo-logo-window` line through a PR, replacing the `owner-pending nintendo-logo-window` row in `tests/signatures/logo.sha256`.
**Expected:** `signature.logo.real_digest` reports PASS (not SKIPPED) and the summary reads `skipped 1 (allowed 1)`.
**Why human:** Rights-bearing material; the agent may not open, copy, or hash an owner-held file. The owner has replied `pending`. Until done, EVID-04's real-logo rule is inactive for real files and `signature.logo.real_digest` is a visible SKIPPED with its reason (never PASS).

#### 2. Publish the review-fix branch and observe `CI required` live

**Test:** Push `gsd/phase-01-ci-and-verification-foundation` and merge it through one PR under the ruleset (no admin bypass), then confirm the PR's and the next main push's `CI required` are `success`, and that a `.planning/`-only PR still reports heavy=false.
**Expected:** All lanes pass on GitHub with the fixed scripts and `ci.yml`, including the WR-05 `F `/`R ` classifier markers.
**Why human:** Publication is an owner gate. GitHub has only run the pre-fix code (120 expected tests locally on the branch versus 93 in the live runs). The classifier change fails closed, so the risk is a spurious `heavy=true`, not a missed code review, and I validated the new jq expression against the live API read-only.

### Gaps Summary

No roadmap criterion failed, so there are no blocking gaps and `gaps:` is empty.

Warnings (non-blocking, for the orchestrator to act on):

1. **REQUIREMENTS.md overstates VERIF-01.** It is `[x]`/Complete while the "lints the manifest" clause is explicitly Phase 2 work (ROADMAP Phase 2 criterion 2). Add a note, or re-open VERIF-01 for Phase 2 verification to re-check that the `manifest.lint` test exists in the `ci` workflow.
2. **EVID-04 must stay Pending** until the owner commits the real logo digest. The current record is honest (01-EVIDENCE.md, 01-11-SUMMARY.md, REQUIREMENTS.md).
3. **WR-05 and the rest of the review-fix set are unproven on GitHub** (Human Verification 2). CR-01's SIGINT handler is covered only by the static `traps.static` gate, since a non-interactive background job ignores SIGINT; TERM and HUP have dynamic probes.
4. **Binary rule is NUL-based** (Anti-Patterns, Info).
5. Two literal-pattern artifact checks (plans 01-04 and 01-06) (`signature: logo-window`, `classify: short listing`) are stale against the final code; the behavior is proven, but the plan text would not match the script by grep.

---

_Verified: 2026-11-09_
_Verifier: Claude (gsd-verifier)_

## Human Verification Outcome (2026-11-10)

1. **Publish the review-fix branch through one PR:** done. PR #4 (squash `4793b10`) ran every lane green on GitHub (including the WR-05 classifier markers live, `heavy=true` for the source PR, 120/118/2 on both OS builds), merged through the ruleset with no bypass, and the main push run (42751830629) is `success` with `CI required` success.
2. **Real Nintendo-logo digest:** the repository owner answered `pending` at the plan 11 checkpoint, on the stated basis that Phase 1 still completes. It is NOT resolved. It remains a visible owner gate: `signature.logo.real_digest` reports SKIPPED with a reason, STATE.md Blockers/Concerns carries it, and **EVID-04 stays Pending** in REQUIREMENTS.md until the digest lands through a PR. Owner next action: run `sh scripts/signature-digest.sh logo <file>` on a file they hold outside the repo and commit the printed row.
3. **VERIF-01 manifest-lint clause:** deferred to Phase 2 (ROADMAP Phase 2 criterion 2, `manifest.lint` extension point). VERIF-01 should be re-checked when the manifest linter joins the canonical command.

