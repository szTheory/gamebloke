---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T18:00:23Z
depth: standard
files_reviewed: 13
files_reviewed_list:
  - .github/workflows/ci.yml
  - CMakeLists.txt
  - CMakePresets.json
  - cmake/GblTests.cmake
  - tests/ci/ci_cases.cmake
  - tests/gates.cmake
  - tests/gates/synthetic_diagnostic_controls.cmake
  - tests/gates/final_diagnostic_proof.cmake
  - tests/gates/toolchain_spike.cmake
  - tests/inventory/diagnostics-thumb.txt
  - tests/inventory/diagnostics.txt
  - tests/inventory/hygiene.txt
  - tests/toolchain.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: CI Split Code Review Report

**Reviewed:** 2026-11-13T18:00:23Z
**Depth:** standard
**Files Reviewed:** 13
**Status:** clean

## Summary

The original hygiene warning is resolved. The replacement control snapshots only tracked, regular files from the committed producer-path whitelist plus the current parser scripts, verifies the copied bytes, and runs the current parser from the isolated snapshot. The real producer and hosted-proof paths retain their dirty-input rejection. The receipt validator's new metadata-first ordering still reaches the existing full receipt checks for every accepted hosted and local receipt. No active findings remain. The recorded local preflight is not hosted proof; Plan 21 admission remains gated on the parent-owned hosted attempt.

## Narrative Findings (AI reviewer)

No active findings after the scoped re-review at `c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`.

## Resolved Findings (review history)

### WR-01 — Hygiene rejects uncommitted diagnostic input edits (resolved)

**File:** `tests/gates.cmake:2-8`; `tests/gates/final_diagnostic_proof.cmake:92-105,847-862`
**Original severity:** WARNING
**Original issue:** The synthetic receipt control ran in local hygiene and called `proof_inputs()`, which rejected any modified tracked producer input. Editing a listed diagnostic input therefore made the hygiene workflow fail before completing.
**Resolution:** `tests/gates.cmake:2-8` now invokes `tests/gates/synthetic_diagnostic_controls.cmake`. The wrapper enforces the committed input whitelist, verifies current tracked regular files and copied digests, then runs the current parser scripts from a disposable synthetic Git snapshot. The real `proof_inputs()` freshness check is unchanged. Recorded current-source controls show dirty-input hygiene passes while the real producer and hosted validator still reject that same dirty input.

## Re-review Notes

The validator checks live run/job identity and host/group/source metadata before deriving the trusted matrix sets. It then calls the unchanged `proof_host_receipt()` for all four hosted subsets and both complete local receipts, preserving input freshness, archive pins, exact names/IDs, output hashes/sizes, full unions, and local-to-host comparisons. Malformed receipts may now fail on an earlier metadata reason; accepted receipts still pass every original check. The exact-source local preflight and focused-control evidence were inspected; no hosted attempt or fixture admission is claimed.

---

_Reviewed: 2026-11-13T18:00:23Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
