# Diagnostic CI split: local verification and review handoff

The approved ARM/Thumb split is implemented and all required local checks pass.
The exact verified source is `99cbdb2334adc7fcb5855e720d98757a0d382876`.
The parent acts next: independent review, then its one authorized hosted attempt
if review is clean. No hosted attempt was used here. Plan21 admission remains
gated on actual complete green hosted evidence; Task2, Plan22 and Phase6 have
not started.

This report and the ignored publication handoff deliberately remain uncommitted
through review/publication, as instructed by the parent, so a status-only commit
does not invalidate exact-source receipts. All production changes are committed:

- `caafe2400d9dfb2428a75a25fcbc778aff97ba8a`: split diagnostic proof by host and instruction group.
- `99cbdb2334adc7fcb5855e720d98757a0d382876`: bind every diagnostic group to one source authority.

## Changes and preserved evidence

The existing diagnostics matrix now has four explicit entries with stable job
names containing actual host and group. `diagnostics-arm` and
`diagnostics-thumb` are literal workflow presets, each with its own configure,
build, test and post steps. The default local `diagnostics` preset explicitly
sets `all`; each grouped preset explicitly sets its group. Cached group values
cannot silently carry between these presets. Empty/unknown group values refuse.
Required hosted receipts cannot omit a group or substitute the all-mode layout.

| Group per host | Source-built images | Distinct source-built case IDs | Ownership |
| --- | ---: | ---: | --- |
| ARM |478 |241 |236 semantic pairs,4 guest-stop images, smoke pair |
| Thumb |244 |126 |118 semantic pairs,8 guest-stop images |
| Exact disjoint union |722 |367 | API/owner-only rows excluded |

ARM retains every smoke architecture and raw runner semantic check. Thumb has
its own inventory and no smoke test/dependency or stale-smoke consumption.
Every per-image ELF/v4T, executable disassembly, map/original-input, no-runtime,
header/opcode, source/input/tool/loaded-byte and signature check remains in the
unchanged producer recipe. ARM's476-image matrix parent and independent2-image
smoke parent, and Thumb's244-image matrix parent, each retain their exact real
aggregate scan. Deferred child signature authority still requires its actual
parent receipt, matching producer digest and exact scan witness. Standalone
signature scans were also rerun on every actual parent output directory.

Only the complete platform-independent synthetic receipt controls moved to the
existing hygiene lane, under their existing `diagnostics.final.controls` name.
They are absent from normal CI and sanitizer registration. Hygiene now has206
executable checks, including the prior205; its owner-local optional signature
check remains the sole explicit allowed skip. ARM has6 executable checks and
Thumb5, plus each post-inventory. No skipped result substitutes for a moved check.

Every diagnostic job retains small platform date/budget controls. They call the
same production timestamp and budget functions used by hosted validation, test
zero and exactly180-second boundaries, and refuse181 seconds, reversed time,
invalid/normalized dates, malformed dates and empty input. They do not parse
722 output rows to test a date. Timestamp round-trip checking rejects Darwin
date normalization as well as invalid Linux date input.

The committed hosted validator now requires four unique host/group parts,
current emitted run/job context, exact live numeric job binding, tool archive
pins, producer digest, exact subset case IDs/output names/sizes/digests, no
duplicate jobs/parts/overlap, and the exact full union on both hosts. Every
required live job must be complete/green and take at most180 seconds including
setup and cleanup. Source/input authority and both complete local byte sets
remain mandatory. Hosted hashes are explicitly distinct from local literal cmp.

Pinned actions, PR source-head checkout/full history, the existing required
diagnostics dependency, normal/sanitizer checks, limits and publication settings
remain. There is no new workflow cache, concurrency setting, dependency or ROM
artifact upload. Tool setup repeats in the two extra jobs, as approved.

## Controls, fixes and scope

The original clean receipt baseline and all nine existing refusal classes
passed before implementation (`build/phase05-ci-split/baseline-controls*`).
The updated complete controls preserve those classes and add14 group/job
refusals: wrong/missing/swapped/duplicate groups, duplicate/swapped job binding,
overlap, incomplete union, old two-job layout, missing subset output, duplicate
live ID, and wrong/missing proof run or wrong proof job name. They run as one
explicit synthetic control test in each complete local hygiene workflow.

Self-review found an introduced cache invariant error: selecting a previously
unread group could replace the shared matrix digest before detecting changed
bytes. The new cross-group control first reproduced
`changed-authority-was-reused`; the fix refuses `source-authority-changed` before
loading any new subset. Clean full receipt validation still passes. Exact logs
are `cross-group-authority-red.log`, `cross-group-authority-green.log` and
`authority-fix-clean-current.log` under the evidence root. This was an actual
control-driven fix; no formal CTest/JUnit TDD sequence is claimed or manufactured.

The unchanged exact-line signature-witness function was compared byte-for-byte
against source58 (`witness-check-preserved.json`). All seven existing actual
witness refusals pass: prefix, suffix, substituted line, wrong count, missing
newline, symlink and missing witness. Each restores the real witness exactly;
a fresh complete ARM collection passes after restoration.

The ignored four-log collector has an isolated synthetic baseline plus17
refusal controls for group/log/run/job identity, output/hash/tool/input identity,
missing union, missing/red/duplicate jobs, budget and old layout. Its output is
marked synthetic and requires an explicit synthetic-controls mode; it supplies
no hosted evidence. `synthetic-parser-controls-final/results.json` records them.
Earlier partial parser harness runs are retained; their failures were expected
reason mismatches in the local harness, corrected without changing production.

The CI route control checks the exact four host/group rows, stable names and
literal grouped commands, plus wrong/duplicate matrix negative controls. The
existing missing-required-dependency control remains. All40 focused CI checks
pass; normal metadata index generation and both focused manifest/runner metadata
checks pass. CI and ASan retain355 and97 executable registrations respectively,
plus post checks. Their unaffected complete runtime suites were not repeated
solely for timing. ASan was configured with its existing native Apple Clang
override to check complete registration, without dropping instrumentation.

No producer recipe, assembly/case program, manifest pin, CPU/runner behavior,
license, private content or dependency changed. The producer digest remains
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.
No new external trust boundary, auth path or network endpoint was introduced;
the existing receipt boundary now requires explicit group/run/job identity.

## Exact-source cold preflight

Evidence root: `build/phase05-ci-split/`. `preflight.py` ran all six actual
workflows at source99 from independent `git clone --no-local` tracked checkouts.
Each completed configure/build/test/post. Groups never shared a source or build
root, and the proof verifier checks unowned output directories are absent.
The earlier caafe24 preflight is preserved under `historical-caafe24-preflight*`;
its passed hygiene/ARM observations are historical, not current-source proof.
It stopped at the deliberate source-head assertion after the correction moved
HEAD. It is not counted among the six final workflows.

| Actual complete cold lane | Wall / user / sys seconds | Result |
| --- | --- | --- |
| macOS arm64 hygiene |108.74 /84.08 /20.05 |206 checks, sole allowed owner-local skip, post passes |
| macOS arm64 ARM |39.15 /22.40 /14.96 |478 images/241 IDs,6 checks, no skips, post passes |
| macOS arm64 Thumb |25.25 /14.75 /10.03 |244 images/126 IDs,5 checks, no skips, post passes |
| native Linux arm64 hygiene |135.99 /122.29 /10.88 |206 checks, sole allowed owner-local skip, post passes |
| native Linux arm64 ARM |119.50 /65.85 /45.19 |478 images/241 IDs,6 checks, no skips, post passes |
| native Linux arm64 Thumb |66.70 /50.04 /23.31 |244 images/126 IDs,5 checks, no skips, post passes |

Native tool checks measured0.03/0.04 seconds. Linux tool checks and all raw lane
timings are retained under `preflight/linux/`. Already verified exact xPack
15.2.1-1.1 installations were reused with `--check`, not downloaded again.
Linux used the existing pinned native arm64
`ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`
image with4 CPUs/8GiB, source/tools read-only, and tracked clones under `/work`.
It copied no ignored/private workspace content. Provisioning and the three
sequential Linux lanes together took389.22 seconds; that orchestration interval
is not one hosted lane duration. Native and Linux compiler identities were
checked as AppleClang and GNU through the existing guard.

`verify-preflight.py` checks each exact subset/ID set, actual file size/SHA,
regular-file status, absent unowned directories, full JUnit inventories, and
the complete union. It literally compared every722 macOS/Linux ROM pair,
every722 new macOS ROM against its preserved58 macOS ROM, and every722 new
Linux ROM against its preserved58 Linux ROM. All2166 cmp operations returned0.
`preflight/literal-cmp.json` records each comparison; `preflight/local-proof.json`
contains the complete real local authority with admission false.
`expected-host-hashes.json` is derived expected local authority, not fabricated
observed hosted evidence. Every standalone exact-directory signature scan also
passes (`preflight/standalone-signatures.json`).

Current native/Linux hygiene receipt controls took60.05/95.96 seconds; their
exact workload moved out of the producer jobs. The prior hosted normal jobs
were already green and remain unaffected. These local observations isolate the
approved workload split; they are not emulator-throughput benchmarks.

## Fixed budget and remaining boundary

There is still no current all-green hosted receipt. Historical run43450350277
remains cancelled/red: macOS finished production output proof and ran out during
synthetic controls; no success is inferred for its missing rows. Its raw API/logs
and all earlier stop/counter records remain untouched.

For planning allowance only, the last actual hosted fetch took16 seconds and
setup/checkout about5 seconds. Adding16 seconds plus10 seconds total for
checkout/setup/cleanup allowance to these four local producer workflows gives
65.15/51.25/145.50/92.70 seconds respectively. The extra cleanup allowance is an
estimate. Local machines, containers and hosted runners differ; this arithmetic
does not establish a hosted budget pass. The complete hygiene lane likewise
needs its actual hosted green duration, not a locally inferred result. Every
actual required job must finish green within180 seconds from live API times.
No timeout or check was changed to make the comparison pass.

The parent exclusively owns the one-attempt counter at
`build/phase05-ci-split/approval.json`, publication, live monitoring, raw API/log
collection and the new-red stop. Old exhausted counters were not reset. The
ignored `PUBLICATION-CHECKPOINT.md` gives the four-log collector/committed
validator commands; source99 stays fixed through review/publication unless a
real review fix requires a new source freeze and affected proof refresh.
No local result admits a ROM pin or completes Plan21. No user acknowledgment is
needed for the parent to perform the already authorized independent review.

## Verification and handoff inventory

Canonical commands were the actual `cmake --workflow --preset hygiene`,
`diagnostics-arm`, and `diagnostics-thumb` on each host. Exact orchestrator
commands/limits and all wall/user/sys timings are in `preflight.py` and
`preflight/summary.json`; full console/JUnit/post evidence is under `preflight/`.
Focused routing/metadata, cached-group/refusal, witness and standalone logs are
indexed by their named files above. Source commits, every required generated
proof/JUnit/log, exact union and all literal comparisons were checked on disk.

All source changes passed fresh diff whitespace, tree/staged hygiene, workflow
policy and tree signature checks before their commits. The scanner has no
staged mode; its unsupported invocation was refused and replaced with its real
tree mode, with no bypass. No unrun required local check or implementation stub
remains. Existing owner changes to AGENTS/config/state/cache and untracked notes
were preserved. Shared STATE/ROADMAP/requirements/routing/WINDOWS, approval and
stop records were not mutated by this executor.

Documentation consulted before workflow editing: official GitHub
[matrix include syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idstrategymatrixinclude)
and official CMake3.25 [presets](https://cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html).
Context7 MCP and the ctx7 executable were unavailable; official docs were read
directly. The repository has no ci_monitor helper; no unrelated tool was added.

## Self-check: PASSED

Both introducing source commits are ancestors of source99; all twelve source
paths and this report exist. The final preflight completion marker, six full
JUnit files, all post results, exact source/input identities, group ownership,
2166 literal comparisons, standalone scans, witness restoration and ignored
publication/collector handoff exist. Report/handoff remain intentionally
uncommitted; parent review/publication is the next action, not owner input.
