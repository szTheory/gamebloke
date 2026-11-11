---
phase: "04"
slug: "cpu-execution-core-and-headless-runner"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 04 — Validation Strategy

## Test Infrastructure

| Property | Value |
| --- | --- |
| Framework | Existing CTest + C17 assertion harness and CMake/POSIX controls |
| Config | CMakePresets.json; tests/core.cmake; sorted lane inventories |
| Quick command | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(cpu\.|runner\.|peek\.|trace\.)' --no-tests=error --output-on-failure` |
| Full suite | `cmake --workflow --preset ci` |
| Sanitizers | Fresh installed-Clang local ASan/UBSan; canonical asan workflow in Linux CI |
| Runtime baseline | Phase 3 hosted main tests about 70–73 seconds; Phase 4 not measured |

## Sampling Rate

- After every task: run its named focused behavioral test and failure condition.
- After each meaningful wave: canonical CI workflow and inventory post check.
- Before verification/publication: final-source CI, fresh complete sanitizer lane,
  hygiene, signature scan and current inventory reconciliation.
- Preserve existing CI limits. Measure new feedback latency; avoid repeated full
  builds for individual vectors and do not invent timing evidence before execution.

## Per-Task Verification Map

The planner must replace this requirement seed with every concrete task id,
plan/wave, unique threat reference, real command and artifact-creation dependency.
No planned test below is represented as having already run.

| Requirement | Secure behavior / acceptance witness | Test type | Creation / execution state |
| --- | --- | --- | --- |
| CPU-03 | Explicit PC/refill/latch state through canonical bus, bounded clock | core + semantic control | planned, CPU tests created by implementation tasks |
| CPU-04 | Isolated banks and precise saved exception state | core + semantic control | planned |
| CPU-05 | Defined arithmetic boundary results and sanitizer enforcement | unit + sanitizer | planned; existing sanitizer controls retained |
| PROF-03 | Named no-BIOS stops before absent vector fetch | core/runner integration | planned |
| RUN-01 | Fresh bounded run, manifest/digest-bound JSON result and explicit refusal/limits | producer + validator | planned |
| RUN-02 | Neutral peek and versioned mailbox, zero/partial state never verdict | unit + integration | planned |
| RUN-03 | Selected categories only, bounded records, explicit output full | unit + integration | planned |
| RUN-04 | Moved-prefix installed CLI matches build-tree result | installed consumer | planned |

## Wave 0 Requirements

Existing infrastructure covers compilation, CTest, inventories and sanitizer
controls. Each plan creates its missing test/source artifact before verification;
the planner records these dependencies rather than using nonexistent commands as
proof. No new test framework or runtime dependency is required.

## Manual-Only Verifications

All required CPU/runner behavior has automated evidence planned. Claim wording
and source/policy distinctions may retain judgment-tier flags under the GSD
prohibition protocol; keep those visible at phase acceptance. Hardware post-BIOS,
physical devices and game compatibility are outside this phase's claim.

## Validation Sign-Off

- [ ] Every actual task has an automated verify and an explicit failing signal.
- [ ] Every missing reference has a task that creates it before use.
- [ ] No three consecutive tasks lack automated behavioral feedback.
- [ ] Current inventory and permitted skips reconcile after each complete lane.
- [ ] Final source is instrumented, and required negative controls actually run.
- [ ] Measured runtime and CI critical-path review are recorded.
- [ ] Nyquist audit disposition is recorded from actual evidence.

Approval: pending execution and audit; no test or acceptance pass is claimed.

## Current-context reconciliation

Planning must assign explicit tests for zero/tiny cycle budgets, initial-refill
preflight, whole-unit refusal/clock overflow without partial effects, and trace
capacity before events. The metadata/producer slice tests index-to-loaded-digest
binding, dispute propagation, raw-input non-PASS and relocation without runtime
CMake/source paths. Direct-start CPU fields are deterministic project policy,
versioned in the profile and included in full canonical state comparisons.
These are current context obligations, all still planned rather than passed.
