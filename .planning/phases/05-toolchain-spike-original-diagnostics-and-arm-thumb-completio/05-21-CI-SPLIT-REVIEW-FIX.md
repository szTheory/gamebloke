# CI split review fix: final local proof and re-review handoff

The dirty-working-tree hygiene warning is fixed. All six final cold workflows,
complete output/byte comparisons, signature checks and focused regressions pass
at `c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`. Final complete hygiene took130.75
seconds on macOS and124.46 seconds on pinned Linux. Both retain a deliberate
uncommitted producer edit while testing the current parser implementation.

The parent acts next: independently re-review the three changed source files,
including production-validator refusal ordering, then publish the exact tested
head under its existing one-attempt authority if review is clean. No hosted
attempt was used here. Actual green hosted proof and Plan21 admission remain
pending; Task2, Plan22 and Phase6 have not started. No owner acknowledgment is
needed for the already authorized re-review.

This report and the ignored publication handoff remain intentionally uncommitted
through re-review/publication. Source commits are:

- `e3e889f3c8662f8ce1fc581ec2da1cd75b131e12`: isolate synthetic controls from working-tree freshness.
- `f7a93f925fd52241d8da0ea1292b54784a7da71e`: run the snapshot parser without an extra capture process; validate consumed data and complete control execution.
- `c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`: refuse invalid proof metadata before matrix derivation/output parsing.

The prior split implementation/review at source99 remains historical context in
`05-21-CI-SPLIT.md` and `05-21-CI-SPLIT-REVIEW.md`. This report supplies the current
publication source and evidence. Shared planning state, owner changes, approval
and stop records were not modified by this executor.

## Warning reproduction and bounded fix

An isolated tracked-only source99 clone received one extra newline in
`diagnostics/armv4t/cases.json`. The existing synthetic control failed with
`dirty-producer-input:diagnostics/armv4t/cases.json` before exercising its parser
cases. `build/phase05-ci-split/review-fix/dirty-producer-red.log` preserves the
actual baseline cause. The main working tree's producer inputs were untouched.

The hygiene registration now invokes the narrow test-only
`tests/gates/synthetic_diagnostic_controls.cmake` wrapper. It obtains the producer
path whitelist from the committed input declaration, requires the current
declaration to contain exactly that path set, and validates every current input
and both current parser scripts as tracked regular files with no symlink or
resolved-path escape. Unknown/missing/duplicate paths refuse. It copies only
those current bytes into a fresh ignored test directory and verifies each copy's
digest. It never copies ignored/private workspace content or the source Git
directory. Adding a new producer path requires updating the committed whitelist;
the wrapper does not silently widen its copy boundary.

Strict JSON validation uses the existing `GblJson.cmake` policy, including
trailing-content refusal. Needed matrix shape is checked explicitly: schema,
nonempty cases array, consumed source-test kind/ISA fields, and positive uint32
case IDs. The synthetic parser test does not perform assembly/family/form
qualification; all existing real producer coverage and per-image audits retain
that responsibility. No real producer check was removed.

The wrapper commits the copied current bytes in a separate test-only repository
with an explicit synthetic branch/identity, empty Git template and isolated Git
configuration/environment. Current dirty parser changes are executed from that
snapshot; an injected parser failure proved this rather than reusing an older
committed implementation. The real parser runs directly in the same short-lived
test process. There is no generic freshness bypass, new dependency, shared cache
or concurrency setting. Existing real producer and hosted-proof freshness checks
remain strict.

Each snapshot is explicitly synthetic, never real source/build/admission
authority. The wrapper refuses an incomplete parser run unless all23 fresh
per-case validation logs and the separate source-authority refusal log exist.
An isolated current parser changed to return early was refused as
`incomplete-controls`. Both complete hygiene workflows retain206 executable
tests and their post-inventory, with only the existing optional owner-local
signature skip.

## Preserved strict boundaries and controls

`review-fix/focused-results.json` and `metadata-first-focused.log` record13
focused controls: unchanged real producer dirty-input refusal, unchanged hosted
validator dirty-input refusal, source changes between groups, execution of the
current dirty parser, invalid/trailing JSON, invalid schema/cases/row/ID shape,
unknown and missing whitelist inputs, symlink input, and the complete dirty-tree
hygiene workflow. The separate `incomplete-control-refusal.log` proves the
early-return guard. Current dirty matrix and current parser bytes are compared
with the snapshot after successful hygiene, and the producer edit remains dirty.

Real `BuildArmDiagnostics.cmake` rejects the same newline-only input with
`source-input-mode-or-content-mismatch`; real `final-hosted-proof` rejects it with
`dirty-producer-input`. The new source-metadata preflight accepts neither of
these operations. It only permits accepted metadata to reach the existing full
checks. Source changes between cached groups still refuse with
`source-authority-changed`.

The clean synthetic baseline, all nine original receipt refusal classes and all
fourteen group/job refusal classes remain. The exact matrix/output ownership,
source/input/tool identity, every row/hash/size/ID check, complete disjoint unions,
current run/job binding and fixed180-second checks remain mandatory. No required
check was replaced by a skip or metadata-only success. Standalone signature scans
and every original exact-line/nonregular witness refusal are retained.

The final additional ordering pass checks all four host/group/run/job/source
bindings and local source metadata before trusted matrix derivation or output
rows. It compares identity with the actual source Git HEAD, verified live job
mapping and fixed format/host/group expectations. It does not accept an expected
set, digest or other authority supplied by a record. Accepted receipts still
invoke the original full `proof_host_receipt` checks for all four hosted parts
and both complete local sets, and the same union/oracle comparisons.

`unchanged-proof-functions.json` verifies that all seven existing functions for
expected source sets, producer freshness, parent signature witness, collection,
complete host receipt validation and timestamp/budget checking are byte-identical
to source f7. Only rejection order changed. The matched baseline/prototype have
identical isolated reasons for the clean companion and every23 refusal classes
(`metadata-first-probe/refusal-equivalence.json`). A record with multiple faults
can now report a cheap metadata/source error before its deeper row error; it
remains refused. The parent must independently re-review this production-validator
ordering change.

## Measured runtime correction and preserved history

The first review-fix source e3 passed all six workflows functionally, but its
Linux hygiene cost318.17 seconds, including236.73 seconds in the receipt control.
Source f7 also passed functionally, but complete Linux hygiene cost204.98 seconds
with147.61 seconds in that control. Neither is a budget pass or current publication
evidence. Their full receipts/byte proof are preserved in
`review-fix/historical-e3e889f-preflight*` and
`review-fix/historical-f7a93f9-preflight*`; source99's proof remains at the original
`build/phase05-ci-split/preflight/` root.

A separate pinned Linux attribution run at exact e3 measured strict syntax7.83
seconds, full assembly-coverage validation3.63 seconds, complete snapshot and
validation prefix9.87 seconds, and unchanged receipt controls101.41 seconds.
Those observations do not explain the entire earlier236.73-second cost; no
precise causal speedup is inferred from unlike environments/capture paths.
The unnecessary full assembly pass and extra long-lived capture process were
removed, while strict syntax and needed-shape refusals remained. A focused Linux
CTest prototype passed122.98 seconds; it was not called a complete hygiene pass.

Before the final source freeze, one matched baseline/prototype pair ran
sequentially in the same pinned4CPU/8GiB container, with identical dirty producer
data and current implementation bytes verified in each snapshot:

| Receipt-control CTest command | Wall / user / sys seconds | Semantic result |
| --- | --- | --- |
| Frozen f7 baseline |153.02 /148.73 /4.12 | Clean companion and all23 refusal classes pass |
| Metadata-first prototype |96.42 /92.96 /3.31 | Same isolated refusal reasons and complete control logs |

The matched command is `metadata-first-probe.sh`; both complete JUnit files,
parser digests and all24 per-case/authority logs are retained under
`metadata-first-probe/`. The first timing probe was intentionally stopped and
preserved to avoid overlap with a native regression run; only the completed
sequential pair is used above. An earlier probe configuration lacked the existing
project C compiler and stopped before tests; its logs remain under `prototype/`.
The corrected `prototype-v2/` used the existing verified compiler packages, with
no alternative or new dependency.

## Final exact-source cold workflows

Current evidence root: `build/phase05-ci-split/review-fix/preflight/`.
`preflight.py` ran independent tracked-only `git clone --no-local` source/build
roots at exact c3. Each actual workflow completed configure/build/test/post.
Both hygiene clones deliberately retain the newline-only producer edit and
prove the snapshot copied current producer and parser bytes. The four producer
clones remain clean and cannot consume another group's stale outputs.

| Final complete cold lane | Wall / user / sys seconds | Inventoried result |
| --- | --- | --- |
| macOS arm64 dirty hygiene |130.75 /93.43 /31.38 |206 checks, sole allowed owner-local skip, post passes |
| macOS arm64 ARM |62.09 /33.30 /24.75 |478 images/241 IDs,6 checks, no skips, post passes |
| macOS arm64 Thumb |39.32 /21.16 /16.34 |244 images/126 IDs,5 checks, no skips, post passes |
| native Linux arm64 dirty hygiene |124.46 /109.40 /13.10 |206 checks, sole allowed owner-local skip, post passes |
| native Linux arm64 ARM |61.02 /43.09 /18.41 |478 images/241 IDs,6 checks, no skips, post passes |
| native Linux arm64 Thumb |30.56 /25.87 /8.60 |244 images/126 IDs,5 checks, no skips, post passes |

Final receipt-control time is52.27 seconds on macOS and74.18 seconds on Linux.
All four producer workflows retain their lightweight platform date/budget and
offline controls. Verified exact xPack15.2.1-1.1 installations were reused with
`--check`, not redownloaded. The Linux image remains
`ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`,
with4 CPUs/8GiB, source/tools read-only and independent tracked clones under
`/work`. Provisioning and all three sequential Linux lanes took255.22 seconds;
that interval is not one hosted job duration.

`verify-preflight.py` verifies exact478/244 ownership, absent unowned directories,
regular output files, every actual size/SHA, exact722-image/367-ID union per host,
full JUnit inventories and preserved dirty hygiene inputs. It literally compared
all722 current macOS/Linux ROM pairs, all722 current macOS ROMs with each preserved
58 and99 macOS set, and all722 current Linux ROMs with each preserved58 and99 Linux
set. Every3610 cmp operation returned0. The complete actual local proof retains
admission false. Every standalone exact producer-directory signature scan passes
on both actual host output sets. The seven real scan-witness refusal controls
pass and restore the witness exactly; fresh ARM collection passes afterward.

No assembly, producer recipe, tool/flag/input declaration, CPU/runner behavior,
manifest/ROM pin, license or private content changed. Producer digest remains
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.
Final focused CI/routing/metadata checks pass42/42. Normal CI/ASan still register
355/97 executable tests plus post; their unaffected full runtime suites were not
repeated solely for timing. The four-log Python collector's synthetic baseline
and17 refusal controls pass at the final source; no synthetic result is hosted
evidence.

## Remaining publication boundary and handoff

Final local workflows are within180 seconds. This is local evidence only.
Historical hosted fetch took16 seconds; a10-second total checkout/setup/cleanup
allowance is an estimate. Adding those26 seconds to the four current local
producer workflows gives88.09/65.32/87.02/56.56 seconds. Hygiene has no tool fetch;
its actual hosted setup and cleanup still count. Neither this arithmetic nor
historical timings establish current hosted success. Every actual required job
must finish green within180 seconds from live API timestamps.

The updated ignored `build/phase05-ci-split/PUBLICATION-CHECKPOINT.md` binds the
four-log collection/validation commands to exact c3 and current local proof.
`expected-host-hashes.json` contains actual expected local hashes, distinct from
observed hosted hashes. Prior source99/e3/f7 expected sets and the original
handoff remain preserved. The parent exclusively owns `approval.json`, publication and live
monitoring. No attempt was used/reset here, and a new hosted red stops fixes and
retries until owner renewal. Historical failed run43450350277 remains red.

No implementation stub or required unrun local check remains. Test-only Git
snapshots introduce a bounded local file-copy boundary protected by the explicit
tracked regular-path whitelist; no network/auth endpoint or new external trust
boundary was added. No cache, dependency, concurrency setting, job limit or
additional job was introduced by this review fix. Existing owner AGENTS/config/
state/cache/untracked notes, shared planning records, stop records and private
workspace content were preserved.

## Self-check: PASSED

All three review-fix source commits are ancestors of c3; the three changed source
paths and this report exist. The final source marker, six full JUnit/post results,
both dirty-input byte witnesses, all group sets,3610 literal comparisons, six
standalone scans, seven witness refusals/restoration,42 focused checks, complete
registration inventories, collector controls and exact-source publication handoff
were checked on disk. Fresh whitespace, tree/staged hygiene and signature checks
passed before commits. This report remains uncommitted; parent independent
re-review is next, not owner input or Plan21 completion.
