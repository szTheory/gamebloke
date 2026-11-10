---
phase: 01-ci-and-verification-foundation
plan: 08
subsystem: infra
tags: [github-rulesets, branch-protection, cmake-string-json, posix-sh, stub-gh, negative-controls]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "ci.yml with the CI required aggregator (plans 01-06); action pin and workflow policy gates, ci lane at 83 expected (plan 07 plus two orchestrator controls); gbl_expect_fail/gbl_expect_pass; gbl_add_test and the inventory compare"
provides:
  - ".github/rulesets/main.json: the authorized ruleset body (pull_request and required_status_checks with context `CI required`, integration_id 15368, bypass_actors empty)"
  - "cmake/RulesetCheck.cmake: validate, projecting canon, and find-id, ci-required-ok and advisory readers, all through string(JSON)"
  - "scripts/apply-repo-settings.sh --check | --apply | --print-canonical [--repo owner/name]: idempotent create-or-update of only the main ruleset, refuses until a CI required run has succeeded on main, advisory lines for every other setting"
  - "tests/ci/ruleset_cases.cmake: mutation and stub-gh case driver; ci.ruleset.* (4) and ci.repo_settings.* (6) tests in the ci lane only"
affects: [01-09, 01-10, 01-11]

actuals:
  tokens: 9400
  tasks: 2
  commits: 2
plan_head_before: b85772a2867d0c2302e7da03ea90ac54e78b96d4
plan_head_after: 3199f994c21ef17fbb245d08bf64bd3d9d0cf6fc

tech-stack:
  added: []
  patterns:
    - "All JSON handling lives in one CMake script mode (string(JSON)); shell scripts only fetch and write with gh, never filter with --jq, so a stub gh serves every branch offline"
    - "Drift comparison projects the live object onto the keys the committed file sets, and compares security-relevant lists (rule types, bypass actors, ref lists, required checks) in full"
    - "Stub-gh controls build every canned response with string(JSON) at test time; a write attempt is recorded in calls.log as WRITE-ATTEMPT and asserted absent after a refusal"

key-files:
  created:
    - .github/rulesets/main.json
    - cmake/RulesetCheck.cmake
    - scripts/apply-repo-settings.sh
    - tests/ci/ruleset_cases.cmake
  modified:
    - tests/ci_gates.cmake
    - tests/inventory/ci.txt

key-decisions:
  - "The pull_request rule omits allowed_merge_methods, so no merge method is restricted; the REST docs list it as optional. Not proven against the live API in this plan (no write is allowed here); plan 10 is the first real apply, and the fallback in the plan (all three methods) is a one-line change plus a canon-neutral edit"
  - "Validate folds name, target, active enforcement and the ~DEFAULT_BRANCH ref scope into `ruleset: enforcement: <what>` so a ruleset that targets nothing cannot pass as authorized"
  - "A gh failure is its own reason `repo-settings: gh-failed: <endpoint>` with exit 3, kept apart from drift or missing (exit 1) and usage (exit 2)"
  - "--check always fetches the repo and Actions permissions endpoints and prints five advisory lines (squash title, sha_pinning_required, secret scanning, push protection, rules not requested); each says it was not applied"

patterns-established:
  - "Canonical lines carry no list-breaking characters: `;`, `[` and `]` in values are percent-escaped, and array positions use `.N`, so a canonical line is always one CMake list element and one diff line"
  - "Apply-script scenarios share one sandbox builder (rs_stub_dir) with a per-case allow-write switch, so the refusal controls cannot write by construction"

requirements-completed: [VERIF-05, VERIF-02]

coverage:
  - id: D1
    description: "The ruleset body is limited to the owner's authorization and an offline validator refuses a bypass actor, an extra rule, and a renamed required context, each with a mutation control"
    requirement: "VERIF-05"
    verification:
      - kind: unit
        ref: "ctest -R ^ci\\.ruleset\\. (ci.ruleset.wellformed, ci.ruleset.nc.bypass, ci.ruleset.nc.extra_rule, ci.ruleset.nc.context)"
        status: pass
    human_judgment: false
  - id: D2
    description: "scripts/apply-repo-settings.sh creates or updates only the main ruleset, refuses before any CI required run, reports drift and missing ruleset, ignores server-filled defaults, and every GitHub-facing branch has a stub-gh control"
    requirement: "VERIF-02"
    verification:
      - kind: unit
        ref: "ctest -R ^ci\\.repo_settings\\. (usage, nc.ruleset_drift, nc.ruleset_missing, nc.no_ci_required_run, pos.check_ok, pos.check_server_defaults)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Read-only --check against the live repository reports ruleset-missing with advisory lines and leaves the repository without rulesets"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "sh scripts/apply-repo-settings.sh --check (exit 1, ruleset-missing, five advisory lines); gh api repos/szTheory/gamebloke/rulesets prints []"
        status: pass
    human_judgment: false

duration: not measured (start time was not recorded)
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 08: Ruleset File and Guarded Apply Script Summary

**The `main` ruleset is now a checked-in file (pull request plus the `CI required` status check, no bypass actors) held to the owner's authorization by an offline validator, and `scripts/apply-repo-settings.sh` applies or checks only that ruleset, refusing until a `CI required` run has succeeded on main, with a stub-`gh` control for every GitHub-facing branch.**

## Performance

- **Tasks:** 2
- **Files:** 4 created, 2 modified
- **Verification surface:** ci lane 93 expected / 91 ran / 2 skipped; hygiene lane 41 / 39 / 2 (unchanged)

## Accomplishments

- **Ruleset body (D-13, resolution 2):** `.github/rulesets/main.json` has exactly two rules, `pull_request` (zero required reviews, no merge-method restriction) and `required_status_checks` with one check, `CI required` from integration 15368, and `bypass_actors: []`. Deletion, non-fast-forward, and linear-history rules are deliberately absent; `--check` mentions them as not requested.
- **Validator (D-03):** `cmake -DMODE=validate` reports every finding and exits 1: `ruleset: bypass-actors`, `ruleset: unauthorized-rule: <type>`, `ruleset: missing-rule: <type>`, `ruleset: required-check`, `ruleset: enforcement: ...`. Each of the three refusal cases in the plan has a control that builds its mutation with `string(JSON SET ...)` at test time; no ruleset JSON is committed under `tests/`.
- **Projecting canon (W-8):** `canon` drops server-only fields and keeps a key only when the committed file sets it at the same place, but compares the rule-type set, bypass actors, ref_name include/exclude, and the required check list in full. Scratch confirmation (commands below): a copy with server fields plus an `allowed_merge_methods` default gives an identical canon; an added bypass actor, an added `deletion` rule, and a renamed context each change it.
- **Apply script (D-02, D-13):** `--print-canonical` is offline; `--check` finds the ruleset by name via `find-id`, prints `repo-settings: ruleset ok (id <n>)`, `ruleset-drift` plus a diff, or `ruleset-missing`; `--apply` validates, refuses with `repo-settings: no-ci-required-run` unless a `CI required` run succeeded on main, POSTs when absent or PUTs when present, then runs `--check`. Exactly one `--jq` remains (the default-repo lookup, skipped when `--repo` is given). It also runs under `dash`.
- **Controls with a stub `gh` (W4):** the stub on PATH records every call, serves canned JSON built at test time, and answers a `--method POST|PUT` with `WRITE-ATTEMPT` and exit 1. `ruleset_drift`, `ruleset_missing`, `no_ci_required_run` (also asserts no write attempt) and the two positive cases pass for their stated reasons. I confirmed the controls bite by mutating a scratch copy of the script: removing the CI-required guard, ignoring the diff, and dropping the projection each made the matching case fail.
- **Live read-only smoke:** `sh scripts/apply-repo-settings.sh --check` against the real repository exited 1 with `repo-settings: ruleset-missing` and five advisory lines (squash title `COMMIT_OR_PR_TITLE`, `sha_pinning_required=false`, secret scanning and push protection `enabled`, rules not requested). `gh api repos/szTheory/gamebloke/rulesets` still returns `[]`. No `--apply` was run and nothing was written to GitHub.

## Task Commits

1. **Task 1: authorized main ruleset file with offline validator and controls** - `c76ca06` (feat)
2. **Task 2: guarded ruleset apply and drift check with stub-gh controls** - `3199f99` (feat)

## Verification Evidence

- Task 1 automated command: validate exit 0, `ctest -R '^ci\.ruleset\.'` 4 of 4 passed, `Total Tests: 4`. `ctest -R '^ci\.ruleset\.nc\.' -V` printed `expect-fail: ok:` three times with `ruleset: bypass-actors`, `ruleset: unauthorized-rule: deletion`, `ruleset: required-check`.
- Task 2 automated command: `--print-canonical` contains `CI required` and `15368`; `cmake --workflow --preset ci` exit 0 with `inventory: expected 93, ran 91, skipped 2 (allowed 2)`; `cmake --workflow --preset hygiene` exit 0 with `inventory: expected 41, ran 39, skipped 2 (allowed 2)`. `git diff` shows `tests/inventory/hygiene.txt` unchanged.
- `ctest -R '^ci\.(ruleset|repo_settings)\.'` ran 10 tests, all passed; `ci.repo_settings.pos.check_server_defaults -V` contains `repo-settings: ruleset ok (id 42)`; the non-comment `--jq` count in the script is 1.
- Linux proof (Docker available): `sh scripts/linux-lane.sh ci gcc` exit 0, `100% tests passed, 0 tests failed out of 93`, `inventory: expected 93, ran 91, skipped 2 (allowed 2)`.
- `scripts/hygiene.sh --tree`: ok.
- `gh --version`: 2.101.0 (the script needs 2.48 or later for `--paginate --slurp`).
- Task 1 canon scratch commands (files under the scratchpad, built with `string(JSON SET)` from the committed ruleset; server.json adds id, created_at, _links and `allowed_merge_methods` on the pull_request rule):
  - `cmake -DMODE=canon -DFILE=<scratch>/server.json -DREF=.github/rulesets/main.json -P cmake/RulesetCheck.cmake | diff <canon of committed> -` printed nothing
  - same command on `bypass.json`: adds `bypass_actor=5,RepositoryRole,always` and `bypass_actors.count=1`
  - same on `extra.json`: adds `rule_type=deletion`
  - same on `context.json`: `required_check=CI optional:15368` replaces `CI required:15368`

## Deviations from Plan

### Auto-fixed Issues

None needed fixing during execution; the first full run of every control passed.

### Plan-detail choices (not defects)

- **[Rule 2 - Missing critical functionality] Apply write path has an offline control.** The plan's six `ci.repo_settings.*` tests cover the refusal and check branches but not the POST and PUT calls that plan 10 will make for real, although a must-have says every GitHub-touching branch has an offline control. The `check_ok` case now also runs two apply scenarios against the stub (create when no ruleset exists, update when it drifts), asserting the exact `--method POST repos/stub/repo/rulesets --input .github/rulesets/main.json` and `--method PUT repos/stub/repo/rulesets/42 ...` calls, and that the update never POSTs. The stub only accepts writes when its sandbox holds `allow-write`, so every refusal control stays write-proof. Test counts are unchanged (93/91/2 and 41/39/2). Files: `tests/ci/ruleset_cases.cmake`. Commit `3199f99`.
- **Validator reason set widened by one clause.** Name, target, enforcement and ref scope fall under `ruleset: enforcement: <what>`, and a duplicated rule type under `ruleset: unauthorized-rule: <type> (duplicate)`, so a vacuous or doubled ruleset cannot pass. The plan's codes are all present.
- **Added `repo-settings: gh-failed: <endpoint>` (exit 3)** and `repo-settings: invalid-ruleset` (exit 1, after the validator's own findings) so an API failure or an invalid file is not mistaken for drift or a missing ruleset.
- **`allowed_merge_methods` omitted** as the plan directs; see key-decisions for the open risk.
- **Single `feat` commit per task, not RED then GREEN**, as in plans 01 to 07: a committed failing test would turn the ci lane red on main.

**Total deviations:** 1 Rule 2 addition (extra scenarios inside an existing test), 4 plan-detail choices. **Impact:** none on scope, authority boundary, or the 93/91/2 and 41/39/2 totals.

## Known Gaps

- The ruleset body has never been accepted by the live API (plan 10 is the first apply). If GitHub rejects it for the missing `allowed_merge_methods`, set it to `["merge","squash","rebase"]` in the file; canon projects server defaults away so `--check` stays green either way.
- Projection assumes the REST ruleset response uses the same key names as the request body, which the docs state; the stub models that, the live API has not been compared against a real body because none exists yet.

## Auth Gates

None. `gh auth status` was already valid for the read-only smoke run.

## Known Stubs

None. The `gh` stub is a test double built at test time, not a placeholder in shipped code.

## Threat Flags

None beyond the plan's register. The script's new network surface is the `gh api` calls it makes to the repository's own rulesets, check-runs, repository and Actions-permissions endpoints (GET for `--check`; POST or PUT of the ruleset for `--apply`), all covered by T-01-21, T-01-22, T-01-28 and T-01-37. No package was installed (T-01-SC-08). The `--repo` value is restricted to `owner/name` characters before it enters an API path.

## Next Phase Readiness

Plan 01-09 can publish the phase branch. Plan 01-10 applies the ruleset after a `CI required` run exists: `sh scripts/apply-repo-settings.sh --apply`, then `--check` must print `repo-settings: ruleset ok (id <n>)`. The ci lane baseline is now 93 expected, 91 ran, 2 skipped; hygiene stays 41, 39, 2.

## Self-Check: PASSED

Created files exist (`.github/rulesets/main.json`, `cmake/RulesetCheck.cmake`, `scripts/apply-repo-settings.sh`, `tests/ci/ruleset_cases.cmake`); commits `c76ca06` and `3199f99` are ancestors of HEAD; `git rev-list --count` from the recorded base gives 2; plan verification re-run green (93/91/2 ci, 41/39/2 hygiene).
