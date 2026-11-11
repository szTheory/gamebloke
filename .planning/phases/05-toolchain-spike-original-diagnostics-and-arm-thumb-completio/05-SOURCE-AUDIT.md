# Phase 05 multi-source coverage audit

All rows are planning coverage, not implementation or test passes. No Phase5 tool download, build, test, hosted run or ROM digest admission occurred during planning.

| Source | Item | Required behavior/constraint | Plans | Status |
| --- | --- | --- | --- | --- |
| GOAL | user story | Reproducibly built original full ARM7TDMI diagnostics with precise outcomes and source/tool/output identity | 01–25 | COVERED |
| GOAL | SC1 | Initial and final ARMv4T/cross-host equality or logged Linux fallback before ROM pin | 02,03,05,21 | COVERED |
| GOAL | SC2 | Original owned tools/startup/link/header, public CI source build, manifest/no-logo/no-runtime | 01–05,19–21,24 | COVERED |
| GOAL | SC3 | Explicit tool/BIOS absence SKIPPED, no fallback/PASS, mandatory CI guard | 03,23,24 | COVERED |
| GOAL | SC4 | Production semantic PASS and genuine assertion-broken GUEST_FAIL with exact IDs | 05,19,20,21,23 | COVERED |
| GOAL | SC5 | All named quirks plus full ARM forms and19 Thumb formats | 05–20,22,25 | COVERED |
| REQ | DIAG-01 | Original source/pinned tools/crt0/link/header/no logo/runtime | 01,02,03,04,05,19,20,21,23,24,25 | COVERED |
| REQ | DIAG-02 | Measured v4T/cross-host output identity before digest or Linux ownership | 01,02,03,04,05,21,24,25 | COVERED |
| REQ | VERIF-04 | Visible dependency SKIPPED versus errors/required actual execution | 03,23,24,25 | COVERED |
| REQ | DIAG-03 | Machine-readable real production outcome/ID and broken assertion | 04,05,19,20,21,22,23,24,25 | COVERED |
| REQ | CPU-01 | Complete exact-core ARMv4T families/forms/quirks and diagnostics | 06,07,08,09,10,11,12,13,14,17,18,19,22,23,25 | COVERED |
| REQ | CPU-02 | Complete Thumb formats/forms with separately retired BL | 14,15,16,17,18,20,22,23,25 | COVERED |
| RESEARCH | architecture1 | Gating actual tool/source-image/architecture/map/scan/timing/parity tracer | 01–03,21 | COVERED |
| RESEARCH | architecture2 | Single decoded bounded plan, symbolic loaded PC, snapshot effect order and maximum work | 06–17,22 | COVERED |
| RESEARCH | architecture3 | Exhaustive family/form/case ledger and CPU-1..9 independent expected values | 05–20,25 | COVERED |
| RESEARCH | architecture4 | Source/tool/output identity separation, safe optional artifact and legacy rows | 04,05,21,23 | COVERED |
| RESEARCH | architecture5 | Real production outcome/checker, exact cases, honest skip and stop boundaries | 05,17–21,23 | COVERED |
| RESEARCH | architecture6 | Measured fixed-budget CI, scoped rights/no binary uploads, exact output scans | 01–03,19–25 | COVERED |
| RESEARCH | dont-hand-roll | No second interpreter/runtime parser/SDK/hash/framework or fixture-fetch expansion | 01,02,04,06–25 | COVERED |
| RESEARCH | adversarial | Network/archive corruption; stale authority; false skip; maximum unit; broken source/core controls | 01,02,04,05,22,23,24 | COVERED |
| RESEARCH | validation | Canonical CI/current sanitizers/GNU/installed consumer and actual required hosted proof | 22–25 | COVERED |
| CONTEXT | D-01 | Pinned xPack binary release and exact three host asset size/SHA identities | 01,02,03,21 | COVERED |
| CONTEXT | D-02 | Dedicated bounded redirected tool fetch, hash-before-safe-extract, atomic promotion and component notices; generic fixture fetch unchanged | 01,02 | COVERED |
| CONTEXT | D-03 | First original diagnostic tracer proves object/final ELF v4T, executable disassembly, exact map/no runtime, signature and timing | 02,03 | COVERED |
| CONTEXT | D-04 | Cross-host identity before every admitted ROM pin, exact local cmp, distinct hosted x64 witness and explicit Linux ownership/mismatch behavior | 02,03,05,21,24 | COVERED |
| CONTEXT | D-05 | Measure fresh and final critical paths before CI placement; unchanged 180-second limits, stop on red, no speculative caches/concurrency/dependencies or ROM uploads | 01,02,03,22,24,25 | COVERED |
| CONTEXT | D-06 | Complete exact ARM7TDMI ARMv4T family/form/operand/address coverage and all19 Thumb formats through one interpreter | 05,06,07,08,09,10,11,12,13,14,15,16,17,18,19,20,25 | COVERED |
| CONTEXT | D-07 | Exact-core primary-source hierarchy, all six multiplies, SWI24/8 correction and condition1111 unpredictable | 07,08,09,10,11,12,13,14,15,16,17,18,19,20 | COVERED |
| CONTEXT | D-08 | Same ephemeral decoded ordered transaction/result/register/status plan with maximum-work capacity and no effectful dry-run | 06,10,12,13,16,22 | COVERED |
| CONTEXT | D-09 | Symbolic actual-read PC refill, original banks/SPSR snapshots, exact preflight/no late refusal under uniform bus; future bus-contract prerequisite | 06,08,10,12,13,16,22 | COVERED |
| CONTEXT | D-10 | Bus-owned time and source-referenced CPU-local shift/multiply internal-cost policy, preserving timing limitations | 07,09,18 | COVERED |
| CONTEXT | D-11 | Explicit deterministic unpredictable policies, multiply C/CV preservation, loaded-PC mask/wrap and static B/BX refusal retention | 08,09,10,11,13,14,18,22 | COVERED |
| CONTEXT | D-12 | Honest supported/UND/unpredictable/extension/absent-coprocessor classifications and coherent public reason projection | 18,19,20 | COVERED |
| CONTEXT | D-13 | Original startup/assembly/link/header, explicit stack/mailbox and completion-last; no copied SDK/runtime/content | 02,05,19,20 | COVERED |
| CONTEXT | D-14 | Optional manifest/1 build.artifact path/SHA independent of git source and tool archive, backward compatibility and unchanged result/1 | 04,05,21 | COVERED |
| CONTEXT | D-15 | Source-only revision precedes digest admission, exact full input/flags binding and per-lane artifact roots; safe absent-tool/output index | 04,05,19,20,21,23 | COVERED |
| CONTEXT | D-16 | Actual production runner plus canonical checker, exact status/state/ID/identity and meaningful broken assertion; semantic core mutants separate | 05,19,20,21,22,25 | COVERED |
| CONTEXT | D-17 | Full case crosswalk and CPU-1..9 minimum; BL separately retired/unmasked API IRQ; honest exceptional guest-stop witnesses | 05,13,16,17,18,19,20,22,25 | COVERED |
| CONTEXT | D-18 | Exact generated-directory name allowlist with logo/BIOS checks mandatory and map actual object-only controls | 02,19,20,21,25 | COVERED |
| CONTEXT | D-19 | Optional local dependency visible SKIPPED; required tool/guest/setup errors fail; BIOS unavailable actual SKIPPED/ran=false | 01,03,23,24 | COVERED |
| CONTEXT | D-20 | Measured CI/ASan participation with same canonical core/production index, safe missing outputs and strict inventory/required guards | 03,23,24,25 | COVERED |
| CONTEXT | D-21 | Only narrow original-fixture/necessary public-CI permission, honest notices/provenance, no MIT/LICENSE/binary rights expansion | 05,21,25 | COVERED |
| CONTEXT | D-22 | Profile4→5 coherent expanded claim, retained timing/BIOS/device/qualification limits and historical unresolved probes; no Phase6 expansion | 18,19,25 | COVERED |

## Exclusions, not gaps

Phase6 owns determinism repetition/compiler/interleave/optimization equality and third-party CPU suites. BIOS/HLE, active IRQ/timers/DMA, graphics/audio/devices/Game Pak wait states/prefetch, physical qualification, broad compatibility, stable package/consumer API, project licensing and releases retain their existing later scope. No deferred idea is implemented. Historical Phase4 eleven unresolved rows remain unchanged, and Phase3 Nyquist metadata debt remains for milestone close.

## Dependency and size audit

25 plans, 50 tasks, 25 serial waves. Every plan starts with a production tracer for its own slice and verifies it before expansion. Shared canonical build roots/metadata, CPU files and final source authority make serial waves intentional; no same-wave file conflict is hidden. Every task has at most5 modified files and each plan at most9. Estimate calibration returned factor1 / zero samples / low confidence; raw and calibrated projections are equal. No context-cost scope reduction is used.

## Sources and discovery

Current 05-RESEARCH and three supporting NOTES already perform the domain-specific primary-source discovery; no repeated hardware/manual research or package download was needed. PATTERNS confirmed tracked analogs; git ls-files confirmed actual existing paths. Prior Phase2 contract and Phase4 CPU/runner summaries/history informed source/index/result/verification patterns. No project skills or agent skill mapping are configured; graphify is disabled. The generic API detector and assumption-delta result are handled in COVERAGE.md.

## Remaining unknowns, not missing plans

Actual tool archive/component notices, tool times, architecture attributes, host parity/fallback, final pipeline limits and all implementation/test outcomes await execution in their owning tasks. Three unclassified taxonomy items and three descriptor-less bespoke prohibitions remain flagged. Nine edges have explicit acceptance criteria, with test status pending, not passed. No approval or waiver is inferred.
