# Plan21 hosted stop

The owner acts next. The standing policy requires renewed approval after any new hosted red before implementation, fixes or retries. No repair, retry, admission or Phase6 work has started. This report and STATE updates are stop bookkeeping only; they are not pushed.

Published source552a3afa428912f265a2af4650302641122ba783 on existing draft PR8. Actual run: https://github.com/szTheory/gamebloke/actions/runs/43411613601 (failure).

Local proof remains valid: every722 final ROM compared literally equal across actual macOS/native Linuxarm64,367 source-built IDs. Fresh native diagnostics workflow146.86s and Linux build85.27s pass. Both sets also match preserved prior bytes. All scan-witness controls pass. Hosted success for the whole required job set is absent, so Plan21 tasks remain incomplete and no new fixture pin is admitted.

## Actual failures and measured limits

- Hygiene configure fails because `result.pos.instruction_reasons positive,result` is registered but absent from tests/inventory/hygiene.txt. It exists in ci.txt. Earlier local source hygiene scans do not exercise this separate lane's configure inventory; this was a preflight gap.
- Linux normal build job is cancelled during test196/355 near its job limit. Prior tests shown in its log pass; the remaining inventory is unverified.
- ASan is cancelled during CPU mutation/control compilation near test76/97. Recorded CPU controls spend roughly5–13s each. The remaining inventory is unverified.
- macOS normal build completes355 tests and its post-inventory but is cancelled at job completion; it is not a green required job.
- macOS diagnostics completes all six tests and post-inventory but is cancelled at182s job duration; it is not valid complete hosted proof. Local workflow plus historical setup underestimated this end-to-end job margin.
- Linux x64 diagnostics succeeds in130s. This partial hash witness cannot override the failed required run or admit fixtures.

Raw API documents and complete job logs are preserved under build/phase05-plan21/hosted-{run,jobs}.json and hosted-{hygiene-failure,linux-build,macos-build,asan,macos-arm64,linux-x64}.log. No synthetic successful hosted receipt was created. CI required correctly fails.

## Recommended bounded renewal

Repair the hygiene inventory, measure repeated compile/setup/parse costs in the timed-out lanes, and reduce duplicate work while retaining all failure classes, sanitizers, inventory checks and fixed180-second job limits. Use targeted reuse only where equivalent inputs are established; do not add dependencies, caches, concurrency or timeouts by default. Preflight every affected workflow in tracked-only checkouts, including the hygiene configure path and end-to-end setup/cleanup margin. Independently review the resulting change, then make one hosted retry through the same PR. A new red renews the stop. If those constraints cannot be met without a material policy change, return the concrete tradeoff to the owner before proceeding.

Model approval through Phase5 remains scoped; temporary executor routing is preserved until its recorded restoration boundary. Owner dirty instructions/config/state-cache/research-cache remain intact. The final phase human-review flags and original Plan19 formal TDD deviation remain open. No user acknowledgement is needed merely to keep this stop in place.
