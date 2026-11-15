# Exact job budgets and producer/run binding

Status: local verification passed; implementation candidate awaits independent review.
This report does not complete Plan23 or qualify hosted evidence. Parent owns
commits, publication and the single newly authorized hosted attempt. Historical
stops, exhausted approval ledgers and every r4 receipt remain unchanged.

## Observable policy change

| Expanded required job | Before | Candidate |
| --- | ---: | ---: |
| `build (macos-26, cc, AppleClang)` |180s |300s |
| `diagnostics (darwin-arm64, arm)` |180s |300s |
| Remaining nine required jobs |180s |180s |

All seven workflow definitions and 11 expanded required jobs remain. Explicit
build naming preserves its original three-field names when the matrix gains a
timeout field. Diagnostic names, required needs, permissions, action pins,
concurrency, upload behavior and all existing checks remain.

`cmake/CiJobBudgets.json` is the shared budget/source table. The CMake loader
also enforces the exact owner-approved name set, numeric caps and producer pin,
so changing the table alone cannot silently authorize another cap. Workflow
preflight, final hosted-proof validation and the promoted local collector read
that table. Unknown names, missing/duplicate jobs, wrong platforms, swapped
groups, absent/misbound matrix budgets and unauthorized caps fail closed.

GitHub's current [job timeout documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idtimeout-minutes)
defines the job limit; the [context availability table](https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#context-availability)
permits the matrix context for that field. No action versions changed.

## Cheap checks precede compilation

Canonical standalone command:

```sh
cmake -P cmake/CiPreflight.cmake
```

The same check runs before `project()` and compiler discovery in each canonical
configure. The existing CI routing test runs its workflow mutation controls, so
there is no extra job or inventory entry. The parser deliberately accepts the
committed workflow's limited indentation/field layout rather than introducing a
YAML dependency.

Every checkout verifies the current producer input digest. Full-history
checkouts additionally invoke `gbl_check_diagnostic_authority` to verify real
ancestry and exact bytes/modes, forcing mode comparison despite `core.filemode`.
Shallow native/sanitizer/hygiene metadata configure explicitly reports ancestry
and modes as unchecked. Required diagnostics refuse missing authority before
compilation. Source-proof stage alone does not make a metadata lane require the
historical object: native and sanitizer presets also use that stage.

## Evidence remains honestly bound

Immutable producer:
`85c361edfd6b56640e68e64c5e32527f85408836`, input
`95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`.

The final consumer distinguishes actual checkout/run `reviewed_head` from
producer `source_head`. Local r4 receipts retain their actual producer source.
Live API run/jobs must identify the actual new reviewed HEAD and run ID; grouped
log witnesses must identify the immutable producer pin, input digest, tool pin,
current run and exact host/group. Consumer-only descendants can reuse local
producer proof only while its exact bytes/modes/ancestry still verify. Existing
fixture pins and the unqualified production index remain unchanged.
Fresh execution/native API receipts additionally bind the actual consumer
`reviewed_head` separately from producer `source_commit`; the receipt consumer
refuses missing/stale consumer heads. Existing r4 receipts are not edited.

`scripts/collect-hosted-proof.py` promotes the original project-authored ignored
r4 collector without editing that original. It is a local Python-stdlib helper,
not a CI/runtime dependency. It runs the shared full preflight first, accepts
`--reviewed-head`, validates all 11 live jobs and four distinct logs, writes a
fresh receipt, then requires the committed CMake validator as the final gate.

Synthetic parser controls require an explicit isolated invocation, a disposable
source repository marker and synthetic tags on all four input documents. They
report that original producer ancestry was not checked and cannot qualify live
proof. The real consumer refuses synthetic evidence. Missing history never
selects this mode implicitly.

## Verification evidence

Focused results and timings are retained under ignored
`build/phase05-job-budgets/`:

| Check | Result | Wall / CPU seconds |
| --- | --- | ---: |
| Terminal full source/policy preflight |11 exact jobs; producer bytes/modes/ancestry verified |0.13 /0.07 |
| Workflow mutation controls |Missing/misbound caps, Linux cap, groups/platform, names/duplicates, source pins refused |cheap standalone controls |
| Exact budget/date controls |All11 zero/exact-limit positives and over-limit refusals; unknown names, negative elapsed, invalid dates refused |4.46 /2.08 |
| Source/collector controls |Passed, including table type/cap/duplicate changes, historical source/digest, current consumer descendant and synthetic-live refusal |23.91 /9.59 |
| Existing synthetic final-proof controls |All23 parser cases plus changed-authority control passed |326.33 /64.22 |
| Distinct producer/consumer CMake proof |Tagged synthetic producerP→committed consumerQ accepted; old run head, relabelled local source and old digest refused |clean33.78 /10.10 |
| Native configure + selected consumer build |Passed; production index remains unqualified |7.19 /3.18;39.75 /10.73 |
| Existing CI policy/routing tests |All39 passed |8.70 /5.34 |
| Disposable consumer-descendant tracer/receipt |Two real tracer images and receipt passed with producer85c and actual consumer HEAD |22.04 /16.77;0.05 /0.03 |
| Existing seven-stage dependency pipeline |Clean companions and compile/link/header/map/scan refusals passed |5.96 /4.32 |
| Consumer-head controls |Missing/wrong case head and wrong run head refused; original scratch bytes restored |cheap targeted controls |
| Real shallow metadata configure |ci/hygiene and AppleClang sanitizer preset pass without the producer object; required diagnostics refuse it before compiler discovery |included source controls; sanitizer configure6.5s |
| Live CMake synthetic refusal |Tagged synthetic evidence with otherwise current API HEAD refused before qualification |0.31s |

The long synthetic wall interval is not equivalent to CPU work or emulator
throughput. The later actual tracer rerun used similar CPU with much shorter
wall time; no cache/concurrency/dependency remedy is inferred from that interval.
The final synthetic wrapper explicitly gives its producer and consumer separate
Git commits. The targeted distinct-head controls cover that changed context;
the existing23 refusal classes remain intact. No fake actual hosted receipt was
created. Synthetic output is labelled as such and the live CMake path refuses it.

An earlier synthetic snapshot was stopped after code changes made it stale; it
is not counted as verification. The first narrow pipeline fixture copied only
its parent JSON and omitted required companion files; the real builder refused
it. The complete actual parent proof directory was then copied byte-for-byte
into the disposable checkout and only the affected controls were rerun. The
failed setup log is retained as `consumer-pipeline-missing-companion.log`.
No full diagnostic matrix or10-lane local proof was rebuilt, and no r4 receipt,
fixture pin, approval counter or shared planning/owner file was rewritten.

## Remaining boundary

Independent review and final parent-owned commit/publication remain. The new
hosted attempt must supply actual all-green API data and four current grouped
logs at the tested descendant HEAD within the exact budgets. A new hosted red
renews the stop. No admission, merge, release, rights/accuracy qualification or
next-phase acceptance follows from these local controls.

Terminal checks: full cheap preflight and workflow policy pass; Python collector
and local-control sources parse; `git diff --check` is clean. No implementation
failure remains observed. Changes are intentionally uncommitted for parent
review. The owner has no action pending at this local handoff.

## Parent review fix

Independent review identified the collector's partial synthetic-tag acceptance.
The [focused fix](05-23-JOB-BUDGET-FIX.md) enforces all three boolean input tags
and marked, contained source/output scopes before collection; the tagged output
then supplies the fourth tag required by the CMake gate. The actual old mixed
case was reproduced and now refuses. All source/collector controls passed again
in8.76seconds; earlier unrelated proof/CI controls remain at unchanged code.
Producer inputs and r4 proof were not altered, and no hosted attempt was used.

Marked-context re-review additionally required checking every source/output/input
ancestor and preserving original source aliases before canonicalization. The
CMake and Python live gates now do that; all current focused controls, including
symlink/semicolon paths and strict policy JSON, passed in10.70seconds. Empty
unmarked documents still reach their normal incomplete-data refusal, and marked
synthetic clean/consumer-descendant controls remain explicitly synthetic. See
the focused fix report for the retained history and evidence boundaries.
