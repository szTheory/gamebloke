# Final Phase5 hosted stop

Status: ACTIVE STOP after [run43608603097](https://github.com/szTheory/gamebloke/actions/runs/43608603097)
at reviewed consumer6211cb564e13a4501594071bdc31027841260c9d.
The run completed cancelled. No PR metadata edit, fix, retry, verification-record
push, merge or Phase6 work followed. One of three approved planned runs was
used. The two later metadata/records reservations cannot become retries.

The owner-approved publication proposal explicitly says: “Any new hosted red
stops before fixes or retries. Reserved later runs cannot be repurposed as
retries.” That stop now applies. Historical counters and immutable producer
proof remain unchanged.

## Actual outcomes

| Required job | Actual conclusion | Seconds | Limit |
| --- | --- | ---: | ---: |
| changes / title |success |6 /5 |180 each |
| hygiene |success |104 |180 |
| Linux native |success |119 |180 |
| macOS native |success |235 |300 |
| ASan |success |102 |180 |
| Linux Thumb |success |91 |180 |
| Linux ARM |cancelled |184 |180 |
| macOS Thumb |cancelled |181 |180 |
| macOS ARM |failure |258 |300 |
| CI required |failure |5 |180 |

Durations are actual API started/completed intervals. They are not guest
throughput or a conversion from queue time. macOS ARM waited for a runner;
its final started/completed interval is258seconds and it failed a real test,
within its300second budget.

## Clean-build dependency failure

Both ARM jobs fail diagnostics.spike with
toolchain: diagnostic-build-receipt-missing. Its registered OUT points to
build/phase05-spike/hosted-linux-x64 or hosted-darwin-arm64. In admitted mode,
CMakeLists.txt:200 and tests/toolchain.cmake:444 still register this legacy root.
The actual admitted preparation creates current smoke output and its receipt
under build/phase05-spike/admission-smoke instead. The two schemas agree
(gamebloke.arm-diagnostic-run/1), but the test and producer paths are not wired
together. The required artifact is absent in a clean hosted checkout.

Warm local output hid this dependency. Fresh history/configure checks were
useful but did not substitute for a cold required execution. The local GNU
helper deliberately excludes ignored tool installs and therefore exercised
optional skips rather than this required smoke path. Prior local passes and
clean reviews remain observations of their recorded scopes; they do not waive
this newly observed cold-build failure.

## Actual timing and repeated consumer work

Linux ARM: configure4.19s, build66.02s, main95.00s plus setup/cleanup. Its main
reports549passing/1failed out of550; the failure is the missing smoke receipt.
It was cancelled near the full job limit. Its largest tests were actual setup
32.93s, coverage22.08s, final outputs6.84s, receipt controls5.97s and installed
relocation5.37s.

macOS Thumb: configure9.75s, build75.35s and main70.73s. All292main checks and
the strict post passed, but the authoritative job was cancelled at181s. This is
not green hosted qualification. Actual setup alone took40.26s.

macOS ARM: configure10.04s, build120.02s, main99.75s. It reports549passing/1failed,
the same missing smoke receipt. Largest main tests include setup30.20s and
coverage20.18s.

Source inspection confirms repeated consumer work: the build generates the
immutable production index; admitted setup invokes diagnostic_admission again,
which unconditionally regenerates that index and requests a runner rebuild
(tests/gates/diagnostic_admission.cmake:204–226). This is a candidate for measured
reuse after validating exact current authority/index/executable identity. The
time it would save is not yet measured; setup timings include other real work.
No duplicate work has been removed and no repair experiment has run under this
stop. Producer byte-affecting inputs need not change for the proposed consumer
repair.

## Read-only execution observations

The four real log payload groups were parsed with the tracked admitted parser,
using their exact host/group/run contexts and immutable expected output set.
Both hosts emitted478ARM/smoke and244Thumb guest results; ARM groups include
19native API rows. Actual output hashes match the unchanged producer pins and
the source/consumer identities bind85c/6211cb. These are partial observations,
not a substitute for all-green required jobs or a hosted-proof receipt.

Raw final API documents, four diagnostic logs, macOS native log, stage timings
and observations remain ignored in build/phase05-final-publication/event1/.
read-only-diagnosis.json records timing/failure details;
execution-observations-not-qualification.json explicitly has hosted_qualified
false. No successful final-hosted receipt was manufactured. Initial read-only
parser audit used the all-group expected set for a split group and correctly
refused it; correcting only the caller selection allowed observation parsing.
No source or log was changed.

## Recommended next decision

Authorize one bounded consumer repair/review cycle for these two issues:

1. Wire the smoke check to the current admitted producer receipt, keeping all
   existing source/tool/architecture/scan and real positive/broken checks.
   Add a cheap clean-tree prerequisite/path check before the full corpus.
2. Measure and remove repeated index/render/rebuild work only where exact
   current identity and refusal checks can be preserved. Use a cold required
   ARM/Thumb path with phased setup/build/test/post timing before hosting.
   Retain existing job names, caps, checks, actions and producer pins.
3. After relevant local checks and independent review pass, allow one additional
   hosted candidate attempt. A new red stops again. Only a new green result
   may resume the two previously reserved metadata/records events.

This is a proposal, not renewed authority. No timeout/cache/concurrency/job,
dependency, producer-input, fixture-right or runtime behavior change is proposed.
Do not repeat unchanged source-parity proof. If evidence requires a materially
different remedy, surface that decision. Owner acts next to authorize repair;
until then only stop bookkeeping and read-only diagnosis are permitted.

Plans24/25, shared requirement/phase completion, owner guide/probe/process
judgments, the ancestry-preserving merge choice and Phase6 remain gated.
