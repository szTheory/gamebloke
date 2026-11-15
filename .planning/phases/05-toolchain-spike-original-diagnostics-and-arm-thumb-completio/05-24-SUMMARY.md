---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "24"
subsystem: testing
tags: [admission, ci, source-authority, timing]
requires:
  - phase: "05-23"
    provides: Admitted exact fixture and API execution sets
provides:
  - Mandatory admitted presets and full-history source authority before builds
  - Current actual hosted execution and exact per-job budget evidence
  - Cheap cold smoke graph controls before corpus work
affects: [05-25, final-verification]
tech-stack:
  added: []
  patterns: [separate producer and consumer identity, cheap controls before long gates]
key-files:
  created: [tests/ci/admitted_hosted_cases.py, tests/ci/admission_cases.cmake]
  modified: [CMakePresets.json, .github/workflows/ci.yml, tests/toolchain.cmake, cmake/CiPreflight.cmake, cmake/CiJobBudgets.cmake, cmake/CiJobBudgets.json, docs/verify.md]
key-decisions:
  - "Required final lanes cannot accept proof-stage or shallow-history escape."
  - "Use actual current grouped receipts and every required job, not passed steps inside cancelled jobs."
  - "Owner approved240s for Linux native only; existing macOS300s exceptions and other180s limits remain exact."
requirements-completed: [DIAG-01, DIAG-02, DIAG-03, VERIF-04]
duration: "multi-resume; complete duration not reconstructed"
completed: 2026-11-14
status: complete
coverage:
  - id: D1
    description: Required admitted execution rejects proof-stage and stale source/history identities
    requirement: DIAG-01
    verification:
      - kind: integration
        ref: "tests/ci/admission_cases.cmake; tests/gates/diagnostic_dependency.cmake; 05-24-LOCAL-REVIEW.md"
        status: pass
    human_judgment: false
  - id: D2
    description: Complete current hosted original diagnostics and exact budgets pass actual receipt validation
    requirement: DIAG-03
    verification:
      - kind: integration
        ref: "05-HOSTED-CHECKS.md; actual run43623595995 and final-hosted-proof"
        status: pass
    human_judgment: false
---

# Plan24: Required admitted diagnostic CI

Required final presets execute admitted original diagnostics and reject proof
escape, missing required tools and shallow source authority. The actual hosted
consumer35c7513 passes all eleven jobs and committed admitted validation.

## Implementation and verification

The primary admitted/history switch is8e21ef6. Later reviewed consumer2fb31ff
fixes cold admission-smoke production/consumption and removes a measured second
manifest parse without caching successful validation. Independent review found
and closed the initial weak graph guard using actual graph mutations and clean
companions. The budget-only35c7513 applies the owner's Linux native240s policy;
its review caught and fixed the recurring boundary test's old180/300 assumption.
No producer, runtime C or fixture pin changed in those consumer corrections.

Canonical local CI, required admitted execution, policy/history/refusal controls,
actual-log collector controls and final hosted validation have terminal evidence.
The full hosted result, exact timings, actual guest/API sets and preserved failed
runs are in [05-HOSTED-CHECKS.md](05-HOSTED-CHECKS.md). Each host reports355PASS,
355deliberateGUEST_FAIL,12named stops and19separate native API rows. Exact local
literal equality and hosted hash witnesses retain separate meanings.

Source/helper and collector controls were authored and observed before integration;
actual failure and bounded fix histories are retained in05-24-REVIEW-FIX.md,
05-FINAL-CONSUMER-REPAIR.md and05-LINUX-NATIVE-BUDGET240-REVIEW.md. The original
Plan19 historical test-first gap is not reconstructed or closed by this work.

## Publication and limits

The metadata event is already published. Its failed Linux native timing result
is historical and resolved by the explicitly renewed current green. One unused
final-records event remains, with a new red stop and no retry credit. This summary
records implementation/automated outcomes; it does not mark shared requirements
or the phase accepted. Final documentation-descendant CI and owner guide/merge
judgments remain separate. No merge, main proof, release or Phase6 occurred.

## Self-check

Current hosted raw documents/logs, collected proof and terminal validator exist;
all exact budgets and source/output identities are checked. Producer85c/input95
and preserved R4 evidence remain unchanged. The final records run is pending
publication, not inferred from this implementation-head result.
