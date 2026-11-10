---
phase: "03"
slug: "bus-api-memory-map-and-rom-loading"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 03 — Validation Strategy

Planning contract; executable task rows and threat references are finalized with the plans. This file is not execution evidence.

## Test Infrastructure

| Property | Value |
|----------|-------|
| Framework | CTest and in-tree tests/check.h, C17 |
| Config | CMakePresets.json, tests/core.cmake, tests/inventory/*.txt |
| Quick command | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(bus|machine|loader)\.' --no-tests=error --output-on-failure` |
| Full command | `cmake --workflow --preset ci` |
| Additional lane | Existing ASan/UBSan workflow; report tool absence explicitly |
| Runtime | Measure during execution; no unmeasured latency claim |

## Sampling Rate

- After each task: focused tests with `--no-tests=error`, then source/gate checks appropriate to changed files.
- After each wave: canonical workflow including sorted inventory check.
- Before verification: full ci and hygiene workflows; ASan/UBSan where available; record any unavailable lane and owner.
- Do not run ctest without configure/build when sources or registrations changed. Missing tests and unexpected skips are failures.

## Per-Task Verification Map

Planner must replace this paragraph with one row per task, including requirement, unique threat ID, expected safe behavior, test type, runnable command, creation dependency, and pending status. No completed execution statuses at plan time.

## Wave 0 Requirements

- Add machine, bus, and loader C tests and register them in ci/asan lanes; no framework install.
- Each creating task supplies its tests before focused verification; later tasks depend on that creation.
- Update sorted inventories with new test names. Preserve existing allowed skips.
- Extend profile JSON, validator, narrative, and semantic negative controls together.
- Assert canonical state fields/arrays, not raw padded struct or pointer identity.
- Include invalid-length-before-allocation/read controls, loaded non-default refusal, allocation failure, copy isolation, successful replacement/reset, clock overflow atomicity, mirror/width/video mode cases, and diagnostic comparison controls.
- Use only synthesized rights-clean image bytes. No canonical logo bytes, digest, BIOS, or game ROM.

## Manual-Only Verifications

| Behavior | Requirement | Why external | Instructions / owner |
|----------|-------------|--------------|----------------------|
| Real-logo canonical mismatch | LOAD-02 | Owner-pending digest; synthetic expected window proves algorithm only | Owner supplies approved digest; implementation reports unchecked until supplied. Keep LOAD-02 partial and phase acceptance gap visible. |
| Hardware ROM-tail behavior | BUS-02 | Consulted reference does not settle physical behavior | Project open-bus placeholder is tested and labelled unverified; future evidence owner is core maintainer. No hardware claim. |

## Validation Sign-Off

- [ ] Every task has automated verification and stated failure direction
- [ ] Missing tests have explicit creating task/dependency
- [ ] Sampling continuity and inventory controls checked
- [ ] ci, hygiene and available sanitizer evidence recorded after implementation
- [ ] No fabricated real-logo acceptance; LOAD-02 canonical gap remains partial

**Approval:** pending execution. Do not set nyquist_compliant or wave_0_complete merely because plans exist.
