# Plan23 hosted runtime stop

Owner subsequently answered Yes to the bounded recovery proposal below. One
performance repair cycle and at most one fresh hosted attempt are now authorized;
no retry has been used. See ignored `build/phase05-plan23-runtime-repair/approval.json`.
The original stop/evidence below is historical; any new hosted red stops work again.

Implementation, fixes, retries and readmission are stopped pending explicit owner
renewal. Plan23 is incomplete. The source is pushed to existing draft PR8; no new
fixture pins were admitted and no Phase6 work started.

The accepted Plan23 constraint is: “Stop on any red hosted CI before fixing or
retrying.” This is a new hosted red, not a continuation of an old repair budget.
Earlier approval counters remain unchanged. The owner acts next on the bounded
recovery recommendation below; the agent must preserve the stop meanwhile.

## Actual evidence

Frozen source: `5b71c0ace49c2e91ea5d09ac076bf1c97b332663`.
Producer input: `792d96f7e3f4aa1f513a404ae6dc0e379544d6ff4b9fb402dd62360c8ce460c7`.
[Actual hosted run](https://github.com/szTheory/gamebloke/actions/runs/43537187982)
completed cancelled; required aggregation failed. Both native build jobs and both
ARM diagnostic jobs were cancelled by their unchanged three-minute job limits.
No assertion failure appears before cancellation; incomplete jobs are not passes.

| Hosted job | Actual outcome | Job seconds |
| --- | --- | ---: |
| Linux native build | Timeout/cancelled |196 |
| macOS native build | Timeout/cancelled |198 |
| Linux ARM diagnostics | Timeout/cancelled |195 |
| macOS ARM diagnostics | Timeout/cancelled |196 |
| Linux Thumb diagnostics | Passed |74 |
| macOS Thumb diagnostics | Passed |129 |
| Linux hygiene | Passed |167 |
| Linux sanitizer | Passed, no sanitizer finding |154 |

Job elapsed seconds include cancellation/cleanup beyond the configured timeout;
they are not evidence that a larger cap was allowed. Title and change routing
passed. The required aggregator correctly rejected the cancelled jobs.

Complete fresh local evidence remains valid at its stated boundary: ten local
lanes passed;722 actual images per host matched literally across Darwin/native
Linux ARM64 and both preserved baselines. Source-only production metadata refused
even matching bytes before guest execution. Local Linux ARM64 runtime was never
hosted Linux-x64 qualification. Historical setup comparisons did not predict the
fresh hosted build and test cost closely enough; complete local green was
insufficient for hosted readiness.

Raw run/jobs API documents and complete actual logs are retained under ignored
`build/phase05-plan23-proof-r2/hosted-attempt1/`, with active `STOP.json`.
Current local proof, comparisons, audits, commands and timings remain under
`build/phase05-plan23-proof-r2/`; the frozen receipt artifact digests were checked
before publication. No successful current hosted-proof receipt was fabricated.

## Measured causes and concrete recovery recommendation

macOS native configure/build took about9/36seconds. Its result-contract test took
53.53seconds and metadata32.03; it was still in tree/signature checks at timeout.
Linux native configure/build took about3/20seconds, result-contract30.20,
metadata19.79, manifest14.55; timeout occurred during CI policy checks. The two
native lanes did not finish their inventories.

ARM build took about72seconds on macOS and48 on Linux. Grouped execution then
took80.88/78.07seconds. macOS cancelled during per-image receipts; Linux completed
the exact-set guard in8.37seconds and cancelled during final coverage controls.
Thumb completed independently. Hygiene's synthetic proof controls alone took
61.78seconds, so it also has limited margin despite passing.

Read-only source inspection identifies repeated authority loading in result and
metadata controls, plus repeated full-ledger/output verification in diagnostic
setup and final coverage, as concrete profiling seams. It does not yet establish
which proposed optimization is safe or how many seconds it will save. No fix,
performance experiment or retry was performed after this stop.

Recommend one bounded repair cycle: profile these exact verification paths,
remove proven duplicate parsing/validation while retaining immutable identity,
all semantic assertions, every negative control, exact inventories and real
per-image checks; independently review; run the necessary focused and complete
local gates; then allow at most one new hosted attempt after measured readiness.
Keep the same jobs and180-second caps, with no speculative cache, new dependency
or concurrency increase. Prefer consumer-only changes; any producer-input change
requires a fresh source freeze and complete proof before admission. A new hosted
red stops the cycle again; a green source-proof still does not complete Plan23
until readmission and required admitted execution pass.

## Process lesson

Use actual current hosted step/test attribution to choose the next change.
Historical machine/runner overhead is contextual evidence, not a budget guarantee.
Sample the expensive verification consumers at final fixture scale before a long
whole-matrix proof run. Keep proof computation and guest throughput distinct.
Do not respond to this overrun by repeating unchanged jobs or increasing limits.
