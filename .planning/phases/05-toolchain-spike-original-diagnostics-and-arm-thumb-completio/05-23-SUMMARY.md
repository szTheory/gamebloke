---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "23"
subsystem: testing
tags: [diagnostics, admission, inventory, evidence, cmake]
requires:
  - phase: "05-22"
    provides: Reviewed native CPU and mutation/refusal witnesses
provides:
  - Complete registered admitted production and native API receipt sets with exact inventories
  - Specific optional absence, required execution and dependency/BIOS refusal controls
  - Renewed immutable producer proof and reviewed consumer-only metadata admission
affects: [05-24, 05-25, diagnostic-ci]
tech-stack:
  added: []
  patterns: [cheap source and policy preflight, separate producer and consumer identities, explicit isolated synthetic controls]
key-files:
  created: [tests/diagnostics.cmake, cmake/DiagnosticMatrix.cmake, cmake/CiPreflight.cmake, cmake/CiJobBudgets.json, scripts/collect-hosted-proof.py]
  modified: [CMakeLists.txt, tests/toolchain.cmake, tests/gates/diagnostic_setup.cmake, tests/fixtures/manifest.json, .github/workflows/ci.yml, docs/verify.md]
key-decisions:
  - "Reuse complete unchanged producer proof for consumer-only corrections while binding actual tested consumer identity separately."
  - "Run cheap exact policy/source/inventory checks before expanded gates; preserve concrete failures and affected-boundary retests."
  - "Owner approved300seconds only for macOS native build and ARM diagnostics; other nine jobs remain180seconds; one renewed hosted attempt consumed successfully."
requirements-completed: [DIAG-01, DIAG-03, VERIF-04, CPU-01, CPU-02]
actuals:
  tokens: 648061
  tasks: 2
  commits: 6
commits: 6
plan_head_before: 426af7e
plan_head_after: bb9c24d
duration: "multi-resume; final admitted main119.88s, canonical native workflow185.02s; full plan duration not reconstructed"
completed: 2026-11-14
status: complete
coverage:
  - id: D1
    description: All production cases and native API assertions have actual admitted identity-bound receipts and exact inventories
    requirement: DIAG-01
    verification:
      - kind: integration
        ref: "build/phase05-plan23-admitted/identity-and-results.json;793 admitted tests and strict post,722results/19APIrows"
        status: pass
    human_judgment: false
  - id: D2
    description: Optional absence and required execution/refusal paths cannot silently skip or manufacture completion
    requirement: VERIF-04
    verification:
      - kind: integration
        ref: "05-23-ADMITTED-EXECUTION.md; dependency,receipt,pipeline,proof-stage and missing-tool controls"
        status: pass
    human_judgment: false
  - id: D3
    description: Current immutable producer outputs match full local literal and distinct hosted hash witnesses under exact job budgets
    requirement: DIAG-03
    verification:
      - kind: integration
        ref: "05-23-JOB-BUDGET-HOSTED.md; actual run43560267506 and committed final-hosted-proof validator"
        status: pass
    human_judgment: false
---

# Phase 05 Plan 23: Inventoried Admitted Diagnostics Summary

Complete production diagnostics now execute through the canonical checker with
exact receipt sets, native API assertions, visible optional absence and mandatory
execution controls. Independent targeted review is clean.

## Accomplishments

All722 production fixtures retain their exact output hashes and rights while
binding renewed producer85c/input95. Actual admitted results are355PASS,
355deliberateGUEST_FAIL and12namedstops for367guestIDs;19native API rows retain
separate actual assertion evidence. The full admitted lane and strict post pass
with no skips. Canonical native CI passes with specific permitted optional
skips. Focused AppleClang ASan/UBSan CPU, metadata, relocation and dependency
checks pass without findings. The [execution report](05-23-ADMITTED-EXECUTION.md)
contains commands, timings, concrete integration failures and bounded retests;
[review](05-23-ADMITTED-REVIEW.md) has no findings.

The complete preserved r4 source proof records both local hosts, all722literal
comparisons,1444imageaudits and4332current/historical literal comparisons in
separate fields. Actual hosted run43560267506 atconsumer56c passed all11jobs
within exact approved budgets. Four actual log-derived grouped witnesses and
raw API documents passed the collector and committed final validator. The
[hosted report](05-23-JOB-BUDGET-HOSTED.md) separates this hosted hash evidence
from local literal equality and later local admitted execution. No prior receipt
was relabeled. Consumer corrections were tested as the saved56c diff committed
inbb9c24d; hosted56c does not claim those later changes ran remotely.

## Task commits and deviations

Source/registration and renewed proof work: d128c40,5b71c0a,41931a2,85c361e.
Exact reviewed job-budget/source binding policy:56c2213. Metadata readmission
and admitted consumer corrections:bb9c24d. These are two tasks across multiple
source/proof/review cycles, not invented separate historical RED commits.

Initial registration/proof-isolation RED and review history remain in
[the candidate report](05-23-CANDIDATE.md), [initial review](05-23-REVIEW.md),
[census repair](05-23-CENSUS-REPAIR.md), and budget policy/fix/review reports.
Measured JSON/parser and redundant authority costs required reviewed producer
changes and genuinely renewed proof. Historical hosted failures and exhausted
approval ledgers remain visible. Owner renewed only the concrete targeted300s
policy and one hosted attempt; that attempt succeeded and is consumed.

Final admitted integration exposed three consumer defects: proof-only final
registration, ISA output paths and early native API source-pin emission. Each
concrete failure is retained; corrected narrow controls and full integration
pass. WINDOWS entry15 records their resolution. No producer input changed in
these fixes. Existing Plan19 historical TDD deviation remains open; current
checks cannot fabricate that history.

## Verification and limits

Fresh parent audit rechecked all fixture identities, current receipt sets,
producer bytes/modes/ancestry and saved tested-file hashes before commit.
Repository hygiene/signature scans pass. A report's nonsecret receipt-token
example initially tripped the hygiene assignment pattern; prose was corrected
without weakening the gate. No binary content was committed or uploaded.

No BIOS execution, physical accuracy, commercial-game/device compatibility,
release or installed admitted-package qualification follows. Shared requirement
completion remains gated by later plans. Ready for Plan24's tracked admitted
preset/workflow switch and Plan25 final integration. No Phase6 advance or
additional hosted attempt is authorized here. Owner action is not needed for
continued local work. Phase5 acceptance and merge-provenance choice remain the
explicit later owner boundary.

## Self-Check: PASSED

Evidence artifacts and reviewed files exist; actual saved consumer hashes match,
strict inventories and producer checks pass, and preserved r4 digests remain
unchanged. Summary records existing evidence rather than asserting fresh hosted
qualification of a later commit.
