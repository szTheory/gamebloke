---
phase: 01-ci-and-verification-foundation
plan: 10
subsystem: infra
tags: [github-actions, rulesets, required-status-check, path-routing, branching-strategy, evidence]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "PR B (#1) merged; main push run 42737946832 with a successful CI required check run (plan 09); guarded apply-repo-settings.sh and .github/rulesets/main.json (plan 08); changes classifier and CI required aggregator (plan 06)"
provides:
  - "PR A (#2, .planning/ only) ran with heavy=false: build legs and asan skipped, hygiene and title success, CI required success; squash-merged through the active ruleset"
  - "Ruleset `main` (id 26908822) active: pull_request plus required_status_checks (CI required, integration 15368), zero bypass actors; apply-repo-settings.sh --check exits 0"
  - ".planning/config.json git.branching_strategy is `phase`; every later change reaches main through a PR"
  - "01-EVIDENCE.md sections PR B, main push run, PR A, Ruleset and Close-out publication filled; the real-logo-inactive-on-main line recorded in evidence and STATE.md"
  - "The owner's close-out publication decision, recorded below for plan 11"
affects: [01-11]

actuals:
  tokens: 4022
  tasks: 3
  commits: 2
plan_head_before: 6fac7c09ad764ee8cb737c79dcccd9f3cffa659b
plan_head_after: 56a89f9d4b4097f406b60567f3d426452f3a01c2

tech-stack:
  added: []
  patterns:
    - "Ruleset applied last: only after main already has a successful CI required check run and after the .planning/-only PR proved skip routing"
    - "Squash merge through the ruleset with no admin flag; tree equality (git diff --quiet origin/main <branch>) gates the local reset"

key-files:
  created:
    - .planning/phases/01-ci-and-verification-foundation/01-10-SUMMARY.md
  modified:
    - .planning/phases/01-ci-and-verification-foundation/01-EVIDENCE.md
    - .planning/config.json
    - .planning/STATE.md

key-decisions:
  - "The main ruleset requires CI required with no bypass, so GSD uses per-phase branches (git.branching_strategy: phase) and every change reaches main through a PR"
  - "The close-out reaches main through ONE additional PR (owner's answer); plan 11 Task 3 exercises it, this plan opened nothing"

patterns-established:
  - "Owner-authorization gating: a public action outside the recorded authorization (a third PR) stops at a blocking-human checkpoint:decision with no auto-select, and the answer is captured as one machine-readable line in the SUMMARY"

requirements-completed: [VERIF-05]

coverage:
  - id: D1
    description: "A .planning/-only PR skips every heavy lane yet CI required still passes"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "gh run view 42738583708 (PR A, head 6a166ea): changes printed heavy=false; build matrix and asan skipped; hygiene, title, CI required success"
        status: pass
    human_judgment: false
  - id: D2
    description: "The ruleset is applied last, requires CI required on main with no bypass actors, and the checked-in script reports no drift"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "sh scripts/apply-repo-settings.sh --check (repo-settings: ruleset ok (id 26908822)); gh api repos/szTheory/gamebloke/rules/branches/main; bypass_actors length 0"
        status: pass
    human_judgment: false
  - id: D3
    description: "PR A merged through the active ruleset without an admin flag, and the main push run is green"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "gh pr merge 2 --squash --delete-branch (599559c); gh run view 42738703843 (push, main): conclusion success"
        status: pass
    human_judgment: false
  - id: D4
    description: "GSD branching switched to per-phase branches and the owner's close-out decision recorded for plan 11"
    verification: []
    human_judgment: true
    rationale: "The close-out publication choice is an owner decision, not something a test asserts"

duration: not measured
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 10: Routing Proof, Ruleset, and Close-out Decision Summary

**A `.planning/`-only PR (#2) skipped every heavy lane while `CI required` passed, the `main` ruleset (id 26908822, `CI required`, no bypass) was applied last and PR A merged through it, GSD switched to per-phase branches, and the owner authorized one more close-out PR.**

## Performance

- **Duration:** not measured (this plan ran across a checkpoint and a continuation)
- **Tasks:** 3 of 3 (Task 3 is the owner decision checkpoint)
- **Commits:** 2 reachable from the plan base to HEAD: `599559c` (PR #2 squash on main) and `56a89f9` (local close-out branch). The plan's own task commits are `6a166ea` (Task 1) and `56a89f9` (Task 2); `6a166ea` is reachable only through the PR #2 squash `599559c`, whose tree it equals.

## Accomplishments

- **Skip routing proven live (D-11):** PR A (https://github.com/szTheory/gamebloke/pull/2) changed 7 files, all under `.planning/`. Run 42738583708: `changes` printed `heavy=false`, build matrix and asan `skipped`, hygiene, title and `CI required` `success`.
- **Ruleset applied last (D-13):** `scripts/apply-repo-settings.sh --apply` only after main had a successful `CI required` check run. Ruleset 26908822, active, `pull_request` plus `required_status_checks` (`CI required`, integration 15368), `bypass_actors` empty. `--check` exits 0 and its advisory lines are recorded verbatim in 01-EVIDENCE.md. Nothing but the ruleset was written (`squash_merge_commit_title` still `COMMIT_OR_PR_TITLE`, `sha_pinning_required` still `false`).
- **Merge through the ruleset:** `gh pr merge 2 --squash --delete-branch`, no admin flag, merged as `599559c`; main push run 42738703843 is `success` with `CI required` success.
- **Branching switch:** `.planning/config.json` `git.branching_strategy` is `phase`; the template is unchanged. STATE.md records why.
- **Real-logo gap recorded:** `real-logo rule inactive on origin/main until a digest lands` is in 01-EVIDENCE.md (`## Ruleset`) and STATE.md Blockers/Concerns.
- **Close-out publication decision (Task 3):** the owner answered the blocking-human decision checkpoint directly and authorized ONE additional close-out PR under the same rules (`CI required` passing, squash-merge through the ruleset, no admin bypass, no direct push). The authorization is exercised by plan 01-11 Task 3. This plan opened no third PR and pushed nothing; the close-out branch `gsd/phase-01-ci-and-verification-foundation` is local and unpushed.

## Owner decision (read by plan 11)

close-out decision: close-out-pr

## Deviations from Plan

None - plan executed exactly as written. One observation, not a deviation: after the squash merge, `gh pr merge` could not fast-forward the checked-out local main because it still carried plan 09's local commit under a different hash. The tree-equality check (`git diff --quiet origin/main 6a166ea`) passed and local main was reset to origin/main as the plan prescribes.

## Authentication Gates

None. `gh` was authenticated with admin on the repository throughout.

## Known Stubs

None. The `tests/signatures/logo.sha256` row remains owner-pending by design (plan 11), a documented gap rather than a stub introduced here.

## Threat Flags

None. The only new trust-boundary surface is the ruleset, which the plan's threat model (T-01-29, T-01-30, T-01-35) covers and whose mitigations were applied: branching switch through PR A, ruleset-only apply script with unchanged-setting checks, and a no-auto-select owner checkpoint before any third PR.

## Issues Encountered

None beyond the local-main fast-forward note above.

## Next Phase Readiness

Plan 01-11 starts from the local close-out branch `gsd/phase-01-ci-and-verification-foundation`. It handles the real-logo digest outcome (EVID-04), updates the inactive-rule line, and opens the single authorized close-out PR for the `close-out-pr` decision above.


## Self-Check: PASSED

- FOUND: `599559c` and `56a89f9` are ancestors of HEAD.
- `6a166ea` exists as a commit but is not an ancestor of HEAD by design: it is the PR #2 branch tip, squash-merged as `599559c` with an identical tree (verified by `git diff --quiet origin/main 6a166ea` during Task 2).
- FOUND: 01-EVIDENCE.md `## Close-out publication` filled; exactly one `close-out decision:` line in this file.
