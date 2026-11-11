---
phase: "05"
slug: "toolchain-spike-original-diagnostics-and-arm-thumb-completio"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-11"
---

# Phase 05 — Validation Strategy

## Test Infrastructure

Existing CTest/C17 assertion harness, strict CMake validators, POSIX controls, sorted per-lane inventories and JUnit reconciliation. No new testing framework or Python runtime.

| Property | Value |
| --- | --- |
| Config | CMakeLists.txt, CMakePresets.json, tests/core.cmake, tests/gates.cmake |
| Quick command | Task-specific created selector or script with --no-tests=error when using CTest; planner must name exact targets after creation ordering is set. |
| Full command | cmake --workflow --preset ci |
| Sanitizers | Existing fresh AppleClang compiler override locally; canonical Linux clang-21 asan workflow in hosted CI. |
| Measured baseline | Final Phase 4 main jobs: macOS140s, Linux129s, ASan119s, hygiene48s. These are not Phase 5 measurements. |
| Resource limit | Existing hosted job timeouts remain180s; spike measures tool install/build separately before CI placement. |

## Sampling Rate

After each production tracer/change, run the exact new behavior plus a relevant negative/clean control. Do not build once per opcode/vector or repeat full suites for unchanged documents. Run canonical CI at integration boundaries and final source; sanitizer/current compiler evidence when host C changes. All checks background by default. No watch mode. A missing check or skipped case remains visible.

## Per-Task Verification Map

Pending ordered plans. Planner owns replacing this paragraph with every actual task ID/wave/requirement/threat/test command and creation dependency before plan-check. No nonexistent target is claimed run. Each task leads with a real production tracer and has its own observable test; all six requirements below must be accounted for.

| Requirement | Behavioral verification to author | Failure control |
| --- | --- | --- |
| DIAG-02 | First real original toolchain smoke image: exact pins/components, ARMv4T object+ELF tags/disassembly/map, generated-ROM signature scan, timed fresh installation and exact macOS/Linux byte comparison before digest admission. | Bad pin/architecture/runtime/map/signature/redirect/extraction; cross-host mismatch remains honest fallback, not equality. |
| DIAG-01 | Build original ARM/Thumb source with owned crt0/link/header; exact source/tool/artifact authority; real production CLI result. | Stale source/output/index, modified bytes, linked runtime/map input, signature mutation and present-tool mandatory guard. |
| DIAG-03 | Stable nonzero per-case mailbox IDs, each positive and real assertion-broken variant executes through production runner and checker. | Exact GUEST_FAIL/state/ID, not fabricated mailbox or identity refusal before execution. |
| VERIF-04 | Explicit local tool dependency SKIPPED in JUnit/inventory, real declared-BIOS SKIPPED/ran=false; required CI tools/guests cannot skip. | Simulated missing dependency with named reason plus mandatory-present guard and unusable-tool failure. |
| CPU-01 | Complete ARM family/form ledger; all named CPU-1..9 quirk assertions, defined flags and explicit unpredictable policies; whole-unit transaction bounds. | Selected real semantic mutants, one-short/exact/overflow/max-list/bank/PC/refill/trace capacity tests with non-default complete state. |
| CPU-02 | All19 Thumb formats and forms, BL halves separate, real API IRQ boundary after explicitly unmasking, no v5 interwork semantics. | BL fusion, incorrect bank/PC/flags/memory cases, separate retirement/trace and exact mailbox IDs. |

## Wave 0 Requirements

Existing infrastructure already runs. New test files/selectors/gates are created with their owning production tracers and must be referenced by dependent verification only after creation. Planner fills concrete dependencies; no up-front stubs or future tests are represented as passing.

## Manual-Only Verifications

No new subjective/hardware/rights approval is invented during discussion. Current task scope covers original owner-controlled diagnostics and necessary public-CI execution only. Exact content beyond it requires a concrete owner choice. Physical qualification, broad compatibility, BIOS execution, release and project license remain outside this phase. Actual tool/parity/timing experiments are automated work still pending, not human acceptance.

## Probe/Prohibition Boundaries

Run the installed spec-less edge protocol against the six exact requirement texts; retain its report in 05-PROBE-STATUS.json. Defensible explicit resolutions require authored acceptance criteria and actual verification; unclassified/unresolved items remain flagged. Descriptor-less project prohibitions remain unverified as required; no check or owner waiver is fabricated. The historical Phase 4 eleven rows remain unchanged.

## Validation Sign-Off

- [ ] Actual per-task map and automated commands/creation dependencies complete
- [ ] Every production tracer verified before expansion
- [ ] All missing target references have explicit creation dependencies
- [ ] No watch mode or silently permitted required-CI skip
- [ ] Tool/time/cross-host witnesses measured and bounded
- [ ] Final current inventories and meaningful negative controls pass
- [ ] nyquist_compliant true only after actual audit

**Approval:** pending implementation and audit; no Phase 5 test has run.
