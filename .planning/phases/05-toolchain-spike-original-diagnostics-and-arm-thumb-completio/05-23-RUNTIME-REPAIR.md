# Plan23 bounded consumer runtime repair

Status: locally verified review candidate; uncommitted. Plan23, source freeze,
readmission and hosted qualification remain incomplete. The parent owns review,
the next freeze/readiness decision and the single authorized hosted attempt.

All measurements below are Darwin arm64 with CMake4.4.3 and AppleClang21.0.0.21000101.
They measure verification cost, not emulator throughput or hosted Linux-x64 cost.
Heavy probes ran serially except the explicitly excluded overlapping sample.
Existing pinned ARM tools were checked and reused; no tool installation occurred.

## Measured changes

The same722-row canonical manifest and four-row synthetic control authority were
used for the runner comparison. The original runner subset already passed its
positive and negative contracts. This is a performance repair with added refusal
controls, not a manufactured behavioral RED.

| Scope | Before wall / user / system seconds | Candidate wall / user / system seconds |
| --- | ---: | ---: |
| Runner result-contract plus metadata subset |70.46 /66.07 /1.72 |29.24 /26.07 /1.15 |
| Result-contract CTest within that subset |43.78 / not isolated / not isolated |22.31 / not isolated / not isolated |
| Metadata CTest within that subset |26.64 / not isolated / not isolated |6.87 / not isolated / not isolated |
| Exact ARM receipt set, isolated command |10.10 /8.37 /1.47 |4.82 /3.55 /0.76 |
| Final ledger clean and coordinated negative controls, isolated command |52.38 /41.78 /0.66 |23.88 /20.98 /0.35 |
| ARM setup, saved original script versus candidate CTest setup |75.44 /64.85 /7.10 |60.91 / not isolated / not isolated |

The complete candidate ARM run independently measured setup60.91seconds,
receipt set4.15 and final coverage21.72. The saved original setup ran in the same
isolated source/build directory immediately before the candidate suite. Its
subprocess observation consumer had the same canonical validation behavior;
the duplicate setup authority load remained present in that original script.

One earlier final-coverage candidate sample recorded69.16wall/21.80user/0.46system
seconds while hygiene overlapped its end. Its wall time is inconclusive and is
excluded from the comparison. The later isolated23.88-second command and the
serial21.72-second integration result are retained separately. No host scheduling
or whole-machine cause is asserted from the overlap.

## Implementation and preserved checks

| Files | Change and preserved contract |
| --- | --- |
| `tests/gates/runner_case.cmake`, new `runner_result_batch.cmake` | Queue every existing actual-producer result document into one CMake invocation. Strictly load vocabulary, registry and each manifest once; call the unchanged canonical result validator for every document. Delimited per-case reports still require zero findings for acceptance, or findings plus the exact expected reason for refusal. Each authority's bytes are rehashed before reuse and at the end; changed bytes and symlink replacement have clean companions and mandatory refusal controls. Existing independent `result.nc` script-mode controls retain nonzero-exit and reason assertions. CLI status/usage exits, bounds, semantic, timing, dispute, one-line output, trace, file-read and ROM-size checks remain in the original runner matrix. |
| `cmake/GenerateFixtureIndex.cmake` | Check the small synthetic authority and actual image digest before loading the independent large production manifest. A known bad synthetic image now refuses immediately. Every successful generation path still runs the same complete canonical manifest validator before headers are written. Malformed-manifest, stale-image, missing artifact-pin/legacy runner and source/index/wiring controls remain mandatory. The metadata test's complete722-row ID/digest reconciliation is unchanged. |
| `tests/gates/diagnostic_setup.cmake`, `diagnostics_case.cmake`, new `diagnostic_result_authority.cmake` | Setup and its included raw-observation consumer share the same strictly validated canonical prefixes within one invocation. Function scope contains observation temporaries. Source identity and all three authority-file hashes are checked on reuse and at completion. Manifest, registry, vocabulary and symlink changes have explicit refusal controls. Every real raw image still executes the runner, canonical result validator, semantic/trace checks and original observation controls. Smoke results retain their own execution and refusal checks. Full observation output now reaches the enclosing CTest log directly; each existing group log contains its exact checked summary, and per-image checker/trace files remain present. |
| `tests/gates/diagnostic_receipt.cmake`, `diagnostic_receipt_controls.cmake` | Compute actual manifest/producer-input/runner/native hashes once per invocation, compare every receipt's original identity fields, then re-read actual identity after the full guard walk. Every case still rehashes its own result and ROM, validates contained paths, exact semantic ID/state, stop reason, execution/budget and admitted/proof identity. Token, source pin, stale authority, changed bytes, missing files/receipts/setup, incomplete set, required skip, wrong stage, native assertions and exact stop-reason controls remain. |
| `tests/gates/coverage_case.cmake` | Reuse row-location indices built during the existing inventory walk, while checking the literal ID on lookup. Project the exact canonical fixture-ID sequence once for the two final checker invocations. The unchanged final checker only consumes that sequence for manifest links; production index generation separately validates all rights/digest metadata. Order, duplicates and count remain in the projection, and an independent terminal comparison confirmed all722 IDs literally match. The canonical manifest hash is checked around use. Both real clean final validation and all coordinated semantic/stop/API/pending/missing-link refusals remain mandatory. |

No producer input, assembly, ledger, strict parser, fixture pin, inventory,
workflow job, timeout, concurrency setting, dependency or sanitizer flag changed.
No persisted authority cache was added. All reuse ends with the current process.
The output-ID projection is a disposable test input, never a canonical manifest,
fixture admission input or production index authority.

## Verification receipts

Ignored evidence lives under `build/phase05-plan23-runtime-repair/`:

- `baseline/runner.log`, `runner-time.log`, and `runner.xml`: original runner
  positive/refusal matrix and metadata baseline.
- `baseline/arm-raw-time.json`, `arm-guard-time.json`,
  `final-coverage-time.json` and adjacent logs: original full-scale observations.
  ARM raw baseline checked236 semantic pairs and four named stops,44.35wall/
  40.43user/2.34system seconds.
- `candidate/runner.log`, `runner-time.log`, and `runner.xml`: focused repair
  comparison with all existing producer contracts.
- `candidate/focused-times.json`, `focused-driver.log`, `arm-tests.log`,
  `arm-post.log` and `ci.log`: serial fresh ARM build, original setup comparison,
  complete candidate ARM tests/inventory and canonical CI workflow.
- `candidate/terminal-profile-times.json` and `terminal-profile-driver.log`:
  isolated post-integration guard/coverage CPU and wall measurements, plus
  retained nonzero/refusal logs for all six new snapshot controls. Each snapshot
  control first establishes an unchanged clean companion.
- `candidate/consumer-digests.json`: exact digests of the nine code files, equal
  between the working tree and isolated ARM integration source.
- `candidate/terminal-checks.json`: producer digest/mode check, independent ID
  sequence comparison, literal rebuilt output comparison and JUnit census.

The fresh ARM build and full candidate suite passed. Its inventory reconciled
549 expected,549 executed, zero skipped, plus the post audit. All236 ARM semantic
pairs, four ARM stops, smoke pair, registered native tests and19 API boundaries
were retained. Compile/link/header/map/scan failures, mixed skip/failure markers
and source/receipt controls all passed their expected refusal checks.

`cmake --workflow --preset ci` passed after the final code changes, including
the post inventory:1106 expected,381 executed and725 explicitly allowed skips
(724 optional diagnostic absence checks and one owner-local check). The optional
tool root remained absent for this lane. This is not execution of all canonical
fixture pins or readmission evidence. The owner-local check skipped; no private
ROM or BIOS content was accessed by this repair.

| Complete candidate local command | Wall / user / system seconds |
| --- | ---: |
| Fresh ARM configure |4.52 /3.05 /0.49 |
| Fresh ARM build, including source diagnostic build |91.04 /65.92 /33.59 |
| Complete ARM CTest |131.43 /94.92 /21.39 |
| ARM post inventory |0.14 /0.10 /0.02 |
| Canonical CI workflow in the existing local build tree |303.82 /192.28 /56.58 |

These complete-command observations do not establish a hosted180-second budget.
The local fresh ARM workflow totaled approximately227.13seconds. Runtime varies
materially from earlier local receipts; no cross-platform scaling factor or
margin is inferred. Historical local/hosted ratios are not a readiness guarantee.

## Source and output boundary

Base HEAD remains `5b71c0ace49c2e91ea5d09ac076bf1c97b332663`; the isolated integration
source has that HEAD plus the exact nine consumer overlays. It is not a newly
frozen source revision. The unchanged producer digest is
`792d96f7e3f4aa1f513a404ae6dc0e379544d6ff4b9fb402dd62360c8ce460c7`.
Forced Git file-mode comparison also found no producer-input change.

All478 newly built ARM/smoke outputs compared literally equal to the preserved
fresh-source-proof-r2 outputs. This is local identity/repeatability evidence,
not new hardware accuracy, compatibility, hosted parity or rights evidence.
Historical canonical fixture pins remain untouched. The production proof index
still has qualification zero and setup still requires refusal before guest
execution when a fixture ID is supplied, even for matching ROM bytes.

No implementation commit or hosted publication was made. Parent-owned planning,
configuration, approval counters, historical proof outputs and stop records were
preserved. No new network, package, authentication or public file-input surface
was introduced; new helpers serve existing verification consumers.

## Required next action

The parent reviews this uncommitted candidate and resolves findings within the
approved repair scope. After a narrow commit, it owns a new source freeze and
the required current-source local proof/readiness assessment, including Linux
and the remaining compiler/sanitizer/Thumb/hygiene boundaries. This task did not
repeat the ten-lane proof matrix and does not supersede the retained hosted stop
with a pass or accept the remaining runtime risk.

Only measured readiness after review can justify consuming the one authorized
hosted attempt. Keep all existing checks and180-second job caps. A new hosted red
stops fixes/retries again. A hosted source-proof green would still leave fixture
readmission and required admitted execution before Plan23 completion. No user
action is needed for the parent to continue review and local readiness work.

## Independent review closure and timing context

Independent review found one request-serialization portability defect, now fixed:
all values escape quotes, backslashes and non-NUL control bytes before JSON SET.
A real quote/tab filename control failed through the batch authority path before
the fix and passed afterward in the full runner contract test (19.45seconds).
A separate exact-byte roundtrip checks backslash and all non-NUL C0 characters.
An earlier CMake backslash directory-setup failure is retained as a setup failure,
not behavioral RED. See [the resolved review](05-23-RUNTIME-REVIEW.md) and ignored
`build/phase05-plan23-runtime-repair/review-fix/`.

A read-only shared-machine snapshot subsequently showed substantial unrelated VM,
other-project test and compiler activity. No unrelated process was changed.
This establishes a contended timing context, not a quantified slowdown or hosted
budget excuse. Complete frozen-source readiness remains the next gate. No hosted
attempt has been consumed under the new one-attempt renewal.
