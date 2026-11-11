---
phase: "04"
status: required_ci_passed
checked: "2026-11-11"
pr: "https://github.com/szTheory/gamebloke/pull/7"
tested_head: "4f24e49bd620ec066a3d4f3a17321f5588dd9ee3"
run: "https://github.com/szTheory/gamebloke/actions/runs/43025978730"
---

# Phase 04 hosted check and review handoff

Draft PR #7 is open against main. Required CI passes on the tested head above. Main remains the accepted Phase 3 squash; Phase 4 is not merged or accepted. This local evidence/handoff commit follows the tested PR head and is intentionally awaiting the owner review before its next push.

| Job | Conclusion | Measured job duration | Actual inventory |
| --- | --- | --- | --- |
| hygiene | success | 42 seconds | Required job passed |
| changes | success | 7 seconds | Actual PR diff classified |
| title | success | 6 seconds | Live title checked |
| ASan/UBSan | success | 102 seconds | 72 expected/ran, zero skipped; post check passed |
| Ubuntu 26.04 GNU | success | 149 seconds | 329 expected, 328 ran, one allowed owner-local skip; post check passed |
| macOS 26 AppleClang | success | 140 seconds | 329 expected, 328 ran, one allowed owner-local skip; post check passed |
| CI required | success | 6 seconds | Required aggregator passed |

The initial PR-open run was superseded/cancelled by the grouped body-link correction. The final current-head run completed successfully; no red hosted failure was remediated or retried. Raw job snapshots and logs are retained under `build/phase04-review/hosted-*`.

## Critical path review

The longest job is GNU at 149 seconds, within the unchanged 180-second job limit. Its CTest portion took 130.27 seconds; macOS CTest took 123.34 seconds and instrumented CTest 86.10 seconds. Checkout/setup is small; distinct compilers and instrumented execution provide separate evidence. The intentional scratch variants and negative controls establish recurring correctness failures. No measured low-risk setup/build duplication justified a CI cache, new job, dependency, concurrency change or timeout increase. Keep the current limits and review future growth from fresh measurements.

## Who acts next

Owner: judge whether `docs/evidence-contract.md` clearly explains the bounded runner, mailbox, metadata authority and evidence/rights limits (Plan 12 D3; pending test in 04-UAT.md). The concrete limit is that synthetic controls establish configured software behavior and grant neither ROM redistribution rights nor hardware, BIOS-boot or game-compatibility qualification. All eleven probe rows remain unresolved. No risk acceptance or probe classification is requested.

Agent after owner response: record the actual UAT result, use the canonical verification transition/predicate, complete Phase 4 bookkeeping only if permitted, publish the bookkeeping through this same PR and inspect required CI, then squash-merge under the standing policy and verify main. If any new hosted run is red, preserve the standing stop and report before fixing or retrying. Begin Phase 5 only after the phase boundary passes. No tag, release, LICENSE or unrelated publication is authorized.

Pre-existing AGENTS.md, config.json, state cache and research cache edits remain preserved and excluded from these commits. Private inputs remain ignored and were never CPU fixtures.

## Owner acceptance and publication follow-up

Owner replied "yes i accpet" to the pending evidence-guide clarity question. The canonical UAT completion and verification transition pass. The initial snapshot above remains historical evidence for its exact tested head; final accepted bookkeeping is being published through the same PR and must pass required CI before squash merge. No source change or new rights/qualification claim follows.
