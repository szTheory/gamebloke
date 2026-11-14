# Linux native CI allowance repair

The owner approved raising only build (ubuntu-26.04, gcc, GNU) from180 to240
seconds, one corrective hosted attempt, and resuming the unused final-records
event only after a new green result. The concrete cause and same-head168/184s
observations are in [the preserved stop](05-FINAL-METADATA-STOP.md). Historical
failed runs and counters remain intact. This is a whole-job maximum allowance,
not an emulator performance improvement. Any new hosted red stops again.

## Exact policy change

The workflow matrix, JSON policy, CMake allowlist and exact preflight matrix
agree on240seconds for Linux native. The two existing macOS300second exceptions
remain; the other eight jobs remain180seconds. No job/check, action, permission,
dependency, upload or runtime/producer input changed. No cache or concurrency
was introduced. GitHub documents the field as a maximum job allowance before
[automatic cancellation](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idtimeout-minutes).

Before implementation, the approved four-minute row was refused by the old
exact preflight for workflow-budget-matrix-mismatch. Current controls require
the approved row to pass, refuse its old three-minute and unauthorized
five-minute replacements, and separately refuse raising Linux diagnostic rows.
Policy controls reject300,180 and string-valued240 for the Linux native row.
The existing collector driver exercises every exact approved boundary and its
one-second excess, as well as missing/red/misbound jobs and source identities.

The admitted collector's disposable source now copies both current budget
authority files, matching its other current-policy overlays and the legacy
driver. This keeps uncommitted candidate checks bound to the policy actually
under review. It grants no synthetic document live-evidence authority.

## Local verification and evidence boundaries

The source/budget collector controls passed with isolated synthetic documents;
they are parser controls, not actual hosted results. An attempted admitted
driver using a preserved older consumer log correctly refused the clean test
for an identity mismatch. That log remains unchanged. The parent then refreshed
only actual current-head setup/output evidence before reusing it in the driver.
This harness input failure is neither a product regression nor invented TDD
history. The producer and full parity/runtime matrices were not regenerated.

Exact commands, logs, current file hashes, owner-file preservation and the new
one-attempt ledger are retained under ignored build/phase05-linux-native-budget240/.
Canonical local CI and independent policy review are pending at report creation;
their terminal outcomes will be appended before publication. No hosted attempt
or new metadata edit has occurred for this candidate.

No merge, settings/bypass, release, private content or Phase6 authority is added.
The original final-records event remains a separate later event, never a retry
credit. The owner guide/bespoke-judgment and ancestry-preserving merge choice
remain the phase boundary after technical publication is complete.

## Local terminal supplement

The manual source/budget controls passed, and admitted controls passed in58.35s
using refreshed current-head actual setup/output evidence (configure3.39s,
no-op build0.67s, selected current setup/output35.76s). Canonical local CI passed
in218.60s with its strict inventory and explicit optional-tool skips.

Independent review then found a real recurring test defect: final-budget-controls
still assumed every non300second limit was180seconds. Its old native240 case
wrongly expected181seconds to refuse. The parent reproduced the actual failure
before changing that test-only branch. The test now derives exact-limit and
plus-one-second timestamps from each job's actual policy value. The actual
registered diagnostics.final.budget and standalone eleven-job boundary/refusal
control pass after this correction. No production budget validator changed.

The canonical native CI result precedes this test-only correction; its unrelated
implementation results remain scoped to that run. Current focused recurring
budget checks cover the correction without repeating an unchanged corpus,
sanitizer matrix or full native suite. Independent focused rereview closed the blocker, retaining its history and
confirming the correction changes only the test branch. No hosted attempt has
been consumed.
