---
phase: 03-bus-api-memory-map-and-rom-loading
verified: 2026-11-10T22:52:33Z
status: gaps_found
score: "24/26 must-haves verified"
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
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW-DISPOSITION.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-REVIEW.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-SECURITY.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-VALIDATION.md
  - CMakeLists.txt
  - cmake/CheckProfile.cmake
  - docs/bus-api.md
  - docs/profile-agb.md
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - profiles/agb.json
  - src/bus.c
  - src/loader.c
  - src/machine.c
  - tests/bus_test.c
  - tests/core.cmake
  - tests/gates.cmake
  - tests/gates/core_mutation.cmake
  - tests/gates/profile_case.cmake
  - tests/inventory/asan.txt
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
  - tests/loader_test.c
  - tests/state_check.h
  - tests/version_test.c
covered_digest: "v4:sha256:4391ede343a28dfb762daf50cf528fb0a7b64c04cd546f20a57e9ad600ba2dfd"
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "Under direct-start, a size-valid ROM with a canonical-logo mismatch is reported as an advisory diagnostic and still loads."
    status: failed
    reason: "The loader always records UNCHECKED/OWNER_PENDING and does not call the generic comparison helper. Synthetic expected-byte comparisons test helper mechanics only. The approved digest alone is insufficient; the loader must consume it and commit the comparison result."
    artifacts:
      - path: src/loader.c
        issue: "The diagnostic initializer sets canonical_logo to UNCHECKED with OWNER_PENDING; no canonical comparison occurs during load."
      - path: tests/loader_test.c
        issue: "Tests exercise synthetic comparison spans and assert the canonical result remains unchecked; they do not prove a real canonical mismatch through the loader."
    missing:
      - "Owner supplies the approved digest through the existing workflow."
      - "Integrate comparison of the approved digest into the successful loader diagnostic path while keeping logo findings advisory."
      - "Add rights-clean tests for matching and mismatching inputs through the integrated loader path."
---

# Phase 3: Bus API, Memory Map, and ROM Loading Verification Report

**Phase Goal:** As a core developer, I want to build the canonical core with its final timing-bearing bus API, full GBA memory map, deterministic power-on state, and a ROM loader that validates untrusted input before allocation or state mutation, so that subsequent CPU work has a deterministic, testable and safe machine boundary. The original technical boundary is retained verbatim in ROADMAP.md.

**Verified:** 2026-11-10T22:52:33Z
**Status:** gaps_found
**Re-verification:** No; initial verification.

## MVP User Flow Coverage

This is a developer and consumer-code flow. It does not promise a GUI, installed package, CPU execution, or physical hardware behavior.

| Step | Expected | Evidence in codebase | Status |
| --- | --- | --- | --- |
| Initialize a caller-owned core instance | Machine storage starts in the profile's deterministic zero state | `src/machine.c:3-25`; `profiles/agb.json`; `tests/bus_test.c` state selector | VERIFIED |
| Load a bounded synthetic ROM through the real API | Invalid inputs are rejected before allocation or mutation; valid inputs are copied and committed | `src/loader.c:63-131`; `tests/loader_test.c:238-272,287-428`; consumer integration test | VERIFIED |
| Access mapped memory through the core bus | Read/write/internal-cycle results carry cost and access class and update the one master clock | `src/bus.c:325-436`; `tests/bus_test.c`; fresh local CI log | VERIFIED |
| Use the boundary for subsequent CPU work | Deterministic state, explicit mapping and timing are available to a future CPU consumer | API and lifecycle are implemented and integrated; no CPU execution is claimed in this phase | VERIFIED for the stated boundary; CPU behavior is out of scope |
| Diagnose canonical-logo mismatch during load | A mismatch is an advisory diagnostic and does not refuse a size-valid image | Loader currently reports UNCHECKED/OWNER_PENDING and never compares the canonical digest | FAILED |

## Goal Achievement

The 26 unique truths below merge repeated roadmap and plan statements; every roadmap success criterion and every plan must-have is represented. Twenty-four are verified, the current sanitizer-coverage claim is uncertain, and the canonical-logo mismatch behavior is failed.

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Each bus request returns and commits its placeholder cost, requested sequential class, and checked master-clock advancement; caller ordering is explicit. | VERIFIED | `src/bus.c:325-436`; `docs/bus-api.md`; timing tests in `tests/bus_test.c`. |
| 2 | Adjacent region endpoints decode correctly; overflow at UINT64_MAX refuses before effects. | VERIFIED | Bus decode/timing implementation and named boundary/overflow tests. |
| 3 | Zero internal-cycle advance costs zero; invalid width, enum, null instance and overflowing address spans return bounded errors without mutation. | VERIFIED | Validation precedes effects in `src/bus.c`; named timing tests. |
| 4 | Separate machine instances have independent backing state; same-instance concurrency and reentrancy limits are documented host responsibilities. | VERIFIED | `src/machine.c`; `docs/bus-api.md`; independent/interleaved instance tests. |
| 5 | Repeated cold reset returns canonical mutable state to zero, preserves the owned ROM and matches a fresh loaded instance. | VERIFIED | `src/machine.c:69-75`; profile fill policy; state and integration tests. |
| 6 | The AGB profile names one-cycle raw-request costs and zero initialization as project policy, while hardware power-on RAM remains unknown. | VERIFIED | `profiles/agb.json`; `docs/profile-agb.md`; validator and profile cases. |
| 7 | Missing, mislabeled, or changed bus policy fails the real validator and source-specific controls. | VERIFIED | `cmake/CheckProfile.cmake`; `tests/gates/profile_case.cmake`; focused profile lane 29/29. |
| 8 | ROM tails, absent BIOS and unsupported I/O use explicit unverified current-latch policy; WAITCNT/prefetch and active devices remain unmodeled. | VERIFIED | Profile and bus guide label these limits; bus outcomes are distinct. This verifies stated policy, not hardware accuracy. |
| 9 | Zero and below-header lengths refuse distinctly before allocation or image reads; size-valid null input is invalid and preserves state. | VERIFIED | `src/loader.c:73-117`; length/argument tests and allocation controls. |
| 10 | Lengths and offsets are byte-based; exact 0xC0 and 32 MiB even images are accepted, with zero/short/oversized/odd refusal precedence. | VERIFIED | `src/loader.c`; length-boundary and precedence cases in `tests/loader_test.c`. |
| 11 | Successful loading owns a copy; later caller-buffer edits do not alter the active ROM. | VERIFIED | `src/loader.c:121-130`; caller-edit and bus-read tests. |
| 12 | Refusal and allocation failure preserve all canonical state and caller bytes on fresh and non-default loaded instances. | VERIFIED | `tests/state_check.h:23-77,107-143`; `tests/loader_test.c:238-272`; required fault controls. |
| 13 | All documented GBA windows and mirrors have explicit width behavior with GBATEK 3.06 citations on individual test cases. | VERIFIED | Decode table and cited cases in `tests/bus_test.c:118-210`; implementation in `src/bus.c:82-120`. |
| 14 | A loaded ROM is readable at equal offsets in three 32 MiB aliases; reads beyond its actual extent use the declared unverified policy. | VERIFIED | `src/bus.c:224-233`; ROM alias/tail tests in `tests/bus_test.c`. |
| 15 | Physical VRAM translation precedes OBJ byte-write thresholds: 0x10000 in modes 0–2 and 0x14000 in modes 3–5. | VERIFIED | `src/bus.c:280-299`; threshold/mirror test cases. |
| 16 | BIOS absence, unsupported mapped I/O, ROM tail and unmapped access have distinguishable outcomes and bounded per-request timing. | VERIFIED | `src/bus.c`; outcome and timing tests. |
| 17 | Under direct-start, bad header checksum and unusual game code are bounded advisories; size-valid images still load. | VERIFIED | `src/loader.c:3-20,63-131`; loader diagnostics tests. |
| 18 | The canonical logo path reports unchecked with owner-pending reason; synthetic equality/mismatch only demonstrates comparison mechanics. | FAILED | The truthful unchecked state is present, but this is weaker than the roadmap and PLAN-05 requirement to diagnose an actual canonical mismatch. See gap. |
| 19 | Diagnostics change only on successful commit, survive reset, and remain unchanged after refusal together with caller bytes. | VERIFIED | Candidate commit in `src/loader.c:121-130`; diagnostic lifecycle/refusal tests. |
| 20 | A consumer-style C test loads, accesses, refuses replacement, resets and disposes independent machines through the real API. | VERIFIED | `tests/loader_test.c` integration selector and `loader.integration`; saved final CI log passed. |
| 21 | The canonical local/CI workflow checks behavior and sorted inventories, with missing checks and permitted owner skips visible. | VERIFIED | `CMakeLists.txt`, `tests/core.cmake`, inventories; saved CI and hygiene logs; 2 permitted owner skips visible. |
| 22 | Available sanitizer evidence covers current bus/loader sources and controls with compiler identity and measured time. | UNCERTAIN | Evidence document records ASan 36/36, but current inventory has 38 non-post entries while the saved configured ASan build lists 36. The two current identity controls are absent there. No current 38/38 artifact was independently verified; no sanitizer run was repeated for this report. |
| 23 | Phase evidence identifies canonical-logo acceptance as partial with owner and next action, without treating green local workflows as phase acceptance. | VERIFIED | `03-EVIDENCE.md`, `03-VALIDATION.md`, `docs/verify.md`; report independently retains the gap. |
| 24 | Fresh instances have byte-identical memory/register contents matching the profile fill policy, while retaining independent backing memory. | VERIFIED | `src/machine.c`; profile zero-fill policy; fresh-instance state and isolation cases. |
| 25 | Successful loading resets canonical mutable state and commits the candidate before releasing the old allocation. | VERIFIED | `src/loader.c:121-130`; replacement and reset tests. |
| 26 | The phase delivers a deterministic, testable and safe canonical machine boundary for later CPU work. | VERIFIED for the stated boundary | The real core target combines machine, bus and loader and is exercised through the consumer-style lifecycle test. No CPU execution is claimed. |

**Score:** 24/26 truths verified (1 sanitizer-coverage claim is uncertain; 1 canonical-logo behavior failed). The uncertain claim is not scored as verified.

## Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `include/gamebloke/gamebloke.h`, `src/machine.c` | Caller-owned lifecycle and API contract | VERIFIED | Substantive declarations and deterministic initialization/reset/disposal; used by bus, loader and tests. |
| `src/bus.c` | Checked timed address decode and access behavior | VERIFIED | Full declared map, mirrors, width behavior, clock commit and outcomes; exercised by named tests. |
| `src/loader.c` | Safe transactional bounded ROM loading and diagnostics | PARTIAL | Validation/copy/commit behavior is substantive and tested; canonical digest comparison is not wired. |
| `profiles/agb.json`, `cmake/CheckProfile.cmake`, `tests/gates/profile_case.cmake` | Machine-checkable profile policy and controls | VERIFIED | Real profile schema/doc comparison plus mutation cases; focused profile lane passed. |
| `tests/bus_test.c`, `tests/loader_test.c`, `tests/state_check.h` | Semantic, transaction and integrated behavior witnesses | VERIFIED | Real core API calls and canonical field/array comparisons, not struct-padding comparisons. |
| `tests/core.cmake`, `tests/inventory/*.txt`, `tests/gates*.cmake` | Required registrations, inventories and meaningful controls | VERIFIED for current CI/hygiene | Fresh saved CI/hygiene logs show inventory checks and two permitted skips; local saved ASan configuration is stale for current inventory. |
| `docs/bus-api.md`, `docs/profile-agb.md`, `docs/verify.md` | Contract, policy limits and verification procedure | VERIFIED | Explicitly distinguish project policy, synthetic evidence, owner gates and out-of-scope hardware/CPU claims. |

## Key Link Verification

The generic link query could not interpret PLAN `from:` labels as filesystem paths. I manually traced each source-to-consumer path below.

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| `gbl_bus_read/write/internal` | `gbl_machine.master_cycles` | Checked bus-owned commit | WIRED | Commit occurs after validation and before return in `src/bus.c:325-436`. |
| Test registrations | CI/ASan inventories | `gbl_add_test` plus sorted list | WIRED | `tests/core.cmake`; inventory post-check. Current configured ASan listing is stale, see truth 22. |
| AGB profile policy | Profile documentation | Semantic validator comparison | WIRED | `cmake/CheckProfile.cmake` compares parsed policy and narrative table. |
| Profile mutation registrations | Mutated profile copies | Expected failure and reason prefix | WIRED | `tests/gates/profile_case.cmake`; 29/29 focused profile cases passed. |
| `gbl_load_rom` | Allocator callback | Length/argument validation before allocation | WIRED | `src/loader.c:73-117`; tests assert no allocation for malformed extents. |
| Candidate ROM | Active ROM/reset/release | Commit, reset, release old allocation | WIRED | `src/loader.c:121-130`; replacement tests. |
| Refusal tests | Canonical state helper | Field/array and caller-byte comparison | WIRED | `tests/loader_test.c` calls `tests/state_check.h`; excludes padding and pointer identity. |
| Bus ROM read | Copied ROM bytes | Alias offset and actual length guard | WIRED | `src/bus.c:224-233`; alias and tail cases. |
| DISPCNT mode | Physical VRAM write classification | Mirror translation before threshold | WIRED | `src/bus.c:280-299`; threshold tests. |
| Candidate header diagnostics | Machine diagnostic fields | Successful candidate commit | WIRED | Candidate values commit in `src/loader.c`; refusal and reset lifecycle tested. |
| Owner logo gate | `UNCHECKED/OWNER_PENDING` result | Bounded pending diagnostic | PARTIAL | The pending diagnostic is wired. There is no loader comparison, so this link does not prove mismatch reporting. |
| `loader.integration` | Lifecycle, loader, bus, reset | Linked real C API and synthetic host inputs | WIRED | Integration test executes the in-tree API end to end; no installed consumer claim. |
| CI/ASan registrations | Inventory post-check | Presets and explicit inventories | PARTIAL | Current CI/hygiene logs pass; saved ASan build configuration predates two current controls. |

## Data-Flow Trace

| Artifact | Data | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| ROM loader to bus | Loaded ROM bytes | Caller span copied to owned allocation; bus reads active image offsets | Yes | FLOWING |
| RAM/video bus | EWRAM, IWRAM, PRAM, VRAM, OAM | Machine-owned arrays updated and read by mapped bus path | Yes | FLOWING |
| Header diagnostics | Header fields from candidate ROM | Bounded header reads copied into candidate diagnostics on successful load | Yes, except canonical logo verdict | PARTIAL |
| Canonical logo result | Approved expected digest | No source is consumed by the loader | No | DISCONNECTED; this is the failed truth |

There is no rendered UI or database in this phase, so UI data-flow checks do not apply.

## Behavioral Spot-Checks

No tests were executed during this verification. I inspected saved outputs from the final recovery checks and enumerated the configured ASan tests to detect stale coverage.

| Behavior | Evidence | Result | Status |
| --- | --- | --- | --- |
| Bus/map/timing, machine state, loader refusal/diagnostics/integration | `/private/tmp/gamebloke-recovery-final-ci.log`; workflow exit 0, 292 expected, 290 run, 2 permitted owner skips, 82.52 s | Named tests passed; inventory complete for configured CI | PASS |
| Hygiene inventory | `/private/tmp/gamebloke-recovery-final-hygiene.log`; exit 0, 200 expected, 198 run, same 2 permitted skips, 45.92 s | Hygiene and inventory passed | PASS |
| Focused profile policy controls | `/private/tmp/gamebloke-recovery-profile-final.log`; 29/29 | Profile policy validation passed | PASS |
| ASan/UBSan current coverage | `03-EVIDENCE.md` reports 36/36; current inventory has 38 non-post tests; saved configured list has 36 non-post tests | Two currently registered identity controls are missing from that configured list; no fresh matching run exists | UNCERTAIN |

The two CI/hygiene skips are `signature.logo.real_digest` (owner-pending) and `signature.owner_local` (`GBL_OWNER_LOGO_FILE` unset). Their skip reasons are visible. They are not evidence of canonical mismatch behavior.

## Probe Execution

No phase-declared probe path or conventional `scripts/*/tests/probe-*.sh` probe was found. Historical validation probe-taxonomy classifications are not owner decisions and were not treated as new gaps.

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| --- | --- | --- | --- | --- |
| BUS-01 | 03-01, 03-02 | Final timing-bearing bus API and master clock | SATISFIED | Timing API, checked clock, exact class/cost tests and named passing CI cases. |
| BUS-02 | 03-01, 03-02, 03-04 | Full GBA memory map and documented width/mirror behavior | SATISFIED for declared project contract | Decode, aliases, outcomes and citations are tested. Save-device window is deliberately unmapped; ROM-tail behavior remains explicitly unverified hardware policy. |
| BUS-03 | 03-01, 03-02 | Deterministic initialization/reset policy | SATISFIED | Machine arrays/scalars initialized explicitly; fresh/reset/loaded state compared. Hardware power-on equivalence is not claimed. |
| LOAD-01 | 03-03 | Validate untrusted ROM extents before allocation or mutation | SATISFIED | Length precedence and pre-allocation checks; transaction and allocator fault cases. |
| LOAD-02 | 03-05, 03-06 | Direct-start logo/checksum/game-code advisory diagnostics | BLOCKED | Checksum/game-code diagnostics load advisably; real canonical-logo mismatch is not compared by the loader. Synthetic comparisons are insufficient. |
| LOAD-03 | 03-03, 03-06 | ROM copy ownership and transactional refusal postconditions | SATISFIED | Caller edits do not change owned image; refusal/allocation failure preserves full canonical state and input bytes. |

All six requirement IDs declared in plans and mapped in REQUIREMENTS.md are accounted for; no orphaned Phase 3 requirement exists.

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| None | — | No unresolved TODO/FIXME/XXX debt marker or substantive stub found in the reviewed implementation files. | — | Guard returns and explicit unsupported-device outcomes are intentional behavior, not empty implementations. |

The clean current `03-REVIEW.md` reports no findings on its reviewed delta; prior broad core review and fixes are complete. I did not reopen that review loop.

## Human Verification Required

1. **Canonical logo diagnostic** — Owner supplies the approved digest through the existing workflow. The maintainer then wires the digest comparison into the loader and tests matching and mismatching synthetic inputs through that path. A digest alone cannot fix the current disconnected loader path. No real logo bytes were acquired or compared here.
2. **Plan prohibition review (recommended)** — Review the project policy and claims for the unresolved plan prohibitions: do not present current timing/fill/latch/ROM-tail policies as hardware accuracy or broad compatibility, and do not treat synthetic logo comparison or a matching hash as canonical validation or rights evidence. Current docs appear to preserve these limits; this is a non-authoritative judgment, not owner acceptance.
3. **ROM-tail hardware behavior** — The core maintainer owns future evidence from a suitable hardware source before making any hardware-accuracy claim. The current project policy remains explicitly unverified; this does not block the software boundary on its own.

## Gaps Summary

The core API, declared memory map, deterministic state policy, transactional ROM load, refusal postconditions and in-tree consumer flow are present and exercised. Phase acceptance is blocked because loading a ROM never compares the approved canonical-logo digest: it reports `UNCHECKED/OWNER_PENDING`, and the synthetic comparator tests do not establish real mismatch reporting. The owner supplies the approved digest through the existing workflow; code integration and rights-clean tests are also required. Separately, the available sanitizer record does not match the current inventory, so that must-have remains uncertain until a fresh installed-Clang ASan/UBSan configuration covers all current registered tests and controls.

Hosted CI has not run in this verification snapshot. The parent handles the already-authorized PR and required hosted checks after the local code fix; no phase acceptance or hardware qualification is inferred here.

---

_Verified: 2026-11-10T22:52:33Z_
_Verifier: gsd-verifier_
