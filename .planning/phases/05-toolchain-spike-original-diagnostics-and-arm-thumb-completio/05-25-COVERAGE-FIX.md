# Plan25 local recurring-coverage correction

Status: local coverage corrections, bounded integration and final hygiene pass;
expanded candidate binding is saved. This records the candidate response
to05-LOCAL-NYQUIST-REVIEW.md, without editing that independent report or claiming
its rereview, hosted qualification, Plan24/25 completion or owner acceptance.

## Retained offline Linux snapshot regression

`tests/gates/linux_snapshot.cmake` and `linux_snapshot_docker.sh` now exercise the
actual helper using only existing CMake/POSIX shell/Git/tar tools. No real Docker,
network, private content, new runtime language/library or root Git commit is
used by the unit control. `linux.snapshot` is inventoried in canonical CI and
hygiene; it is deliberately absent from ASan because no instrumented runtime
behavior is involved. No job, cap, action, upload or dependency was added.

The fixture has two real commits: a file present only in the root commit must
remain accessible by that ancestor's hash after export. The witness checks HEAD,
commit count, ancestry and root bytes, rather than trusting a one-commit
non-shallow flag. It also checks ignored private/build synthetic sentinels are
absent; tracked edits/deletion, unignored space-containing filename, executable
mode with `core.filemode=false`, and an undereferenced owned dangling link are
preserved; and exact read-only source,4CPU/8GiB, native architecture and canonical
workflow invocation remain in force.

The actual helper runs success, Docker-failure37, HUP, INT and TERM paths. A
foreground launcher records its own PID before exec, so the stub signals only
that test-owned helper and INT is not inherited as ignored from an asynchronous
shell job. The control asserts exact129/130/143 exits and observes the real
temporary snapshot is removed on every path. Each helper invocation is bounded
at15seconds. Five independent mutations must fail for their concrete reason:
checkout-wide mount/private content, shallow history, dropped working executable
mode, INT trap resumption, and omitted cleanup. The cleanup mutant's deliberately
leaked synthetic stand-in is removed after its failure is observed.

Actual shallow-mutant RED was captured through a real unittest assertion and
accepted by the SDK as `RED_EVIDENCE_OK`; it is a deliberately broken fixture
export, not a claim of a new production failure or historical RED commit.
Canonical helper and all negative companions pass. The registered focused CTest
passed in1.78s. An initial stub argument-count mistake was a harness fault, not
RED evidence; its log remains. Cheap hygiene also caught the fixture's initially
non-allowlisted synthetic email. It now uses the existing project control
identity idiom. Parent corrected a separately reported absolute link in its own
Nyquist report; this executor did not edit that report.

## Required installed admitted regression

`runner.relocation_admitted` now registers in required ARM/all diagnostic groups
with the real `diagnostic_execution` fixture, no skip property, and mandatory
admitted-tool evidence. It reuses the same production gate, semantic positive,
broken assertion and SWI stop. The Thumb group has no added ARM fixture/test;
its inventory is unchanged. Literal ARM/all inventories include the required
row without `skip-ok`. Optional native CI/ASan skips remain explicit.

The dependency control now pairs genuine optional-tool absence with required
relocation refusal. Its mandatory invocation omits OPTIONAL, matching required
registration, and must reject `required-admitted-toolchain-absent` without a
SKIPPED marker. Actual all/ARM CTest JSON confirms the required fixture and no
skip property; actual Thumb registration confirms absence. The focused control
passed0.75s. The existing instrumented production binary also passed the required
gate directly, preserving exact positive/broken/stop JSON and trace observations.

## Changed inputs and bounded integration

Only test/consumer registration, the existing gate's required setup check,
literal inventories and evidence docs changed. Parent's current profiles/agb.json,
docs/profile-agb.md and cmake/CheckProfile.cmake changes remain untouched. They
retain profile5/machine behavior and the stable separate exact-head hosted rule.
No producer input, fixture pin/rights or runtime C implementation changed.

Current canonical CI, full admitted all diagnostics, required ARM placement with
actual setup, and one fresh4CPU/8GiB Linux GNU canonical run passed serially.
The Linux run is useful portability evidence for the newly recurring POSIX
control. A new full macOS sanitizer matrix is unnecessary because instrumented
runtime inputs are unchanged; the prior full849/zero-skip result is retained
under its actual identity, with the focused required-gate check separately
recorded. Final CI also reruns all31 profile controls for the parent's current
three files. The expanded snapshot will record their actual hashes/modes and
profile JUnit evidence rather than overwriting the prior13-file candidate.

The tracked admitted collector driver passed against the latest complete
actual diagnostics log plus the preserved R4 proof, followed by passing explicit
legacy controls. These are local/manual marked synthetic parser tests, not recurring
CI coverage or actual hosted jobs. Source bytecode is disabled. No whole
producer-parity proof is repeated. Commands, logs and timing receipts live in
`build/phase05-plan25/coverage-fix/`.

| Actual current command/check | Outcome and wall time |
| --- | --- |
| Canonical `cmake --workflow --preset ci` | Passed129.55s;1108expected/382ran/726explicit optional skips, strict post. Includes current31profile controls and recurring snapshot test. |
| `cmake --workflow --preset diagnostics` | Passed86.05s;794expected/794ran/zero skips, strict post; required installed relocation ran. |
| `cmake --build --preset diagnostics-arm`, then focused `runner.relocation_admitted` CTest with real setup | Build19.40s;2selected checks passed29.36s. This is focused placement evidence, not a claimed full ARM local workflow. |
| `sh scripts/linux-lane.sh ci gcc` | Fresh native ARM64/GNU15.2.0/CMake4.2.3 passed172.32s total,148s workflow. Recurring snapshot test passed inside Linux; strict1108/382/726inventory. Same pinned image/4CPU/8GiB, no platform override. |
| Tracked admitted collector driver against diagnostics-current.log and R4 proof | Passed41.03s;722actual guest results/19native API rows parsed before marked synthetic clean/refusal/alias controls. No live hosted qualification. |
| Explicit legacy producer-only/source/isolation driver | Passed7.52s; remains a separate manual control. |
| Direct existing instrumented production required relocation | Passed real positive/broken/SWI stop and exact JSON/trace; no new runtime build or full sanitizer claim. |
| `cmake --workflow --preset hygiene` | Passed117.33s;208expected/207ran/1explicit owner-local skip, strict post. Includes offline recurring snapshot control. |

Exact command vectors and output-boundary timings are separate JSON receipts;
CPU counters cover local child processes and exclude Docker daemon/container CPU.
Source export, package setup, build/test/post and total wall are not guest
throughput. No unchanged R4 producer proof or full sanitizer matrix was rerun.
The fresh Linux helper setup was23s; its observed configure/build step intervals
were4.33/13.43s including output/transport overhead. The recurring snapshot
control took2.61s in current macOS CI and0.44s in fresh Linux CI. The final
all-diagnostics installed gate adds a bounded actual consumer check to its
existing ARM inputs. These distinct failure classes needed no cache,
concurrency, timeout, dependency or job change.

## Expanded candidate and local self-check

The audit crosschecks all386ledger rows,722output pins/actual result identities,
19APIrows and every retained R4 terminal artifact hash. Producer85c/input95,
fixture rights, machine behavior and profile5 remain unchanged. All current
JUnit checks have zero failures; intentional native optional and owner-local
skips remain explicit. No source bytecode was created.

`build/phase05-plan25/coverage-fix/candidate-hashes.json` binds25current paths,
their modes/content hashes, actual checked-out HEAD8e21ef6, the ten successful
new command receipts and current JUnit hashes. Its complete candidate.patch
includes tracked changes and newly created source/docs. Parent's three profile
paths are included without modification by this executor. Current canonical
JUnit contains all31profile checks with zero failures/skips; the parent's older
focused profile JUnit is retained and explicitly labeled historical. The prior
13-file candidate record/patch and proof/log receipts remain separate and are
not relabeled to a future commit.

The blocking retained-helper gap and required-relocation warning are addressed
in this candidate. The collector remains accurately documented as a tracked
manual local control, not recurring CI coverage. Parent's original independent
reports are unchanged by this executor; these implementation observations do
not manufacture a new independent approval. Parent rereview is next.

## Remaining boundary

Parent independently rereviews this expanded candidate and binds its eventual
commit. No commit, push, hosted attempt, SUMMARY, requirement/state/phase advance
or Phase6 action occurred. Existing owner-pending taxonomy/prohibition/TDD/rights,
guide clarity, hosted authority and merge-provenance decisions remain unchanged.
