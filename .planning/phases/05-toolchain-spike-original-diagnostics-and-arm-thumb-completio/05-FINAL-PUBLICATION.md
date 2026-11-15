# Final Phase5 publication proposal

Status: prepared locally; waiting for owner authority. The previous one-run
renewal was consumed successfully by producer-source proof. No new hosted
attempt, PR metadata edit, merge or publication has occurred for this candidate.

## Reviewable result

The final local implementation has clean independent code/security/goal reviews
and resolved local recurring-coverage findings. Current required admitted
execution and installed semantics pass, as do fresh native and GNU/Clang safety
checks. Cheap source/policy/history checks and the offline source-snapshot
regression reject known failures before longer work. Unchanged producer proof
is retained, not regenerated. See05-FINAL-CHECKS.md,05-25-COVERAGE-FIX.md,
05-FINAL-CODE-REVIEW.md,05-FINAL-SECURITY-AUDIT.md,05-LOCAL-GOAL-REVIEW.md and
05-LOCAL-NYQUIST-REVIEW.md. The exact committed candidate and unchanged tested
file identities are recorded in the ignored final freeze receipt.

Prepared PR title: feat: complete ARMv4T execution and admitted diagnostics.
Prepared description:05-PR-BODY.md. Existing draft PR remains
https://github.com/szTheory/gamebloke/pull/8.

## Proposed bounded authority

Authorize at most three planned, serial PR CI runs, with no retry allowance:

1. Push the exact reviewed candidate to existing PR8 and validate actual current
   admitted logs, all11 required jobs, identities and exact budgets.
2. Only after that green result, publish the prepared title/description together.
   The existing workflow intentionally handles pull_request edited and reruns all
   required jobs. Wait for that metadata-triggered run to finish green.
3. Only after those green results, commit/push the resulting verification and
   plan-completion records to the same PR. Validate the final documentation
   descendant's required jobs and actual consumer identity. No runtime/source
   implementation changes are proposed in these record updates.

The workflow's edited handling is explicit in.github/workflows/ci.yml and uses
[GitHub's documented pull-request event types](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request); updating
the PR description is therefore not a free metadata operation. Preparing it
before publication makes this duplicate cost visible instead of discovering
another approval/CI cycle later. No routing, check or action permission is
weakened to suppress a run.

Current caps remain300seconds only for macOS native build and ARM diagnostics,
180seconds for the other nine expanded jobs. Same actions, needs, permissions,
JUnit-only uploads and one-PR scope. Any new hosted red stops before fixes or
retries. Reserved later runs cannot be repurposed as retries after a failure.
If fewer runs are needed, unused authority remains unused.

Final hosted evidence is not inferred from local runs or the old56c source-proof
run. Actual source/run/job/output/native API sets must pass the current admitted
collector and final CMake validator. Keep each real run and its logs separate.

## Excluded and later owner boundary

This proposal does not authorize merge, ruleset/settings changes, bypasses,
release, new license, private ROM/BIOS execution/upload or Phase6. Owner guide
clarity/bespoke judgments, three unclassified probes, descriptor-less
prohibitions and historical process records remain visible. One-time
ancestry-preserving merge-commit recommendation is a separate Phase5 acceptance
decision; squash would remove the producer ancestor and fail main preflight.
Post-merge CI belongs to that later approved merge, not this pre-merge budget.

Parent acts after owner approval. Until then the concrete publication blocker
is exhausted hosted authority; continued automatic publication would exceed
the explicit earlier limit. All authorized local preparation is complete.
