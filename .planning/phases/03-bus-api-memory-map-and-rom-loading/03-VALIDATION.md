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

| Task | Requirement | Threat Ref | Expected safe behavior | Type | Automated command | Creation dependency | Status |
|---|---|---|---|---|---|---|---|
| 03-01 T1 | BUS-01, BUS-03 | T-03-01, T-03-02 | Real EWRAM round trip has cost/class/clock; fresh state zero; no host core dependencies | linked C tracer | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^core\.version$' --no-tests=error --output-on-failure` | Existing core.version; this task creates machine/bus sources | pass — current CI |
| 03-01 T2 | BUS-01, BUS-03 | T-03-01, T-03-02, T-03-03 | Zero work/invalid arguments/clock overflow preserve canonical state; instances independent | unit | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(bus\.timing|machine\.state)$' --no-tests=error --output-on-failure` | 03-01 T1; creates bus_test and named registrations/inventories | pass — current CI/ASan |
| 03-01 T3 | BUS-01, BUS-03 | T-03-01, T-03-02, T-03-03 | All result/refusal/reset contracts are executable and documented | unit + canonical inventory | `cmake --workflow --preset ci` | 03-01 T1–T2 | pass — current CI inventory |
| 03-02 T1 | BUS-01, BUS-02, BUS-03 | T-03-04, T-03-05 | Profile declares one-cycle/zero-fill/read policies as project policy and refuses missing data | profile tracer/control | `cmake --preset hygiene && cmake -DCASE=bus_policy_missing -DSRC=. -DBIN=build/hygiene -P tests/gates/profile_case.cmake && ctest --test-dir build/hygiene -R '^profile\.lint$' --no-tests=error --output-on-failure` | Existing profile.lint; this task creates policy schema/case | pass — current hygiene profile controls |
| 03-02 T2 | BUS-01, BUS-02, BUS-03 | T-03-04, T-03-05 | Every new policy reason rejects a real mutated copy; clean companions pass | semantic mutation | `cmake --workflow --preset ci && cmake --workflow --preset hygiene` | 03-02 T1; creates policy controls and ci/hygiene inventory entries | pass — current CI/hygiene |
| 03-03 T1 | LOAD-01, LOAD-03, BUS-03 | T-03-06, T-03-07, T-03-09 | Valid ROM is copied through host callbacks; no default heap; caller edit isolated | linked C tracer | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^core\.version$' --no-tests=error --output-on-failure` | 03-01 lifecycle; this task creates loader source/API | pass — current CI |
| 03-03 T2 | LOAD-01, LOAD-03, BUS-03 | T-03-06, T-03-07, T-03-08 | Exact size reasons before read/allocation; fresh and loaded state/input unchanged on refusal/failure | boundary/unit | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.(lengths|transaction)$' --no-tests=error --output-on-failure` | 03-03 T1; creates loader_test/state_check and registrations | pass — current CI/ASan |
| 03-03 T3 | LOAD-01, LOAD-03, BUS-03 | T-03-07, T-03-08, T-03-09 | Candidate commit/reset precedes old release; repeated reset/dispose ownership correct | lifecycle/unit | `cmake --workflow --preset ci` | 03-03 T1–T2 | pass — current CI |
| 03-04 T1 | BUS-01, BUS-02 | T-03-10, T-03-12 | Bounded full decode, no ROM-image modulo, explicit fallback outcomes | loaded-ROM tracer/unit | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^bus\.(decode|video)$' --no-tests=error --output-on-failure` | 03-01/03-03; creates decode/video selectors and registrations | pass — current CI/ASan |
| 03-04 T2 | BUS-01, BUS-02, BUS-03 | T-03-10, T-03-11, T-03-12 | Mirror-aware video bytes obey mode-specific thresholds; I/O subset explicit | video/unit | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^bus\.' --no-tests=error --output-on-failure` | 03-04 T1; extends video assertions | pass — current CI/ASan |
| 03-04 T3 | BUS-01, BUS-02 | T-03-01, T-03-10, T-03-11 | Deliberate mirror/video/overflow faults fail real assertions; clean compilation passes | scratch source mutation | `cmake --workflow --preset ci` | 03-04 T1–T2; creates core_mutation driver and controls | pass — current CI/ASan controls and clean companions |
| 03-05 T1 | LOAD-01, LOAD-02, LOAD-03 | T-03-13, T-03-14, T-03-15 | Bounded advisory fields load; canonical logo unchecked; prior diagnostics remain unchanged on refusal | header tracer/unit | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.transaction$' --no-tests=error --output-on-failure && build/ci/loader_test diagnostics` | 03-03; creates diagnostics, compare helper and selector | pass — current CI/ASan; canonical LOAD-02 PARTIAL |
| 03-05 T2 | LOAD-01, LOAD-02, LOAD-03 | T-03-06, T-03-08, T-03-13, T-03-15 | Early allocation/state/input/false-logo faults detected semantically; owner skips preserved | scratch source mutation | `cmake --workflow --preset ci` | 03-04 driver/03-05 T1; creates loader controls and diagnostics registration | pass — current CI/ASan; canonical LOAD-02 PARTIAL |
| 03-06 T1 | BUS-01, BUS-02, BUS-03, LOAD-01, LOAD-02, LOAD-03 | T-03-16 | Consumer-style loaded/reset/refusal/interleaving evidence is explicit | linked integration tracer | `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.integration$' --no-tests=error --output-on-failure` | 03-01–03-05; creates integration selector/registration | pass — focused twice and current CI/ASan |
| 03-06 T2 | BUS-01, BUS-02, BUS-03, LOAD-01, LOAD-02, LOAD-03 | T-03-16, T-03-17 | Current canonical/sanitizer evidence, actual inventory, visible gaps/owners | full verification | `cmake --workflow --preset ci && cmake --workflow --preset hygiene && sh scripts/hygiene.sh --tree && CMAKE=$(command -v cmake) sh scripts/signature-scan.sh --tree`; local sanitizer: `cmake --preset asan -DCMAKE_C_COMPILER="$(command -v clang)" && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post` | All prior tasks; existing presets/tools; creates 03-EVIDENCE.md | pass — final post-documentation workflows, tree scans and ASan; LOAD-02 remains PARTIAL |

Each command's immediate failure signal is specified in its PLAN.md `<fails_when>` sibling: non-zero exit, missing named tests, semantic `check failed`, unexpected skips/inventory differences, or a mutant accepted by the real tests. Compiler-only errors are infrastructure errors for semantic mutation controls, never successful negative evidence. No execution status is inferred from these rows.

The eight resolved edge predicates are represented in plan truths. BUS-02, LOAD-02 and LOAD-03 unclassified edges remain flagged assumptions. The two descriptor-less transparency/rights prohibitions remain flagged-unverified and need evidence review; they have no fabricated check descriptors. Threat IDs are unique across plans except the reserved supply-chain row. No package installs or external API integration are planned.

## Wave 0 Requirements

- Add machine, bus, and loader C tests and register them in ci/asan lanes; no framework install.
- Each creating task supplies its tests before focused verification; later tasks depend on that creation.
- Update sorted inventories with new test names. Preserve existing allowed skips.
- Extend profile JSON, validator, narrative, and semantic negative controls together.
- Assert canonical state fields/arrays, not raw padded struct or pointer identity.
- Include invalid-length-before-allocation/read controls, loaded non-default refusal, allocation failure, copy isolation, successful replacement/reset, clock overflow atomicity, mirror/width/video mode cases, and diagnostic comparison controls.
- Use only synthesized rights-clean image bytes. No canonical logo bytes, digest, BIOS, or game ROM.

## Manual-Only Verifications

| Behavior | Requirement | Why external | Instructions / owner | Status |
|----------|-------------|--------------|----------------------|--------|
| Real-logo canonical mismatch | LOAD-02 | Owner-pending digest; synthetic expected window proves algorithm only | Owner supplies approved digest; implementation reports unchecked until supplied. | PARTIAL; owner action remains open. |
| Hardware ROM-tail behavior | BUS-02 | Consulted reference does not settle physical behavior | Keep the selected current-latch policy labelled unverified; future evidence owner is the core maintainer. | Unverified; no hardware claim. |

## Unresolved probe assumptions and retained prohibitions

The three assumptions the probe did not classify remain open even though the current deterministic contract has automated evidence:

- **BUS-02:** probe taxonomy did not close the map question; in particular, short-image ROM-tail behavior remains unknown on hardware.
- **LOAD-02:** probe taxonomy did not establish canonical-logo comparison. Synthetic spans test only comparison mechanics; the owner digest is still required.
- **LOAD-03:** probe taxonomy did not classify the refusal/ownership edge. Full-state and input comparisons, callback ordering, and allocation-failure tests verify the selected contract but do not retroactively classify that probe result.

These two descriptor-less prohibitions remain flagged-unverified for evidence review and have no fabricated check descriptor:

- Do not present project timing, fill, latch, or ROM-tail policies as measured hardware accuracy or broad compatibility.
- Do not represent synthetic logo comparison as canonical-logo validation or infer redistribution rights from matching bytes or hashes.

## Validation Sign-Off

Current CI, hygiene, direct-scan, and sanitizer results are recorded in
[`03-EVIDENCE.md`](03-EVIDENCE.md). Automated task rows above reflect executed
checks; overall validation and Nyquist sign-off remain pending while the
canonical-logo acceptance and flagged-unverified review items remain open.

- [ ] Every task has automated verification and stated failure direction
- [ ] Missing tests have explicit creating task/dependency
- [ ] Sampling continuity and inventory controls checked
- [ ] ci, hygiene and available sanitizer evidence recorded after implementation
- [ ] No fabricated real-logo acceptance; LOAD-02 canonical gap remains partial

**Approval:** Automated task coverage has been audited and exercised. Overall
phase acceptance remains pending the real-logo requirement; no owner acceptance
is inferred. The legacy probe classifications above are historical tool limits,
not substitutes for the current source/test evidence.

## Validation Audit 2026-11-10

| Metric | Count |
|---|---|
| Gaps found | 1 |
| Resolved | 1 |
| Escalated | 0 |
