# Renewed runtime stop: two macOS jobs need a policy decision

Owner subsequently accepted this exact proposal and one additional hosted run.
The new ledger is `build/phase05-job-budgets/approval.json`, used0/authorized1.
Prior stop and counters below remain historical. Cheap consistency checks must
precede long gates; any new hosted red stops work again.

ACTIVE STOP: no implementation, fixes, retries or fixture readmission until owner
renewal. The bounded repair cycle's one hosted attempt is used. Parent completed
read-only diagnosis and preserved raw evidence; no fix or retry followed the red.
The owner acts next on the concrete budget proposal below.

## Actual outcome

[Renewed hosted run](https://github.com/szTheory/gamebloke/actions/runs/43549814002)
at source `85c361edfd6b56640e68e64c5e32527f85408836` completed cancelled. The required
aggregator correctly failed. Source input is
`95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`.

| Required hosted path | Actual result | Job seconds |
| --- | --- | ---: |
| Linux native build | Passed |170 |
| Linux ARM diagnostics | Passed |171 |
| Linux Thumb diagnostics | Passed |112 |
| macOS Thumb diagnostics | Passed |152 |
| Hygiene | Passed |142 |
| Sanitizer | Passed, no finding |148 |
| macOS native build | Workflow/tests passed, job cancelled at limit |182 |
| macOS ARM diagnostics | Cancelled during final coverage |197 |

Job elapsed time includes scheduling/cancellation/cleanup beyond the configured
180-second timeout; these elapsed values do not authorize a larger cap. Title
and change routing passed. The run is not green and cannot qualify admission.

The macOS native workflow step itself is marked success: main149.46seconds plus
successful inventory post0.64seconds. Checkout/configure/build/cleanup pushed the
job over the limit, and its authoritative job conclusion is cancelled. We do not
reinterpret that as a green job. ARM source build84.27seconds and setup42.96
completed; every guest/API receipt and exact execution-set guard passed. The
job was cancelled during final coverage; remaining coverage/pipeline/receipt/
dependency/post checks are not claimed passed on that hosted runner.

Before publication, all ten current local lanes and inventory posts passed.
Current722-image/367-ID sets on both hosts passed1444 per-image audits and4332
literal cross-host/historical comparisons. Actual complete local host receipts
passed the frozen validator, and missing-case evidence was rejected. These facts
remain local proof, not a complete new hosted qualification. Canonical fixture
pins remain historicalc3/inputd9, and Plan23 is incomplete.

Raw current run/jobs API documents, all eight heavy job logs, required-aggregator
log and attribution are retained under ignored
`build/phase05-plan23-proof-r4/hosted-attempt1/`. ActiveSTOP.json and the repair
approval ledger record used1/authorized1. No historical counter or failed raw
record was reset. The exact frozen source is pushed to draftPR8; no merge,
release, license change or Phase6 occurred.

## Concrete recommendation for owner approval

Raise only `build (macos-26, cc, AppleClang)` and
`diagnostics (darwin-arm64, arm)` from three to five minutes. Keep every other
job at three minutes, all11 required jobs, every existing assertion/refusal and
inventory, and the same no-binary-upload/rights boundaries. This adds at most
four runner-minutes to the allowed total per run; actual use may be shorter.

The original timeout annotations cite the small Phase1 foundation timings, which
no longer represent the complete CPU/diagnostic verification workload. The
measured repairs removed real duplicate work, but the current two cancellations
are still budget failures. A targeted measured budget is less disruptive than
adding jobs or removing verification coverage.

If approved, apply the exact per-job policy to workflow matrix fields, routing
controls, receipt collector and hosted budget validator. Keep unknown/missing/
wrong-platform budgets fail-closed; test exact limits and over-budget negatives.
Do not merely change the workflow while leaving the proof validator hardcoded
at180seconds. No universal cap increase or new job is proposed.

Then independently review, run relevant local workflow/policy/collector controls,
verify immutable producer ancestry and unchanged bytes, and permit at most one
additional hosted attempt. Consumer policy changes do not justify changing ROM
bytes or relabelling old receipts. The producer-input/current-review-head
separation is already a necessary later admission contract; any concrete
implementation must preserve actual tested-head and source-pin evidence.
A new hosted red stops work again. This proposal is unimplemented and grants no
authority until the owner explicitly accepts it.

## Process lesson

Do not require a mature full-matrix verification path to fit a timeout derived
from an early smoke build indefinitely. Keep useful duplicate-work repairs, but
revisit an obsolete time budget through an explicit owner decision when current
complete evidence shows that the remaining bottleneck is the policy itself.
Avoid another long generic optimization cycle or unchanged retry as a substitute
for that decision. Timing gates and correctness/rights gates remain distinct.
