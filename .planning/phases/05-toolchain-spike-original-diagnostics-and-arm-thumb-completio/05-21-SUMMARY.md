---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "21"
subsystem: testing
tags: [armv4t, diagnostics, fixtures, provenance, cmake, cross-host]
requires:
  - phase: "05-20"
    provides: Final original Thumb sources and pre-admission observations
provides:
  - Complete actual cross-host proof for722 images and367 source-built case IDs
  - Immutable production admission of355 positive/broken pairs and12 named stops
  - Independent source/output/pin/generated-index refusal controls
affects: [05-22, 05-23, 05-24, 05-25, fixture-admission, ci-budget]
tech-stack:
  added: []
  patterns: [source-only authority followed by consumer admission, canonical parsed-row reuse]
key-files:
  created:
    - cmake/CheckDiagnosticAuthority.cmake
    - tests/gates/final_diagnostic_proof.cmake
    - tests/gates/diagnostic_admission.cmake
    - tests/gates/diagnostic_admission_controls.cmake
  modified:
    - tests/fixtures/manifest.json
    - cmake/CheckManifest.cmake
    - cmake/GenerateFixtureIndex.cmake
    - tests/gates/diagnostics_case.cmake
    - tests/gates/artifact_case.cmake
    - tests/gates/runner_metadata.cmake
    - diagnostics/armv4t/inputs.txt
    - cmake/BuildArmDiagnostics.cmake
    - .github/workflows/ci.yml
key-decisions:
  - "c3af6de is the actual source-only authority; descendant consumer commits may retain its pins only while every producer input remains identical."
  - "All actual host output bytes agree; no fallback or host-specific fixture variant is needed."
  - "Parent owns remediation/publication assessment for the measured253.63-second local CI cost; producer inputs and pins remain unchanged."
requirements-completed: [DIAG-01, DIAG-02, DIAG-03]
actuals:
  tokens: 270455
  tasks: 2
  commits: 16
commits: 16
plan_head_before: 6ce322caacc1e482703c84d0ce116d1503a4025a
plan_head_after: 6a16350645d146c884d874f98a85298a3f0818e1
duration: "Multiple resumes; complete plan duration not reliably measured; Task2 RED-to-code-commit22m43s"
completed: 2026-11-13
status: complete
coverage:
  - id: D1
    description: Complete local byte comparison and actual hosted hash witness before admission
    requirement: DIAG-01
    verification:
      - kind: integration
        ref: "05-21-HOSTED-GREEN.md; final-hosted-proof actual run43478622357"
        status: pass
    human_judgment: false
  - id: D2
    description: Every admitted positive, broken companion and named stop executes through immutable production metadata
    requirement: DIAG-02
    verification:
      - kind: integration
        ref: "diagnostics_case.cmake CASE=all-admitted; build/phase05-all-admitted-final.log"
        status: pass
    human_judgment: false
  - id: D3
    description: Independent stale source, output, pin and generated-index refusals retain clean companions
    requirement: DIAG-03
    verification:
      - kind: integration
        ref: "artifact_case.cmake CASE=admission-controls; build/phase05-admission-controls.log"
        status: pass
    human_judgment: false
---

# Phase 05 Plan 21: Cross-host Proof and Production Fixture Admission Summary

**All722 final original diagnostic images have actual cross-host evidence and immutable production bindings;355 semantic pairs and12 named guest stops pass their distinct contracts.**

## Accomplishments

Task1's final source-only authority is
`c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`, with producer-input digest
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.
The final local proof literally compares all722 corresponding files between
actual macOS arm64 and native Linux arm64 builds. Separate actual hosted Linux
x64/macOS hash receipts match every expected name, case ID, size and digest.
All eleven jobs in [run43478622357](https://github.com/szTheory/gamebloke/actions/runs/43478622357)
are green within180 seconds (maximum128 seconds). See the
[hosted report](05-21-HOSTED-GREEN.md),
[split report](05-21-CI-SPLIT.md) and
[review-fix report](05-21-CI-SPLIT-REVIEW-FIX.md) for actual job associations,
source checks, scan/map/disassembly controls and the earlier renewal history.
Earlier failed/cancelled runs remain historical failures. No synthetic receipt
is real hosted evidence, and no ROM bytes were uploaded.

Task2 admits exactly722 distinct fixture IDs at that authority: smoke1 pair,
ARM236 semantic pairs plus4 stops, and Thumb118 semantic pairs plus8 stops.
Both members of each semantic pair retain the same exact nonzero ledger case
ID. The nineteen ARM API-only witnesses remain lower-boundary observations.
Every row separately pins source, canonical recipe/input digest, canonical
Linux x64 tool archive, output path/SHA, scoped NOTICE/public-CI use, BIOS-none
mode and project-authored expectation. All host bytes agree; no fallback or
runtime host-pin override is used. The complete
[admission record](05-DIAGNOSTIC-ADMISSION.md) explains these boundaries.

Every admitted image runs through production `gamebloke-run --fixture` and the
canonical `CheckResult.cmake` function. Separate assertions establish exact
status, reason, termination, semantic state/case ID, nonzero bounded guest
progress and equal loaded/index/claim digests. Positives report PASS, broken
companions GUEST_FAIL, and stops their exact named GUEST_STOP with pending
mailbox semantics. No stop witness is relabeled mailbox PASS.

Source ancestry/freshness is checked before index projection. A later consumer
HEAD may differ from the source pin; each pinned producer input must still have
its exact bytes/mode. Independent source-input, output-byte and artifact-pin
changes refuse before emitting indexes and remove stale headers. A production
runner compiled with a wrong generated digest returns
`REFUSED/fixture-digest-mismatch`, `ran=false`, zero cycles and zero instructions;
the restored clean production companion passes.

## Verification Evidence

All commands ran locally in the background. Times are wall/user/system seconds;
they describe these commands, not emulator throughput. Ignored build receipts
remain local evidence, and the hosted URL/report is the publication witness.

| Check | Actual result | Seconds | Local evidence |
| --- | --- | --- | --- |
| New all-admitted contract before implementation | Exact missing-production-fixture-row:arm-343721884 assertion | Not timed | Initial live RED; repeated at isolated e1628a7 in `build/phase05-admission-red.log` |
| Representative admitted ARM pair | Production PASS/GUEST_FAIL, same exact case ID, canonical checks |33.68 /32.39 /0.82 | `build/phase05-admission-pair.log` |
| `cmake --preset ci`; `cmake --build --preset ci` | Configure/build pass; also included in full canonical workflow below | Included below | `build/phase05-admission-ci.log` |
| `cmake -DCASE=all-admitted -DSRC=. -DBIN=build/ci -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake` |722 production results:355 PASS,355 GUEST_FAIL,12 exact named stops; fresh pinned producer rebuilds |133.52 /66.23 /34.50 | `build/phase05-all-admitted.log` |
| Same all-admitted command after final authority assertions | Same722 results; existing current producer roots reused |78.35 /70.15 /4.10 | `build/phase05-all-admitted-final.log`; `build/ci/diagnostics-results/all-admitted/summary.txt` |
| `cmake -DCASE=admission-controls -DSRC=. -DBIN=build/ci -P tests/gates/artifact_case.cmake` | Independent source/output/pin/index refusals; clean production companions pass |33.80 /18.41 /1.84 | `build/phase05-admission-controls.log`; `build/ci/artifact-case/admission-controls/summary.txt` |
| Artifact `compatibility-and-controls` | Legacy/absent metadata, stale-index cleanup, source/result pin, shape and symlink escape checks pass |1.55 /1.25 /0.21 | `build/phase05-artifact-compatibility.log` |
| `cmake --workflow --preset ci` | All required checks and post inventory pass;355 expected,354 ran,1 explicitly permitted owner-local skip |253.63 /208.28 /24.56 | `build/phase05-admission-ci.log`; `build/ci/junit.xml` |
| Independent Python binding check | All722 manifest pins equal final local proof and actual loaded artifact SHA; control smoke row unchanged; input digest unchanged | Not benchmarked | Final tool output; actual files under `build/ci/generated/diagnostics/armv4t/` |
| `git diff --check` | Pass | Not timed | Current task diff |

The canonical result function consumes the same parsed manifest/registry/vocab
once per gate, validating each actual result independently. This avoids
reloading722 fixture rows for every ROM. The index generator likewise projects
the canonical validator's parsed row objects. No alternative schema, dependency,
synthetic production authority or result-check bypass was introduced.

## Task Commits

Task1 spans resumed source/proof, review fixes and bounded CI renewals:

| Commit | Work |
| --- | --- |
| d1ffd43 | Failing fresh smoke-output contract |
|32d6650 | Source-only complete proof authority |
| f2dc0b9 | Complete proof audits and source-scoped parsing |
|7d7cfbe | Duplicate diagnostic proof work removal |
|2a7999c | Exact matrix scan witness |
|552a3af | Proof-path review closure |
| cadd236 | Bounded CI compilation/parsing repair |
|67ef708 | Actual CI repair/preflight record |
|58deaaa | Renewal and clean repair review record |
| caafe24 | Host/instruction-group split |
|99cbdb2 | Group/source binding |
| e3e889f | Snapshot isolation/freshness controls |
| f7a93f9 | Snapshot controls without duplicate capture |
| c3af6de | Cheap metadata refusal before output parsing; final proved authority |

Task2 commits:

| Commit | Work |
| --- | --- |
| e1628a7 | Failing complete production-fixture contract |
|6a16350 | All proven fixture bindings, production checks, independent refusals and metadata integration |

`actuals.tokens` is the realized committed diff's1,081,819 characters divided by
four and rounded up. The16 commits are measured from the existing on-disk plan
ledger, with before/after heads recorded above. This includes Task1 work and
its already committed records; it excludes this summary's metadata commit and
uncommitted parent/owner files. Complete-plan duration is not reliably measured
across resumes. Task2's measured RED-to-code-commit interval is22m43s, excluding
initial reading and subsequent parent review.

## Deviations from Plan

### Auto-fixed Issues

**[Rule3 - Blocking] Repeated whole-document parsing after fixture expansion.**
`CheckManifest.cmake` validates extracted row objects and exposes them to
`GenerateFixtureIndex.cmake`; diagnostic result loops reuse the canonical
authority and checker function. Before this change the raw per-ROM gate would
reparse the entire expanded manifest for every output. The full canonical suite,
production results and compatibility controls pass with the same schema and
refusal semantics. No producer input was changed for Task2.

**[Rule1 - Bug] Absent-output path resolution emitted one warning per fixture.**
The generator resolves the nearest existing ancestor for metadata-only paths
and retains real-path containment checks. An intermediate empty-suffix error
refused all722 paths; it was corrected before acceptance. The final absent-path
and symlink-escape controls both pass. Source-input comment splitting in the new
consumer guard was also corrected using `file(STRINGS)` after checking the
pinned input-list identity. These setup failures are not claimed as semantic
RED evidence; the actual pre-implementation RED is the missing fixture row.

Task1's owner-authorized CI repair/split renewals and earlier stops are retained
in the linked reports. They are not silently converted into passing runs.

## Deferred Issues and Publication Risk

The complete local canonical workflow passes functionally but takes253.63
seconds after manifest expansion. Its test interval is247.86 seconds;
`json.nc.trailing_content` alone takes100.98 seconds because it loads the large
real manifest for seven tails plus two duplicate no-echo invocations.
`manifest.lint` takes10.78 seconds, `runner.metadata`14.70 seconds and
`json.pos.real_docs`12.26 seconds. This is measured local cost and insufficient
evidence of180-second hosted margin; it is not a new hosted failure.

The parent explicitly owns performance remediation and publication assessment
after this Task2 handoff. No hosted run or publication was attempted here.
`cmake/GblJson.cmake` is a frozen producer input and remains untouched; optimizing
it requires coordinating renewed source/output proof before changing admission
authority. A consumer metadata-compaction experiment reduced bytes but did not
demonstrate faster validation and was reverted. No checks, negative tails,
sanitizers or180-second limits were removed or weakened.

The parent owns shared ROADMAP/STATE/requirements/routing updates, defect-ledger
bookkeeping, independent Task2 review, later phase integration, PR monitoring
and the explicit phase boundary. This executor did not mutate those shared
records or owner-dirty files. The canonical suite's single owner-local skip
remains visible; all722 admitted diagnostic executions themselves ran.

## Limits and Next Action

The source ledger remains386 rows/363 inventory targets with385 pending and one
previously verified smoke entry until later coverage closure. This admission
does not rewrite producer ledger state or claim final ISA coverage, physical
accuracy, official BIOS behavior, device/game compatibility, general binary
redistribution, a project license, release publication or Phase6 acceptance.

No authentication or package-install gate occurred. No production-goal stub,
TODO, skipped admitted fixture or unrun Task2 verification remains. The parent
acts next: review the admission changes, handle the measured CI publication
risk, then continue the remaining authorized Phase5 work. No user action is
needed for the completed local admission task.

## Self-Check: PASSED

All required implementation/admission/summary files exist. All16 recorded
commits are ancestors of HEAD, and the on-disk plan ledger and recorded code
head produce the stated count. The final independent binding check confirms722
manifest/proof/loaded-output identities and the unchanged producer digest.
