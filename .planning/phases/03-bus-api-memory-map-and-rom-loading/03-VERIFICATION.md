---
phase: 03-bus-api-memory-map-and-rom-loading
verified: 2026-11-11T00:55:48Z
status: human_needed
score: "28/28 must-haves verified"
covered_files:
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-01-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-01-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-02-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-02-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-03-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-03-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-04-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-04-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-05-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-05-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-06-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-06-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-07-PLAN.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-07-SUMMARY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-CONTEXT.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-LOGO-ACTIVATION.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-LOGO-REVIEW.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW-DISPOSITION.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-SECURITY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-VALIDATION.md
  - CMakeLists.txt
  - cmake/CheckProfile.cmake
  - cmake/ReadLogoDigest.cmake
  - docs/bus-api.md
  - docs/profile-agb.md
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - profiles/agb.json
  - src/bus.c
  - src/loader.c
  - src/logo_sha256.h
  - src/machine.c
  - tests/bus_test.c
  - tests/core.cmake
  - tests/gates.cmake
  - tests/gates/core_mutation.cmake
  - tests/gates/logo_digest_case.cmake
  - tests/gates/profile_case.cmake
  - tests/inventory/asan.txt
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
  - tests/loader_test.c
  - tests/signatures/logo.sha256
  - tests/state_check.h
  - tests/version_test.c
covered_digest: "v4:sha256:c6bf8376d91aedb9ed611633346c0bbe249ca0512fa1ba1dac4c67af9dcec360"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: "27/28"
  gaps_closed:
    - "Owner-authorized digest activation now configures the production comparison, and the actual production library passes match, advisory mismatch, boundary, reset, and refusal witnesses."
  gaps_remaining: []
  regressions: []
prohibition_flags:
  - statement: "MUST NOT present project timing, power-on fill, latch, or ROM-tail policies as measured hardware accuracy or broad compatibility."
    status: "unverified-prohibition — human review recommended"
    judgment: "Current profile and developer docs retain project-policy/unverified labels; this automated assessment is non-authoritative."
  - statement: "MUST NOT represent synthetic logo comparison as canonical validation or infer redistribution rights from matching bytes or hashes."
    status: "unverified-prohibition — human review recommended"
    judgment: "Current docs distinguish configured local evidence from rights and provenance; human review remains pending."
human_verification:
  - test: "Review the timing, initialization, latch, and ROM-tail descriptions in the profile and bus documentation."
    expected: "They remain labelled as project policy or unverified behavior and make no hardware-accuracy or broad-compatibility claim."
    why_human: "This judgment-tier prohibition requires human review; an automated review cannot accept the claim boundary."
  - test: "Review the synthetic and owner-authorized logo evidence descriptions."
    expected: "They report configured comparison results without claiming original-cartridge provenance, redistribution permission, or broad compatibility."
    why_human: "This judgment-tier prohibition requires human review; matching digests and local comparisons do not decide rights or provenance."
---

# Phase 3: Bus API, Memory Map, and ROM Loading Verification Report

**Phase Goal:** As a core developer, I want to build the canonical core with its final timing-bearing bus API, full GBA memory map, deterministic power-on state, and a ROM loader that validates untrusted input before allocation or state mutation, so that subsequent CPU work has a deterministic, testable and safe machine boundary. The original technical boundary remains verbatim in ROADMAP.md.

**Verified:** 2026-11-11T00:55:48Z
**Status:** human_needed
**Re-verification:** Yes — after owner-authorized production authority activation.

## MVP User Flow Coverage

This is a core-developer and consumer-code flow. It makes no GUI, installed-package, CPU-execution, or physical-controller promise.

| Step | Expected | Evidence | Status |
| --- | --- | --- | --- |
| Initialize a caller-owned instance | Profile-defined deterministic state and independent backing storage | src/machine.c; profiles/agb.json; fresh-instance and isolation cases | VERIFIED |
| Load a bounded image through the API | Invalid extents refuse before allocation/mutation; valid bytes are copied and committed | src/loader.c; loader lengths, transaction and integration tests | VERIFIED |
| Access mapped memory through the core bus | Results expose class/cost and advance the checked master clock | src/bus.c; bus tests and current CI evidence | VERIFIED |
| Diagnose the configured logo window | Current production authority yields a checked match; first/last-byte and zero-window changes yield advisory mismatch and still load | Owner-authorized, byte-free local harness against the production library and its ASan/UBSan build | VERIFIED |
| Use the boundary for later CPU work | Deterministic state, explicit map and timing are available to a future consumer | Actual core target and in-tree lifecycle integration; no CPU execution is claimed | VERIFIED |

## Goal Achievement

The previous 28 truths were rechecked against current source and evidence. The owner-authorized activation resolves the prior production-data gate for this configured comparison. The two remaining human items are judgment reviews of claim boundaries, not implementation gaps.

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Every successful bus request reports its placeholder cost/class and commits once to the checked master clock; callers control ordering. | VERIFIED | src/bus.c and named timing tests. |
| 2 | Adjacent map endpoints decode correctly; clock overflow refuses before effects. | VERIFIED | Bus decoder and endpoint/overflow tests. |
| 3 | Zero internal cycles cost zero; invalid width/enum/null and overflowing spans return bounded errors without mutation. | VERIFIED | Bus validation and named timing tests. |
| 4 | Separate instances have independent state; same-instance concurrency/reentrancy are documented host responsibilities. | VERIFIED | Machine lifecycle, bus guide, independent/interleaved tests. |
| 5 | Fresh instances start with profile-defined zero state; reset clears canonical mutable state, preserves ROM and matches a fresh loaded instance. | VERIFIED | Machine initialization/reset, profile, state and integration tests. |
| 6 | Profile labels raw-request timing and zero initialization as project policy; hardware power-on RAM remains unknown. | VERIFIED | AGB profile v3, narrative and profile validator. |
| 7 | Missing, mislabeled or changed bus policy is rejected by the real validator and source-specific controls. | VERIFIED | Profile cases and current CI; version anchor is v3 and coordinated mutation uses v4. |
| 8 | ROM-tail, BIOS-absent and unsupported-I/O behavior is explicit but unverified; WAITCNT/prefetch and devices remain unmodeled. | VERIFIED | Profile and bus docs state the project policy and limitations. |
| 9 | Zero and below-header lengths refuse before allocation/read; size-valid null image is invalid and preserves state. | VERIFIED | src/loader.c and length/allocation controls. |
| 10 | Exact minimum and maximum even lengths are accepted; zero/short/oversized/odd reasons follow documented precedence. | VERIFIED | Loader boundary and precedence tests. |
| 11 | Successful load copies bytes into owned ROM storage. | VERIFIED | Loader and caller-edit/bus-read cases. |
| 12 | Refusal/allocation failure preserve canonical state, diagnostics, ROM allocation identity, allocator identity, and caller bytes. | VERIFIED | State comparison and both semantic identity controls with clean companions. |
| 13 | Documented GBA regions/mirrors have explicit widths and per-case GBATEK 3.06 references. | VERIFIED | Bus implementation and cited decode table. |
| 14 | ROM aliases return equal offsets; bytes beyond the actual image use the declared unverified project policy. | VERIFIED | Bus decode and ROM alias/tail tests. |
| 15 | Physical VRAM mirroring precedes mode-specific OBJ thresholds. | VERIFIED | Bus implementation and VRAM threshold cases. |
| 16 | BIOS absence, unsupported I/O, ROM tail and unmapped access have distinct outcomes and bounded request timing. | VERIFIED | Bus outcomes and timing tests. |
| 17 | Checksum and unusual game-code findings are advisory; size-valid images still load. | VERIFIED | Loader diagnostic and integration tests. |
| 18 | Configured direct-start logo mismatches, including a zero window, are advisory and do not refuse a size-valid image. | VERIFIED | Owner-authorized production-library harness reports checked match for supplied local headers, mismatch after first/last-window changes and for a zero window; all still load. |
| 19 | Diagnostics update only on successful commit, survive reset and remain unchanged on refusal. | VERIFIED | Loader commit ordering plus reset/refusal witnesses. |
| 20 | In-tree consumer loads, accesses, refuses replacement, resets and disposes independent machines through the real API. | VERIFIED | loader.integration and saved CI evidence. |
| 21 | Current CI validates behavior/inventory and exposes any permitted owner skip. | VERIFIED | Current log: 296 expected, 295 ran, one permitted owner-local skip; inventory post-check passed. The real-logo signature check now runs rather than skipping. |
| 22 | Current sanitizer evidence covers all registered tests and controls with compiler identity and passing post-check. | VERIFIED | Fresh installed-Clang ASan/UBSan: 42/42, zero skips; post-inventory 42 expected/42 ran. Production-library header witnesses also passed under sanitizers. |
| 23 | Phase evidence distinguishes local comparison results from rights, game execution and hardware claims. | VERIFIED | Activation evidence, current verification docs, profile narrative and clean focused review. |
| 24 | Fresh machines have byte-identical profile-defined memory/register contents and independent backing storage. | VERIFIED | Initialization policy and fresh-instance/isolation tests. |
| 25 | Successful replacement resets mutable state and commits before releasing the old allocation. | VERIFIED | Loader transaction sequence and replacement/reset witnesses. |
| 26 | Production consumes the fixed trusted digest authority at configure time and hashes only the copied candidate window. | VERIFIED | CMake authority parser/dependency, src/loader.c, activated production configuration. No raw input bytes or authority value are reproduced here. |
| 27 | The synthetic test core exercises match/mismatch via gbl_load_rom using the canonical core sources/options. | VERIFIED | Separate synthetic target, loader logo-digest test, mutation controls and clean companions. |
| 28 | Malformed authority metadata refuses configure with one stable reason and without echoing row content. | VERIFIED | Strict parser and 13 authority cases plus disabled-parser control in saved lane evidence. |

**Score:** 28/28 must-have truths verified. The overall status remains human_needed because two judgment-tier review items are pending.

## Phase Success Criteria

| # | Roadmap criterion | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Bus results report cost/class and advance one 16,777,216 Hz clock; placeholder costs are labelled. | VERIFIED | Timing API/tests and profile policy. |
| 2 | Full declared map, mirrors, width rules, citations and open-bus behavior. | VERIFIED | Bus implementation, cited cases and current CI. |
| 3 | Fresh instances have identical memory/register contents matching fill policy. | VERIFIED | Machine initialization, profile and state cases. |
| 4 | ROM bounds/refusal reasons preserve state and input; successful load owns its copy. | VERIFIED | Boundary, transaction, caller-buffer and allocation-identity tests. |
| 5 | Missing/mismatched logo, checksum and game-code findings load with diagnostics under direct-start. | VERIFIED for configured authority | Actual production-library checks against owner-authorized local headers and controlled window mutations; diagnostic results remain advisory. They do not establish source provenance, redistribution rights or broad game compatibility. |

## Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| include/gamebloke/gamebloke.h and src/machine.c | Caller-owned API and deterministic state | VERIFIED | Substantive lifecycle is linked through bus/loader and integration tests. |
| src/bus.c | Timed full declared map | VERIFIED | Existing implementation passes current regression lane. |
| src/loader.c | Bounds-safe transactional copy and diagnostics | VERIFIED | Hashes the validated owned candidate and commits diagnostic before reset; mismatch is advisory. |
| src/logo_sha256.h | Private fixed-window SHA-256 | VERIFIED | Independent vectors and review; no FIPS validation claim. |
| cmake/ReadLogoDigest.cmake | Strict authority parser | VERIFIED | Fixed source path, strict row grammar and configure dependency; malformed controls pass. |
| tests/loader_test.c and local harness evidence | Integrated comparison and postconditions | VERIFIED | Public tests use synthetic bytes; authorized local production witnesses are retained byte-free and are not in public CI. |
| profiles/agb.json and docs/profile-agb.md | Current machine profile and narrative | VERIFIED | Version 3 is synchronized; policy limits remain visible. |
| test inventories and core registrations | Current CI/ASan expectations | VERIFIED | Current lane logs and post-JUnit reconciliation agree. |

## Key Link Verification

The generic link query treats descriptive plan labels as paths; these concrete source and test connections were traced.

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| Bus APIs | master clock | Checked bus-owned commit | WIRED | src/bus.c; existing named tests. |
| gbl_load_rom | allocator | Validate before allocate | WIRED | Loader guards precede allocator callback. |
| Candidate ROM | active image and diagnostics | Copy, hash, commit, reset, release | WIRED | src/loader.c and transaction tests. |
| Candidate window [0x04,0xA0) | logo diagnostic | Fixed SHA-256 compared with configure-time authority | WIRED | Production harness exercised matching and first/last/zero-window mismatches. |
| Authority metadata | production private compile definition | Fixed-path parser and configure dependency | WIRED | CMakeLists.txt and cmake/ReadLogoDigest.cmake. |
| Production/synthetic core targets | same implementation and options | Shared target helper; only expectation differs | WIRED | CMake target configuration and loader_logo_test. |
| Refusal tests | canonical state and ownership | Field/array and identity comparisons | WIRED | State helper plus ROM/allocator identity controls. |
| Current inventories | CI/ASan post-JUnit | Presets and explicit inventories | WIRED | CI and ASan post-check logs agree with current counts. |

## Data-Flow Trace

| Data | Source | Flow | Status |
| --- | --- | --- | --- |
| ROM candidate | Caller-provided valid span | Bounded validation → owned copy → window hash → diagnostic commit → bus-visible owned ROM | FLOWING |
| Production expected digest | Fixed configure-time authority | Parser → private compile definition → production loader comparison | FLOWING |
| Synthetic expected digest | Owned build-tree pattern | Separate expectation through the same core sources/options | FLOWING (test only) |
| RAM/video bytes | Machine-owned arrays | Bus decode to reads/writes | FLOWING |

The verifier did not read private file bytes or reproduce their names or digest. The public test and CI inputs remain synthetic; the local evidence is byte-free.

## Behavioral Spot-Checks

No tests were executed by this verifier. I inspected the current saved lane and harness evidence.

| Behavior | Evidence | Result | Status |
| --- | --- | --- | --- |
| Current canonical CI | build/phase03-logo-activation/ci.log and JUnit | 296 expected, 295 ran, one permitted owner-local skip; inventory post passed. The logo digest check ran; no real-logo skip remains. | PASS |
| Current ASan/UBSan | build/phase03-logo-activation/asan.log and JUnit | 42/42, zero skips; post inventory passed 42/42. | PASS |
| Actual production-library header behavior | Byte-free owner-header logs, normal and ASan builds | Both authorized local headers match configured authority; first/last byte flips and zero window report mismatch and still load; outside-window changes match; reset/refusal preserve diagnostics. | PASS |
| Private-content boundary | Byte-free scan report and tracked-file check summarized in activation evidence | Inputs were not added to tracked files or public tests; scan deliberately detected logo-window signatures. | PASS |

The current CI run has one allowed owner-local skip because no private file is supplied to public automation. ASan has zero skips. The separate hygiene full workflow was not repeated for activation; current CI included the source-tree and signature checks, and profile mutations ran in the current CI lane. No hosted CI result is claimed.

## Probe Execution

No Phase 03 probe script is declared or present in the conventional probe location. Historical probe taxonomy is not an owner decision; the activation does not invent a probe result.

## Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| BUS-01 | 03-01, 03-02, 03-04, 03-06, 03-07 | SATISFIED | Timing-bearing result, checked single clock and current regression tests. |
| BUS-02 | 03-01, 03-02, 03-04, 03-06, 03-07 | SATISFIED for declared software policy | Regions, mirrors, widths, citations and latch policy are tested. Save-device mapping remains unsupported; ROM-tail hardware behavior remains unverified. |
| BUS-03 | 03-01, 03-02, 03-03, 03-06, 03-07 | SATISFIED | Deterministic profile initialization/reset and state equality cases. |
| LOAD-01 | 03-03, 03-05, 03-06, 03-07 | SATISFIED | Extent checks before allocation; refusal preserves state, ownership and caller bytes. |
| LOAD-02 | 03-05, 03-06, 03-07 | SATISFIED for configured diagnostic behavior | Production-library checks confirm configured match and advisory mismatch behavior. This does not establish provenance, rights or compatibility. |
| LOAD-03 | 03-03, 03-06, 03-07 | SATISFIED | Copy-on-load and refusal/allocation state, diagnostic, ROM identity, allocator identity and caller-byte witnesses. |

All six Phase 03 requirement IDs are accounted for; there is no orphaned requirement.

## Anti-Patterns Found

| File | Finding | Severity | Impact |
| --- | --- | --- | --- |
| None | No unresolved debt marker or implementation stub found in the activation delta. | — | Focused review is clean; intentional guard returns and unsupported-device results remain substantive behavior. |

The current activation review covers the eight-file policy/profile/test delta and is clean. The prior 14-file implementation review remains clean. Security records 22 verified threat rows and zero open high/critical threats. No owner-accepted risk is inferred.

## Human Verification Required

1. **Review project-policy descriptions** — Check the profile and bus documentation against the current policy. It should keep timing, initialization, latch and ROM-tail behavior explicitly project-defined or unverified, without claiming hardware accuracy or broad compatibility. Human judgment is required by UAT item 2.
2. **Review synthetic-evidence limits** — Check the verification docs distinguish synthetic test vectors and owner-authorized local comparisons from provenance, redistribution rights and broad compatibility. Human judgment is required by UAT item 3.

Both items remain pending in the current UAT record. No additional owner-data task is needed. Actual ROM-tail hardware evidence is a future claim condition, not a current-phase acceptance item.

## Gaps Summary

All 28 technical truths are verified, including the actual configured production comparison and current ASan inventory. The owner-authorized local checks establish that the configured diagnostic behaves as expected for the supplied headers and controlled mutations. Their provenance and redistribution license remain unverified, so no rights, compatibility, BIOS, game execution, or hardware claim follows from them.

The two remaining items are judgment-tier reviews in UAT. They keep verification at human_needed; no human approval is inferred from the clean code review or the automated checks. The parent handles the already-authorized PR and hosted CI after this report. Phase acceptance and Phase 4 remain gated on the review disposition.

---

_Verified: 2026-11-11T00:55:48Z_
_Verifier: gsd-verifier_
