# Plan 23 admitted execution

Status: local admitted integration and focused controls passed; parent review
and commits pending. This report does not advance the plan,
phase, requirements, or hosted-attempt authority. Parent review and bookkeeping
remain separate.

## Authority and evidence boundaries

The current checked-out consumer is `56c22133f3697f6a78fed6c0e1a549659707056b`,
with the consumer-only registration correction in `tests/toolchain.cmake`
and receipt corrections in `tests/gates/diagnostic_setup.cmake` described below.
The tested source diff and exact file hashes are preserved as `consumer.patch`
and `consumer-file-hashes.txt` beside the logs. Production fixtures pin immutable producer
`85c361edfd6b56640e68e64c5e32527f85408836`, input digest
`95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`.
The canonical manifest digest is
`643f18a6ed5649b0797a75a105a64765eee4936bee985251326224ca4fdf11cb`.
The admission transaction belongs to the parent. The local audit independently
confirms all 722 output hashes, rights fields and expected results match the
previous manifest while all source revisions identify the accepted producer.

`cmake -DREQUIRE_AUTHORITY=ON -P cmake/CiPreflight.cmake` verifies the actual
producer input bytes, executable modes, ancestor relationship and exact eleven
job budget mappings. The installed Darwin toolchain passed its existing
fetcher's `--check`; no installation or download occurred.

The preserved r4 proof establishes literal equality of corresponding local
macOS arm64/Linux arm64 output files, plus its separately recorded image audits
and historical comparisons. Its terminal artifact digests were checked without
altering any r4 artifact. Actual hosted run 43560267506 establishes hosted
log-derived hash equality and green jobs at consumer 56c, as recorded in
[the hosted report](05-23-JOB-BUDGET-HOSTED.md). Those receipts are neither new
production executions nor a new hosted qualification of this local correction.
The one renewed hosted attempt is consumed; no further attempt was made.

## Consumer registration correction

The actual admitted configure failed before the long build because the literal
inventory required `diagnostics.final.outputs` and `diagnostics.final.budget`,
but `tests/toolchain.cmake` registered them only in proof mode. The original
failure remains in `build/phase05-plan23-admitted/configure.log`.

The correction preserves the literal names and labels. Proof mode keeps its
existing final source-output check. Admitted mode registers the existing
`mandatory-final` guard with the actual diagnostic execution fixture and a
dependency on the complete execution-set check. This guard explicitly requires
admitted evidence. Budget negative controls now register in both stages. No
preset, workflow, cap, producer input, source pin, fixture expectation or public
API changed. All/all-ARM/all-Thumb admitted configurations now pass the same
literal inventory comparisons. This is a concrete integration failure and fix;
no historical TDD claim is inferred from it.

The first expanded run then exposed a second concrete consumer error:
`diagnostic_setup.cmake` looked up every admitted image under the smoke
directory. ARM/Thumb manifest artifacts instead have their ISA subdirectory.
The actual grouped producer/runner command had already checked all 722 outputs,
but setup correctly failed rather than issuing receipts for nonexistent files.
`diagnostics-path-red.log` preserves that failure. Setup now selects the existing
producer subgroup for ARM/Thumb and keeps the existing smoke path. No copied
file, admitted pin, expectation or producer input was changed to satisfy it.

After that correction all 722 individual guest receipt tests passed, exposing
the native API receipts' separate source-pin mismatch. Setup issued those
receipts before admitted execution replaced its initial consumer revision with
the canonical fixture producer revision; guest receipts and the run receipt
were correct. `diagnostics-api-pin-red.log` retains the 22 resulting failures
(19 API checks and their dependent aggregate/control checks). API receipts are
now emitted after stage-specific authority resolution, with their existing
early emission retained for genuine optional tool absence. The original native
TAP execution and exact assertion checks remain unchanged.

## Local commands and observed results

All commands ran as background CLI work, with heavy build/test operations
serial. Raw logs and the independently checked identity/results JSON are under
`build/phase05-plan23-admitted/`.

| Check | Actual command | Result / wall time |
| --- | --- | --- |
| Authority | `cmake -DREQUIRE_AUTHORITY=ON -P cmake/CiPreflight.cmake` | passed; producer bytes/modes/ancestry and budget policy |
| Existing tool | `sh scripts/fetch-toolchain.sh --check --host darwin-arm64 --root build/toolchains` | passed; pinned install reused |
| All admitted configure | `cmake --preset diagnostics -DGBL_DIAGNOSTIC_STAGE=admitted` | passed after registration correction;2.11 s |
| ARM admitted configure | `cmake --preset diagnostics-arm -B build/phase05-plan23-admitted-config-arm -DGBL_DIAGNOSTIC_STAGE=admitted` | passed;2.59 s |
| Thumb admitted configure | `cmake --preset diagnostics-thumb -B build/phase05-plan23-admitted-config-thumb -DGBL_DIAGNOSTIC_STAGE=admitted` | passed;2.49 s |
| Admitted build | `cmake --build --preset diagnostics` | passed;91.08 s |
| Narrow production pair | `cmake -DCASE=admitted-pair -DSRC=. -DBIN=build/diagnostics -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake` | actual PASS/GUEST_FAIL, canonical checker, exact case/hash/progress;18.96 s |
| Dependency controls | `cmake -DCASE=all -DSRC=. -DBIN=build/phase05-dependency -DRUNNER_BIN=build/diagnostics -P tests/gates/diagnostic_dependency.cmake` | passed;0.83 s |
| Focused fixed setup | `ctest --test-dir build/diagnostics -R '^diagnostics.execute$' --output-on-failure` | passed;50.07 s |
| Native API/aggregate controls | `ctest --test-dir build/diagnostics -FA diagnostic_execution -R '^diagnostics[.](api[.]\|execution_set$\|receipt[.]controls$\|final[.]outputs$)' --output-on-failure --output-junit phase05-api-controls.xml` |22 passed;17.62 s; reused that actual setup |
| Full admitted lane | `ctest --preset diagnostics` |793 passed,0 failed,0 skipped;119.88 s |
| Exact inventory post | `ctest --preset diagnostics-post` | expected 793,ran 793,skipped 0,allowed 0;0.17 s |
| Canonical native workflow | `cmake --preset ci -DGBL_TOOLCHAIN_ROOT=build/phase05-plan23-absent`, then `cmake --workflow --preset ci` | passed;185.02 s total,170.02 s main tests; exact 1106 inventory,381 ran/725 expected skips |
| Sanitized admitted metadata configure | `cmake --preset asan -B build/phase05-plan23-admitted-asan -DCMAKE_C_COMPILER=/usr/bin/clang -DGBL_DIAGNOSTIC_STAGE=admitted -DGBL_TOOLCHAIN_ROOT=build/phase05-plan23-absent` | passed with no diagnostic outputs/tools;2.50 s |
| Sanitized build | `cmake --build build/phase05-plan23-admitted-asan` | passed;8.14 s |
| Sanitized changed boundaries | `ctest --test-dir build/phase05-plan23-admitted-asan -R '^(cpu[.]pos[.]clean\|runner[.](metadata\|relocation\|relocation_control)\|diagnostics[.]dependency[.]controls)$' --output-on-failure --output-junit phase05-boundaries.xml` |5 passed,0 failed/0 skipped;17.49 s; no sanitizer finding |

The final admitted lane produced 355 production PASS, 355 deliberate GUEST_FAIL,
and 12 named GUEST_STOP results for 722 exact fixture IDs and 367 distinct guest
case IDs. Every actual result passed the canonical parsed `CheckResult` function,
plus exact status, reason, termination, nonzero semantic case ID, loaded/index/
claim digest, ran=true and nonzero bounded cycle/retirement assertions. Separate
native receipts bind 19 API-only ledger rows to actual complete native TAP and
their exact source-declared assertions. They do not claim guest CLI completion
for those lower API boundaries.

The final lane also passed all registered native selectors, coverage ledger
clean/coordinated semantic/stop/API/pending controls, actual source pipeline
compile/link/header/map/scan refusals and clean companions, receipt corruption/
missing output/current token/source/ID/mailbox/stop/native/required-all-skipped
controls, mandatory admitted execution and offline dependency controls.

Canonical `ci` retains its actual preset proof stage and deliberately absent
optional tool root. Its 724 diagnostic skips plus the inherited unset owner-local
file check are explicit permitted skips; the 19 API rows still execute. They do
not satisfy required admitted execution. No private content was opened. The
native sanitizer build uses actual AppleClang 21 at `/usr/bin/clang` with
AddressSanitizer and UndefinedBehaviorSanitizer, `-fno-sanitize-recover=all`
and the existing flags/standard contract. Its metadata and installed relocation
checks pass against admitted source authority without installing a toolchain or
requiring diagnostic outputs in that lane. Relocation here proves the existing
raw owned-fixture packaging contract, not final installed admitted-fixture
execution or package qualification.

The all/ARM/Thumb admitted override configurations exercise the future stage
without changing any tracked preset. The planned diagnostic workflow command
would reset to the current proof preset, so its configure/build/test/post steps
were run explicitly with the admitted override. The tracked stage switch remains
Plan 24 work. Split producer source proof remains valid and was not repeated;
the all-group production lane executes every affected case once per setup.

The build found the canonical native producer roots still pinned to historical
source c3. Existing admission preparation rebuilt those three native roots from
source 85c and checked every copied output against its admitted hash. This is
fresh native source preparation for production execution, not a repeat of the
full cross-host proof matrix. Copied outputs themselves are not described as
executed until the runner results exist.

The dependency control observed actual optional CTest/JUnit skipping, mandatory
absence, wrong-host and unusable compiler refusals, and failure-marker controls
that remain failures rather than skips. It ran the real synthetic control
runner: BIOS-required refusal is SKIPPED/ran=false with zero progress; zero,
missing and incomplete mailbox results cannot validate as completed PASS or
GUEST_FAIL. These synthetic controls prove their contract, not production
fixture admission.

## Additional refusal controls

`proof-refusal.log` records the current canonical proof runner refusing matching
real source-built bytes as REFUSED/fixture-unknown, ran=false and zero progress.
`proof-stage-refusal.log` records that its actual absent-tool receipt set cannot
satisfy the mandatory lane (`required-all-skipped`). Those are different causes.

To isolate stage refusal with tools actually available, a fresh two-output proof
tracer used byte-identical copies of the just-built canonical proof runner and
native test binary in `build/phase05-plan23-proofstage-control`. Actual commands:
`cmake -DSRC=. -DBIN=build/phase05-plan23-proofstage-control
-DGROUP=arm -DSTAGE=proof -DREQUIRED=ON
-DTOOLCHAIN_ROOT=build/toolchains -DHOST=darwin-arm64 -DTRACER=ON
-P tests/gates/diagnostic_setup.cmake`, then the corresponding
`diagnostic_receipt.cmake -DCASE=guard -DREQUIRED_ADMITTED=ON`. Both commands
used the same nonsecret receipt token, recorded in the ignored raw logs. The tracer
built and actually observed its two real current-source outputs in 12.51 s;
canonical checking refused their unregistered raw observations, and the required
guard refused specifically as `required-stage-not-admitted`. Logs:
`available-proof-{setup,stage-refusal}.log`. This is a negative control, not
complete proof/admission. The older r4 receipt consumer predates the current
reviewed-head binding; its unchanged producer proof was preserved rather than
relabelling those receipts as current consumer execution.

Both native `ci` and AppleClang `asan` were independently configured with
`GBL_REQUIRE_DIAGNOSTICS=ON`, admitted stage, and an absent tool root in isolated
build directories. Both fail configure with
`required-diagnostics-toolchain-invalid`/`install-missing`,
without a skip or install attempt. Logs: `required-absent-{ci,asan}.log`.

## Measured cost and handoff

The final all-group main test pass took 119.86 s. Its slowest setup was 32.74 s;
final coverage took 21.73 s, the exact execution guard 5.33 s, mandatory admitted
guard 4.74 s, pipeline controls 4.73 s and receipt controls 6.47 s. The original native
producer refresh took 91.08 s. Canonical native workflow main tests took 170.02 s
and total workflow 185.02 s. These are local shared-machine observations, not a
new hosted runtime guarantee or emulator throughput benchmark. No evidence
supports adding concurrency, caches or dependencies here. Initial concrete
consumer failures and focused green controls justified the bounded final lane
rerun; the complete immutable source proof matrix was not rerun.

There are no remaining local test failures, implementation stubs, new skipped
tests or new network/auth/schema surfaces from these corrections. Parent acts
next: review the two consumer fixes and reports, commit under the parent's
ledger, and finalize the actual plan summary/shared bookkeeping. Current local
receipts honestly bind 56c plus the preserved tested working-tree diff; they are
not rewritten to a future commit. Plan 24/25 and the Phase 5 owner boundary remain
open. No owner input or further hosted attempt is requested by this handoff.

## Self-Check: PASSED

The modified files, report, draft summary, logs and terminal audit JSON exist.
The final source diff has no whitespace errors, and its exact tested file hashes
remain unchanged. Independent JUnit and result/receipt audits match the reported
outcomes. All 14 preserved r4 terminal artifact digests still match. No staging,
commit, push, shared-state update or external mutation was performed here.
