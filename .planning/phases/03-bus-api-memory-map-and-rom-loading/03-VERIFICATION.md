---
phase: 03-bus-api-memory-map-and-rom-loading
verified: 2026-11-11T00:18:48Z
status: human_needed
score: "27/28 must-haves verified"
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
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-CI-HANDOFF.md
  - .planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md
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
covered_digest: "v4:sha256:c8dd725e68ef8469b34c9509b45b2d392eb3bed7ddcbf7e7b8381a18796b2297"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: "24/26"
  gaps_closed:
    - "The loader now consumes a configure-time expected digest on the validated owned candidate; synthetic match/mismatch behavior is tested through gbl_load_rom."
    - "Fresh installed-Clang ASan/UBSan evidence now covers the current inventory and passes the 42-test post-inventory check."
  gaps_remaining: []
  regressions: []
prohibition_flags:
  - statement: "MUST NOT present project timing, power-on fill, latch, or ROM-tail policies as measured hardware accuracy or broad compatibility."
    status: "unverified-prohibition — human review recommended"
    judgment: "Current profile/docs label these as project policy and unverified hardware behavior; this is a non-authoritative automated review."
  - statement: "MUST NOT represent synthetic logo comparison as canonical validation or infer redistribution rights from matching bytes or hashes."
    status: "unverified-prohibition — human review recommended"
    judgment: "Current tests/docs keep synthetic and canonical evidence separate and make no rights claim; this is a non-authoritative automated review."
human_verification:
  - test: "Owner supplies the approved logo-window digest through the established reviewed workflow, using a file already held; then verify the configured production diagnostic against that approved authority."
    expected: "Production build reports checked match or advisory mismatch from the approved digest. This establishes the configured comparison result only; it does not by itself establish redistribution rights or broad hardware compatibility."
    why_human: "The production authority remains owner-pending. No canonical digest or real logo bytes were acquired or inspected in this verification."
  - test: "Review the timing, initialization, latch, and ROM-tail descriptions in profile and developer documentation."
    expected: "They remain explicitly described as project policy or unverified behavior, without hardware-accuracy or broad-compatibility claims."
    why_human: "The relevant plan prohibition is judgment-tier and the autonomous review is non-authoritative."
  - test: "Review the synthetic logo test and digest statements in the verification documentation."
    expected: "Synthetic match/mismatch evidence is not presented as genuine Nintendo identity, rights, or canonical acceptance."
    why_human: "The relevant plan prohibition is judgment-tier and requires human review rather than a silent automated pass."
  - test: "Before making a hardware-accuracy claim about ROM-tail reads, obtain suitable hardware evidence."
    expected: "The current-latch behavior remains labelled unverified until that evidence exists."
    why_human: "The plan assigns actual ROM-tail behavior evidence to the core maintainer; this is not required to verify the current software policy."
---

# Phase 3: Bus API, Memory Map, and ROM Loading Verification Report

**Phase Goal:** As a core developer, I want to build the canonical core with its final timing-bearing bus API, full GBA memory map, deterministic power-on state, and a ROM loader that validates untrusted input before allocation or state mutation, so that subsequent CPU work has a deterministic, testable and safe machine boundary. The original technical boundary remains verbatim in ROADMAP.md.

**Verified:** 2026-11-11T00:18:48Z
**Status:** human_needed
**Re-verification:** Yes — after Plan 07 gap closure. The source integration and sanitizer-evidence gaps are closed; the owner-data qualification remains open.

## MVP User Flow Coverage

This is a core-developer and consumer-code flow. It makes no GUI, installed-package, CPU-execution, or physical-controller promise.

| Step | Expected | Evidence | Status |
| --- | --- | --- | --- |
| Initialize a caller-owned instance | Profile-defined deterministic state, independent backing storage | src/machine.c; profiles/agb.json; fresh-instance and isolation cases | VERIFIED |
| Load a bounded ROM through the real API | Invalid extents refuse before allocation/mutation; valid bytes are copied and committed | src/loader.c; loader lengths, transaction and integration cases | VERIFIED |
| Access memory through the core bus | Results expose access class/cost and advance one checked master clock | src/bus.c; bus timing/decode tests and final CI evidence | VERIFIED |
| Inspect logo-window diagnostics | Synthetic match and mismatch pass through gbl_load_rom and remain advisory; production pending authority remains explicitly unchecked | tests/loader_test.c logo-digest selector; production-mode tests; current authority skip | VERIFIED for mechanics and pending behavior |
| Establish genuine canonical-logo result | Production uses the approved digest and reports the real result | Authority has not been supplied; do not infer it from the synthetic expectation | HUMAN NEEDED |
| Provide a later CPU consumer a safe boundary | One in-tree canonical machine/bus/loader path is deterministic and testable | Real core target and consumer-style lifecycle integration; CPU execution is outside this phase | VERIFIED |

## Goal Achievement

The previous report’s 26 truths were rechecked against the current code and evidence. The seven Plan 07 truths are represented below; overlaps with prior transaction, lane, and owner-gate truths are merged. The two earlier code/evidence gaps are closed. The only unmet phase truth is the genuine canonical-logo result, which depends on owner-held authority data.

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Each successful bus request returns its placeholder cost and requested access class and commits that cost once to the checked master clock; callers control ordering. | VERIFIED | src/bus.c:325-436; bus timing tests. |
| 2 | Adjacent region endpoints decode correctly; clock overflow refuses before effects. | VERIFIED | Bus decoder and named endpoint/overflow tests. |
| 3 | Zero internal cycles have zero cost; invalid width/enum/null and overflowing spans return bounded errors without mutation. | VERIFIED | Validation precedes effects in src/bus.c; named timing tests. |
| 4 | Separate machines have independent mutable state; unsupported same-instance concurrency and reentrancy remain host responsibilities. | VERIFIED | src/machine.c; docs/bus-api.md; independent/interleaved tests. |
| 5 | Fresh instances start with documented zero memory/register state; reset clears canonical mutable state, preserves owned ROM, and matches a fresh loaded instance. | VERIFIED | src/machine.c; profile policy; state and integration tests. |
| 6 | The AGB profile labels one-cycle raw-request costs and zero fill as project policy while hardware power-on RAM remains unknown. | VERIFIED | profiles/agb.json; docs/profile-agb.md; profile validator and controls. |
| 7 | Missing/mislabeled/changed bus policy is rejected by the real validator and source-specific controls. | VERIFIED | cmake/CheckProfile.cmake; tests/gates/profile_case.cmake; focused profile lane recorded 29/29. |
| 8 | ROM tails, absent BIOS and unsupported I/O have explicit unverified current-latch policy; WAITCNT/prefetch and active devices remain unmodeled. | VERIFIED | Profile, bus documentation, and distinct bus outcomes. This verifies the declared software policy, not hardware behavior. |
| 9 | Zero and below-header lengths refuse distinctly before allocation/read; size-valid null input refuses as invalid and preserves state. | VERIFIED | src/loader.c:73-117; loader length tests and allocation controls. |
| 10 | Exact 0xC0 and 32 MiB even lengths are accepted; zero, short, oversized, odd inputs have documented reason precedence. | VERIFIED | src/loader.c; boundary and precedence tests. |
| 11 | Successful loading copies caller bytes into owned ROM storage. | VERIFIED | src/loader.c:121-130; caller-edit and bus-read cases. |
| 12 | Refusal and allocation failure preserve canonical state, diagnostics, ROM allocation identity, allocator identity, and caller bytes on fresh and loaded machines. | VERIFIED | tests/state_check.h; transaction tests; refusal_rom_identity and refusal_allocator_identity semantic controls and clean companions. |
| 13 | GBA regions/mirrors have explicit raw-width behavior with GBATEK 3.06 references attached to cases. | VERIFIED | tests/bus_test.c decode tables; src/bus.c map implementation. |
| 14 | ROM is readable at equal offsets in all three 32 MiB aliases; beyond-file reads use the stated unverified project policy. | VERIFIED | src/bus.c:224-233; ROM alias/tail cases. |
| 15 | Physical VRAM mirror translation precedes the OBJ thresholds for modes 0–2 and 3–5. | VERIFIED | src/bus.c:280-299; VRAM threshold cases. |
| 16 | Absent BIOS, unsupported I/O, ROM tail and unmapped reads are distinguishable and bounded per request. | VERIFIED | src/bus.c; bus outcome/timing cases. |
| 17 | Header checksum and unusual game-code findings are bounded advisories; size-valid images still load. | VERIFIED | src/loader.c; loader diagnostics and integrated logo-digest selector. |
| 18 | Under direct-start, a genuine canonical-logo mismatch is reported as an advisory and still loads. | HUMAN NEEDED | The production path now hashes the copied candidate and compares the configured authority. Its current authority remains owner-pending; the integrated synthetic test proves the branch, not genuine canonical identity. |
| 19 | Diagnostics commit only on successful load, survive reset, and remain unchanged on refused load. | VERIFIED | src/loader.c commit order; diagnostics and refusal tests. |
| 20 | A consumer-style C test loads, accesses, refuses replacement, resets and disposes independent machines through the actual core API. | VERIFIED | tests/loader_test.c integration selector; final CI evidence. |
| 21 | Canonical CI/hygiene workflows validate behavior/inventory and expose permitted owner skips. | VERIFIED | Current final logs and inventory post checks; 2 documented owner skips in each lane. |
| 22 | Fresh sanitizer evidence covers the current ASan inventory, bus/loader code, and controls with configured compiler identity and passing post-check. | VERIFIED | Fresh /usr/bin/clang (AppleClang 21.0.0.21000101) configuration: 42/42 tests, zero skips; post inventory 42 expected/42 run. |
| 23 | Phase evidence keeps synthetic evidence, owner-pending canonical acceptance, local lanes, and non-hardware limits distinct. | VERIFIED | Current Plan 07 snapshot in 03-EVIDENCE.md; docs/verify.md and validation rows. |
| 24 | Fresh machines have byte-identical profile-defined memory/register contents while retaining separate backing storage. | VERIFIED | Machine/state cases and current regression lane evidence. |
| 25 | Successful load resets mutable machine state and commits the candidate before releasing the old allocation. | VERIFIED | src/loader.c:121-130; replacement/reset/ownership tests. |
| 26 | The canonical production core reads only the fixed trusted digest authority at configure time; pending authority yields UNCHECKED/OWNER_PENDING. | VERIFIED | CMakeLists.txt:43-49,79-85; cmake/ReadLogoDigest.cmake; production pending-mode test. The authority contents were not opened or printed by the verifier. |
| 27 | Synthetic matching and mismatching 156-byte windows traverse gbl_load_rom on the same core sources/options and remain advisory. | VERIFIED | CMakeLists.txt:64-81; tests/core.cmake; tests/loader_test.c:632-710; bypass/forced-match mutants fail their intended assertions and clean companions pass. |
| 28 | Malformed digest metadata fails configure with one stable reason and no row-content echo. | VERIFIED | cmake/ReadLogoDigest.cmake; tests/gates/logo_digest_case.cmake; 13 authority cases plus parser-bypass control pass in saved CI/ASan logs. |

**Score:** 27/28 must-have truths verified. One truth is human-needed because the production canonical authority value remains external and owner-pending. The old ASan uncertainty is resolved.

## Phase Success Criteria

| # | Roadmap criterion | Status | Evidence |
| --- | --- | --- | --- |
| 1 | Bus results return cost/class and advance one 16,777,216 Hz u64 clock; placeholder costs are labelled. | VERIFIED | Bus API/tests and profile policy. |
| 2 | Full declared memory map, mirrors, width rules, cited cases and open-bus behavior. | VERIFIED | Bus implementation, GBATEK-cited test cases and current regression lane. |
| 3 | Fresh instances have byte-identical memory/register contents matching profile fill policy. | VERIFIED | Machine initialization, profile and state cases. |
| 4 | Loader refusal reasons/postconditions and owned-copy behavior hold. | VERIFIED | Bounds checks, canonical state/input/identity tests, current integrated coverage. |
| 5 | Direct-start missing/mismatched canonical logo, checksum and game-code findings load with diagnostics rather than refusal. | PARTIAL — human needed | Checksum/game-code plus synthetic digest mismatch paths are verified through the real loader API. The genuine canonical digest/result remains owner-pending. |

## Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| include/gamebloke/gamebloke.h and src/machine.c | Caller-owned lifecycle/API and deterministic state | VERIFIED | Substantive and exercised by bus, loader, and integration tests. |
| src/bus.c | Checked timing and GBA-region decoder | VERIFIED | Existing substantive implementation remains covered by current CI. |
| src/loader.c | Bounds-safe copy-on-load with transactional diagnostics | VERIFIED | Validates before allocation; hashes owned candidate before commit; digest mismatch stays advisory. |
| src/logo_sha256.h | Private fixed 156-byte SHA-256 helper | VERIFIED | Original freestanding implementation, independent vectors, code review clean; not a FIPS validation claim. |
| cmake/ReadLogoDigest.cmake | Strict private authority parser | VERIFIED | Exact single row rules, stable refusal reason, no content echo, configure dependency. |
| tests/loader_test.c and tests/gates/logo_digest_case.cmake | Integrated logo-path, parser, refusal and lifecycle witnesses | VERIFIED | Same canonical sources for synthetic expectation; parser controls use owned metadata in scratch trees. |
| tests/inventory/*.txt and tests/core.cmake | Registered and sorted current test lanes | VERIFIED | Current CI/hygiene and fresh ASan post-check evidence. |
| docs/bus-api.md, docs/profile-agb.md, docs/verify.md | Contract and evidence limits | VERIFIED | Explicitly preserves owner gate, unverified hardware policy and no rights inference. |

## Key Link Verification

The generic plan link checker treats descriptive link labels as paths. I checked each concrete connection in source and test registration.

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| Bus read/write/internal API | master_cycles | One checked commit | WIRED | src/bus.c:325-436. |
| gbl_load_rom | host allocator | Bounds/argument checks before allocate | WIRED | src/loader.c:73-117. |
| Candidate allocation | active ROM/reset/release | Successful commit sequence | WIRED | src/loader.c:121-130; replacement tests. |
| Refusal tests | canonical state and identity helper | Field/array/pointer/caller-byte comparisons | WIRED | tests/state_check.h and transaction controls, including ROM and allocator identity mutants. |
| Bus ROM read | owned ROM bytes | Alias offset plus actual extent | WIRED | src/bus.c:224-233; ROM alias/tail tests. |
| DISPCNT mode | physical VRAM byte-write classification | Translate mirror before threshold | WIRED | src/bus.c:280-299; VRAM tests. |
| Candidate logo window [0x04,0xA0) | header diagnostic | Fixed hash of validated owned copy before commit | WIRED | src/loader.c:17-28,129-130; synthetic match/mismatch via gbl_load_rom. |
| Fixed digest authority path | production private compile definition | Strict configure parser and configure dependency | WIRED | CMakeLists.txt:43-49,79-85; cmake/ReadLogoDigest.cmake. Actual owner authority remains pending. |
| Synthetic logo core | canonical production source list/options | Shared target helper, only expected digest differs | WIRED | CMakeLists.txt:64-81; loader_logo_test links synthetic core. |
| Parser authority controls | malformed metadata results | Scratch configure using the real parser | WIRED | tests/gates/logo_digest_case.cmake; saved tests assert stable error and no echo. |
| Current ASan inventory | current JUnit post-check | Fresh configure, instrumented run, post inventory | WIRED | build/asan/evidence-03-07/asan-fresh-workflow.log; 42/42, zero skips. |
| Current CI/hygiene registration | inventory post-check | Canonical presets and explicit inventories | WIRED | Final lane logs; 296/294 + two allowed skips and 201/199 + same skips. |
| Production digest to genuine canonical result | owner-supplied authority value | Existing reviewed owner workflow | HUMAN NEEDED | No approved digest was supplied or inspected; synthetic values establish mechanics only. |

## Data-Flow Trace

| Data | Source | Flow | Status |
| --- | --- | --- | --- |
| ROM candidate | Caller-provided bounded span | Validated, allocated, copied, fixed window hashed, diagnostics committed, active ROM exposed to bus | FLOWING |
| Logo expected digest in synthetic test core | Build-tree owned 1..156 pattern | Configure-time digest constant; same canonical sources exercise match/mismatch | FLOWING (synthetic only) |
| Production expected digest | Fixed trusted authority path | Configure parser → private compile definition → loader compare, or empty pending value | WIRED; current real value remains owner-pending |
| RAM/video bytes | Machine-owned arrays | Bus decode to reads/writes and latch behavior | FLOWING |

No UI, external service, or installed-consumer data path exists in this phase.

## Behavioral Spot-Checks

No tests were run by this verifier. The following saved final lane outputs and independently reconciled artifacts were inspected.

| Behavior | Saved evidence | Result | Status |
| --- | --- | --- | --- |
| Full local CI lane | build/ci/evidence-03-07/ci-workflow-final.log and final-junit.xml | 296 expected; 294 ran; 2 permitted owner skips; post-check passed. CTest 76.65 s. | PASS |
| Full hygiene lane | build/hygiene/evidence-03-07/hygiene-workflow-final.log and final-junit.xml | 201 expected; 199 ran; same 2 permitted skips; post-check passed. CTest 43.78 s. | PASS |
| Fresh ASan/UBSan lane | build/asan/evidence-03-07/asan-fresh-workflow.log, JUnit, cache, configured-tests and compile-commands artifacts | 42/42 tests, zero skips; post inventory reports 42 expected/42 ran/0 skipped. CTest 52.12 s. | PASS |
| Integrated logo behavior and negative controls | ASan/CI logs plus tests/loader_test.c | Synthetic match/mismatch and boundary-byte changes run through gbl_load_rom; bypass and forced-match mutants fail on expected assertions; clean companions pass. | PASS |
| Refusal identity controls | ASan/CI logs | ROM-allocation and allocator-identity mutants fail on canonical identity assertions; clean companions pass. | PASS |
| Digest authority parser controls | CI/ASan/hygiene logs | 13 owned metadata cases and parser-disable control pass; malformed content is not echoed. | PASS |
| Genuine canonical logo comparison | Current production authority mode | No owner-approved digest or genuine source was inspected; test is an allowed owner skip. | HUMAN NEEDED |

The two CI/hygiene skips are signature.logo.real_digest (owner-pending) and signature.owner_local (GBL_OWNER_LOGO_FILE unset). Their reasons remain visible. ASan had no skips and includes the current loader identity and digest controls. Earlier 36/38 sanitizer records are historical and not used for this verdict.

Plan 07's final CI/hygiene logs were run on the same implementation/test delta after the portable-plan-reference fix; the fresh sanitizer run used that same source/inventory delta, and only the documentation path fix followed before its final snapshot. The evidence package retains configured CTest lists, compile commands, JUnit, compiler identity, source/tree snapshot, and inventory reconciliation. An independent auditor reconciled the instrumented binaries and the root post-check confirmed 42/42. These are local lanes, not hosted CI.

## Probe Execution

No Phase 03 plan-declared probe path or conventional probe script was found. Historical probe-taxonomy labels remain distinct from owner decisions and were not counted as new gates.

## Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| BUS-01 | 03-01, 03-02, 03-04, 03-06, 03-07 | SATISFIED | Timing-bearing results, single checked clock, costs/classes and current regression evidence. |
| BUS-02 | 03-01, 03-02, 03-04, 03-06, 03-07 | SATISFIED for declared project contract | Regions, mirrors, widths, citations and latch policy tested. Save-device window remains unsupported; ROM-tail hardware behavior remains unverified. |
| BUS-03 | 03-01, 03-02, 03-03, 03-06, 03-07 | SATISFIED | Profile-defined deterministic init/reset and state equality checks. |
| LOAD-01 | 03-03, 03-05, 03-06, 03-07 | SATISFIED | Bounds and refusal precedence before allocation; caller bytes and whole-state postconditions. |
| LOAD-02 | 03-05, 03-06, 03-07 | NEEDS HUMAN — production digest pending | The canonical comparison mechanism and synthetic API witnesses are implemented; genuine canonical input/result remains owner-gated. Synthetic expected-byte or digest tests do not qualify genuine identity. |
| LOAD-03 | 03-03, 03-06, 03-07 | SATISFIED | Copy-on-load plus refusal/allocation state, diagnostic, ROM identity, allocator identity and caller-byte controls. |

No orphaned Phase 03 requirement was found.

## Anti-Patterns Found

| File | Finding | Severity | Impact |
| --- | --- | --- | --- |
| None | No unresolved debt marker or implementation stub found in the reviewed delta. | — | The fixed hash path, parser and test controls are substantive; guards and unsupported outcomes are intentional. |

The current 03-REVIEW.md covers 14 changed implementation/test files and is clean with no findings. The security follow-up records 22 verified threat rows with zero open high/critical threats. This does not constitute rights acceptance or hardware qualification.

## Prohibition Review Flags

- **unverified-prohibition — human review recommended:** Current docs appear to label project timing/fill/latch/tail behavior as policy or unverified; autonomous text review is non-authoritative.
- **unverified-prohibition — human review recommended:** Current docs/tests appear to distinguish synthetic digest mechanics from canonical identity and rights; autonomous text review is non-authoritative.

No owner risk acceptance is inferred. The plan's owner-pending digest and the signature scan skip remain explicit.

## Gaps Summary

No implementation or current-lane evidence gap remains from the previous report. The fixed SHA-256 mechanism consumes only the configured digest authority and hashes the copied 156-byte candidate window before diagnostics commit. Integrated synthetic tests cover match, mismatch and bytes outside the window, and parser and identity controls have meaningful negative controls. Fresh ASan/UBSan now covers the current registered inventory.

The remaining human gate is the genuine canonical-logo authority/result. Production is intentionally still UNCHECKED/OWNER_PENDING. The owner is next to supply the approved digest through the existing reviewed workflow; the already-wired production path can then be configured and verified. Synthetic comparison does not establish Nintendo identity, redistribution rights, or hardware accuracy. Hosted CI and phase acceptance remain parent/orchestrator work; do not start Phase 4 while the owner-data gate is unresolved.

---

_Verified: 2026-11-11T00:18:48Z_
_Verifier: gsd-verifier_

## Narrow GCC portability re-verification

The parent reviewed the authorized explicit `uint8_t` conversion in the shared
test-state digest helper after hosted GCC rejected the implicit conversion.
Both conditional values are exactly representable; production behavior and
warning flags are unchanged. The corrected source passed the full local
GCC 15.2.0 Linux aarch64 workflow (294 run, two documented owner skips, inventory
post-check passed) and a fresh installed-AppleClang ASan/UBSan run (42 run, zero
skipped, inventory post-check passed). The container's initial nested-configure
fixture failure and the local unavailable-compiler attempt are retained and
distinguished in `03-CI-HANDOFF.md`; the successful runs followed environment
correction. This addendum is parent-authored targeted re-verification, not a
repeat of the prior independent whole-phase review.

The source fingerprint is refreshed after this change. The original automated
truths remain supported; `human_needed`, the canonical authority item and human
judgment items remain unresolved. The corrected head's hosted status belongs
to PR #6's live checks, not the earlier failed run.

_Parent re-verification: 2026-11-11T00:32:07Z_
