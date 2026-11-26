# Milestones

## v6.0 Public API Host Session Readiness (Completed locally: 2026-11-25)

**Phases completed:** 2 phases, 3 plans, 7 tasks
**Verification:** 6/6 requirements satisfied; 2/2 phases passed goal verification; 2/2 end-to-end flows complete. Audit status is `tech_debt` for the historical Playstead snapshot, Linux helper coverage, and unpublished local branch.
**Outcome:** Local GSD milestone complete. No hosted CI run, software release, package publication, Playstead route change, or hardware qualification occurred. Remote `main` had no open PR or status/workflow run at closeout.
**Closeout:** Open-artifact audit found no open items. One previously acknowledged v1.0 verification gap remains carried forward.
**What's next:** No scope is selected. Use `.planning/ROADMAP.md` for a clean Playstead route-feasibility assessment, a consumer-driven save-device slice, or targeted compatibility evidence; obtain owner authorization before publication or merge.

**Key accomplishments:**
- A metadata-only preflight validates the archived Phase 21 records before fixture setup.
- A relocated-package C17 consumer proves a source-bound local GBA session, fresh replay, raw media literals, and refusal-safe retry.
- A source-cited report maps the v1 host boundary to one inspected Playstead snapshot and preserves unknowns.
- A measured fast hygiene gate stops expensive CI jobs on consistency failures without removing test coverage.

---

## v5.0 Deterministic Core API and Installed Consumer Proof (Completed locally: 2026-11-24)

**Delivered:** A stable local C API and relocated consumer prove the public GBA machine boundary and provide evidence for future integration decisions.
**Phases completed:** 2 phases, 3 plans, 7 tasks
**Verification:** 9/9 requirements satisfied; both phases passed goal verification and Nyquist validation. Audit status is `tech_debt` for one non-blocking relocated-consumer evidence limit.
**Outcome:** Local GSD milestone complete. No software release, package publication, hosted release run, or hardware qualification occurred. Hardware qualification remains UNKNOWN.
**Closeout:** Verified; the open-artifact audit found no open items. One previously acknowledged v1.0 verification gap remains suppressed.
**Stats:** 47 files changed, 10,028 insertions and 89 deletions across the implementation/review range; 4,458 C/header lines added in 5 files. Two calendar days from the first API implementation commit to final feature commit.
**Git range:** `56d0226` (first Phase 20 implementation) → `0c2080d` (final Phase 21 feature commit); closeout records continue through `c50bcd7`.
**Known evidence limit:** The relocated consumer does not repeat reset, unload, or unsupported-request refusal; Phase 20 API tests cover those cases.
**Known verification overrides:** 0 newly acknowledged; 1 previously acknowledged verification gap from v1.0 remains carried forward.
**What's next:** No scope is selected; use the rolling horizon in `.planning/ROADMAP.md` for the next milestone discussion.

**Key accomplishments:**
- A versioned opaque C boundary now runs the canonical machine with caller-owned allocation, copied input, bounded transactional execution, raw media output and a relocatable static component.
- A relocated public C consumer reproduces a local BIOS-free diagnostic with literal BGR555LE video and pre-PWM S16LE audio evidence.
- The installed consumer now proves atomic refusals and retries, and the LibFuturo assessment is tied to fresh local proof and read-only source identities.

---

## v4.0 Audio (Completed locally: 2026-11-23)

**Phases completed:** 4 phases, 17 plans, 42 tasks
**Verification:** 13/13 requirements satisfied; 4/4 phases passed; 5/5 integration flows wired. Audit status is `tech_debt` for documented owner gates and carryovers.
**Outcome:** Local software milestone complete. No software release, hosted CI run, fixture redistribution, physical hardware qualification, or publication occurred. Hardware qualification remains UNKNOWN.
**Known verification overrides:** 0 newly acknowledged; 1 previously acknowledged from v1.0 remains carried forward.

**Key accomplishments:**
- PSG channels 1–4 and Direct Sound FIFO A/B now advance on guest time through shared timer/DMA behavior.
- A fixed-grid six-source stereo mixer produces deterministic pre-PWM S16LE samples with literal semantic vectors.
- The production runner writes bounded audio artifacts transactionally and reports guest, capture, and artifact outcomes separately.
- Profile, result schema, fixture metadata, and evidence docs agree on capabilities and limits; the source-bound diagnostic remains local-only with redistribution unknown.
- Canonical local CI, a final-source sanitizer subset, a local guest run, and the pinned Linux audio lane passed. Critical-path measurements and cleanup are recorded in the audit.

---

## v3.0 PPU Modes and Composition (Completed: 2026-11-21)

**Phases completed:** 5 phases, 40 plans, 76 tasks
**Outcome:** Local milestone complete; no software release or hardware qualification is claimed.

**Key accomplishments:**
- The AGB profile now declares its six display-mode capabilities, canonical raw-video format and selected CPU/DMA access policy, with exact checks for profile and documentation drift.
- Runner output now pairs model-2 frame observations with profile-bound display and video-memory capabilities.
- Real CPU stores and scheduled DMA units preserve line-start row ordering, existing video-memory access rules, and atomic refusal.
- Mode-0 and mode-1 text BGs now render through bounded canonical sampling, with semantic pixels and failure-atomic edge controls.
- Profile version 11 and runner results now identify the supported mode 0/1 text backgrounds without claiming affine mode 1, composition, or hardware qualification.
- The Phase 12 helper now runs six exact selectors on the configured tree, reuses the Phase 11 closure, and changes only the configured test hook.
- Mode 1 BG2 and mode 2 BG2/BG3 affine rows now use captured signed 8.8 transforms and bounded sampled reads.
- Modes 3–5 now sample direct and indexed BG2 bitmaps through captured affine state with checked extents, page rules, and format-specific transparency.
- Profile and runner version 13 now publish bitmap modes 3–5 exactly, while frame validity and hardware qualification remain separate.
- Plan 12-08's source-bound acceptance passed on revision `8754011`; a later independent review found and fixed four implementation issues. Post-review acceptance now passes on `6abaffc` and is recorded in `12-EVIDENCE.md`.
- Regular OBJ shapes, mappings, clipping, and transparency now render through the captured 240x160 frame path with mode-bounded VRAM reads.
- Per-row OBJ admission now selects eligible objects within 1226 abstract work units, exposes exact results, and preserves valid partial frames on overflow.
- AGB profile v14 now publishes exact OBJ budget/composition policy, and the active runner accepts normal OBJ frames without weakening partial or UNKNOWN claims.
- OBJ row-boundary and atomicity checks now bind the shared compositor to repeatable semantic output across independent machine runs.
- Phase 13 OBJ and compositor evidence passed exact semantic selectors and canonical local CI with inventory agreement.
- Sampled forced blank now commits exact white BGR555_LE rows and a valid 160-row frame through the shared PPU transaction.
- Sampled rectangular and object windows now reveal eligible layers deterministically within the existing bounded OBJ scan.
- The captured-row renderer now blends the immediate visible pair and applies bounded brighten/darken effects while binding raw effect controls to refusal checks.
- BG text, affine, and bitmap sources sample top-left screen-grid anchors through the current row's capture while destination windows and effects remain per pixel.
- Regular, affine and window OBJ mosaic now use bounded screen-grid anchors with current-row memory and unchanged admission costs.
- Phase 14 control captures now have complete named transaction bindings and deterministic replay evidence, including the CPU preview consumer.
- Profile 15 exposes the implemented window, color-effect, mosaic, and forced-blank policies with checked partial-capability claims.
- The active runner emits profile-15 display capability metadata, proves forced blank as white raw output, and rejects stale feature tuples.
- The Phase 14 display-effects manifest now checks 13 exact selectors, their commands and labels, and the separate inherited profile closure.
- The exact 13-test display-effects suite now runs before one inherited closure, with bounded fixture edges and measured stage times.
- Source-bound PPU composition evidence is complete, and the settled software model passed all 1,230 selected local CI cases with one authorized owner-local skip.
- A generated BIOS-free ARM guest proves CPU/DMA mode-0 composition, mailbox completion, literal pixels, full-frame output, and unchanged runner metadata.
- The CPU video-memory matrix and all line-start cases now execute real ARM instructions and assert independent memory and pixel expectations.
- DMA now has independent transfer evidence for every selected video-memory access state, plus sampled-pixel checks around line start.

**Known verification overrides:** 0 newly acknowledged; 1 carried forward from v1.0 ([Phase 6 verification](milestones/v1.0-phases/06-determinism-gates-and-third-party-cpu-fixtures/06-VERIFICATION.md), details in [STATE.md](STATE.md#deferred-items)).

---

## v2.0 Time, Interrupts, DMA, First Pixels (Completed: 2026-11-19)

**Phases completed:** 4 phases, 33 plans, 63 tasks
**Outcome:** Local milestone complete; no software release or hardware qualification is claimed.

**Key accomplishments:**
- The shared machine scheduler now drives timers, IRQ requests, HALT, logical input, timed bus costs, DMA, and recurring PPU events with explicit project-model timing boundaries.
- CPU, DMA, device events, raw mode-3 video, result contracts, profile policy, and deterministic replay are integrated through the canonical core; disputed hardware timing and DMA latch behavior remain labeled unresolved.
- Phases 7–10 passed their current goal verification, validation, security, and fast-path checks, with cross-phase integration and all 13 mapped v2.0 requirements verified.
- An original BIOS-free Playable 0 diagnostic guest runs through the real Libretro module and a dependency-free terminal host with live controls. The owner ran it and accepted this scoped demo on 2026-11-19 after managed macOS policy denied RetroArch launch.
- The demo acceptance covers only the visible local diagnostic guest. It does not establish retail compatibility, hardware timing accuracy, audio/save support, physical controller behavior, redistribution rights, or publication readiness.

**Known verification overrides:** 0 newly acknowledged; 1 carried forward from v1.0 ([Phase 6 verification](milestones/v1.0-phases/06-determinism-gates-and-third-party-cpu-fixtures/06-VERIFICATION.md), details in [STATE.md](STATE.md#deferred-items)).

---

## v1.0 Evidence Spine (Completed with evidence gaps: 2026-11-15)

**Phase status:** 5 of 6 phases complete. Phase 6 remains incomplete; 68 of 71 plans are complete.

**Carried verification gaps:** `DIAG-04` is still open because the local ARM and Thumb diagnostics stopped before source-bound result evaluation. `DET-02` is still open because hosted Linux x64 GCC and Clang evidence has not run. Local Linux ARM64 results passed; they do not close either gap. See [v1.0 audit](milestones/v1.0-MILESTONE-AUDIT.md) and [Phase 6 verification](milestones/v1.0-phases/06-determinism-gates-and-third-party-cpu-fixtures/06-VERIFICATION.md).

**Key accomplishments:**
- `cmake --workflow --preset ci` builds the C17 skeleton, runs CTest with junit, and a verbose post step proves every inventoried test ran (`inventory: expected 1, ran 1, skipped 0 (allowed 0)`), with configure-time refusal of malformed registrations and unsorted or mismatched inventories.
- The ci workflow now fails on every way a test can silently not run (missing, extra, duplicate, failed, disabled, skipped without permission or reason, empty or absent junit), each shape proven by a control that must exit non-zero with its stated reason code, plus a meta-control that proves the wrapper fails when a gate passes; the core and runner contract is pinned by 24 tests (`inventory: expected 23, ran 23, skipped 0 (allowed 0)`).
- `scripts/hygiene.sh` now fails the verify command on credential shapes, non-noreply addresses, and absolute home paths (never echoing the match), proven by 16 hygiene tests with fragment-built planted controls; a compile-free `hygiene` workflow (`inventory: expected 16, ran 16`) is ready for the always-on CI job, and an opt-in staged hook is available (`inventory: expected 39, ran 39` in the ci lane).
- `scripts/signature-scan.sh` fails the verify command on a listed logo-window or BIOS digest, a ROM/save file name, or an unlisted binary, using committed digests only, with 19 synthesized-stand-in controls and a well-formedness check; the real logo digest is visibly owner-pending (`inventory: expected 62, ran 60, skipped 2 (allowed 2)` in the ci lane).
- `cmake --workflow --preset asan` builds the core and runner with Clang 21 under `-fsanitize=address,undefined -fno-sanitize-recover=all`, passes the core and runner tests, catches both a shift-by-32 and a heap-buffer-overflow control by exit status plus sanitizer text (`inventory: expected 9, ran 9, skipped 0 (allowed 0)`), and `scripts/linux-lane.sh` proves it, and the GCC ci lane, in an ubuntu:26.04 container before any CI run.
- A single `CI required` check now sits over a classifier, an aggregator, a live-title gate, and three preset-only lanes (Linux GCC, macOS arm64, Linux clang-21 sanitizers); the routing predicates are case-table tested, and the workflow is SHA-pinned with no expression inside any run block.
- Two checked-in text gates now keep `.github/workflows/ci.yml` honest: every action is pinned to a 40-hex SHA with a version comment, and a twelve-rule policy gate guards labels, triggers, the `CI required` needs list, run-block expressions, preset-only heavy lanes, timeouts, credentials and permissions; each case has a control that mutates a copy of the real workflow.
- The `main` ruleset is now a checked-in file (pull request plus the `CI required` status check, no bypass actors) held to the owner's authorization by an offline validator, and `scripts/apply-repo-settings.sh` applies or checks only that ruleset, refusing until a `CI required` run has succeeded on main, with a stub-`gh` control for every GitHub-facing branch.
- The CMake 3.25.3 floor and both Linux lanes were proven locally (after a policy fix that the floor run exposed), PR B (#1) ran every lane green on GitHub, took measured 3-minute timeouts, and was squash-merged; main's push run reports `CI required` success.
- A `.planning/`-only PR (#2) skipped every heavy lane while `CI required` passed, the `main` ruleset (id 26908822, `CI required`, no bypass) was applied last and PR A merged through it, GSD switched to per-phase branches, and the owner authorized one more close-out PR.
- The owner kept the real Nintendo-logo digest pending, so the EVID-04 real-logo item is recorded as a visible human-needed gap (SKIPPED with reason, CI green), and the phase close-out reached main through one owner-authorized PR (#3) squash-merged through the ruleset.
- Strict string(JSON) loader with a structural root walk, the nine-state evidence vocabulary as data, and an oracle validator, all wired into the canonical ci and hygiene workflows with 22 controls.
- Six seeded open disputes in registry/disputes.json, checked by a CMake validator that pins the seed ids and resolves labels from the evidence vocabulary, with a control for every reason code.
- A versioned `agb` profile whose eleven timing constants each carry one source label (HBlank flag edge is null and UNK, pointing at the dispute registry), nine honest unmodeled entries synced to a narrative doc, and a CMake validator with the R1 BIOS-provider grammar and a control for every reason code.
- A manifest and pins linter (`cmake/CheckManifest.cmake`) with registry and pin cross-checks, an empty real manifest, a header-only `pins.txt`, one committed complete example row, and `manifest.lint` running in the canonical ci command with a control for every manifest and pins reason code.
- A POSIX `scripts/fetch-pin.sh` that downloads by pins.txt row into a temporary directory inside `.fixtures/`, hashes with `cmake -E sha256sum`, and moves the file into place only when the SHA-256 matches, with a control for every reason code through synthesized `file://` fixtures, `.fixtures/` ignored, and a gate proving nothing under it is tracked.
- A `cmake/CheckResult.cmake` validator for `gamebloke.result/1` documents that takes its per-state rules from `contracts/evidence.json`, checks the claim tuple with the R1 BIOS grammar, and refuses a result that points at a disputed manifest row without listing the dispute and carrying the `disputed` label, backed by 27 registered tests and an illustrative PASS example checked in the canonical command.
- The Phase 2 contracts are documented in a new `docs/evidence-contract.md` and a reason-code section covering every validator family in `docs/verify.md`, and the finished phase is proven on a Kitware CMake 3.25.3 floor binary (SHA-256 `771548ed...234e`, verified against Kitware's list), in the Linux container, and on the default toolchain, with `02-VALIDATION.md` signed off for what was verified automatically.
- Caller-owned machine lifecycle and a one-cycle EWRAM bus with failure-atomic clock overflow handling.
- Version 2 of the agb profile now validates bus costs, deterministic initialization, and explicitly unverified read policy against the narrative.
- Bounded synthetic ROM loads now own a copy, preserve all canonical state on refusal, and cold-reset before releasing replaced storage.
- The timed bus now decodes bounded ROM aliases and mapped memory lanes, applies mode-sensitive video byte stores, and proves five fault families with scratch-only mutants.
- The loader now reports bounded advisory header findings from its owned copy, while canonical logo verification stays owner-pending.
- A linked consumer-style test now exercises the assembled loaded-machine lifecycle, with measured CI and sanitizer evidence that preserves the open logo and hardware gates.
- Fresh machines now start in the declared direct-start state and execute a bounded ARM MOV/B slice through real bus fetches.
- Both configured cores and scratch controls now compile the same CPU source, with field-wise lifecycle and hard-budget refusal evidence.
- Defined ARM arithmetic and shifts, Thumb/BX control flow, and bus-backed word and byte transfers with focused semantic controls.
- ARM/Thumb exception entry now preserves banked mode state, uses explicit LR address bases, and stops before an absent BIOS vector can execute.
- AGB profile v4 now records the deterministic CPU start state and bounded ARM/Thumb scope, while keeping architecture facts and project timing policy visibly separate.
- A real guest-written mailbox can be read without changing CPU, bus, memory, or ownership state, and incomplete writes never count as a verdict.
- The canonical CPU path now emits selected guest-time trace records and refuses a whole unit explicitly when its caller-owned trace output cannot fit.
- A dependency-free C17 helper now hashes arbitrary caller-owned spans with independent SHA-256 vectors and explicit no-write refusal behavior.
- Original build-only ARM and Thumb images now feed separate validated production and synthetic indexes, with hash and metadata identity gates in CI and ASan.
- The headless CLI now runs a fresh canonical machine with bounded input and time, and emits a metadata-bound JSON result or an explicitly diagnostic raw-input result.
- The headless runner now has a real CLI failure matrix, bounded signal cancellation, explicit trace failure precedence, and CI/ASAN coverage.
- The result checker now validates actual runner output against mailbox semantics, exact cycle bounds, termination, and trace completeness while preserving synthetic metadata limits.
- The installed production runner now survives prefix relocation with byte-identical bounded output, while the final source, inventory, sanitizer, and claim-boundary evidence is documented.
- Pinned xPack GNU Arm Embedded 15.2.1-1.1 to exact host assets and installed it through bounded download, validated extraction, and atomic promotion.
- Project-owned ARM/Thumb smoke images now build with v4T evidence and byte-identical local macOS arm64/Linux arm64 outputs, with the real runner's mailbox result checked independently from its unregistered-content stop.
- The bounded hosted witness now passes on Linux x64 and macOS arm64 within the unchanged 180-second jobs, with hosted SHA identity kept distinct from local literal byte comparisons.
- Source-built Git fixtures can now carry a separately pinned output digest through the immutable runtime index and result validator without changing manifest/1 or result/1.
- The source-pinned ARM/Thumb smoke pair now runs through the real production index and canonical result checker, with exact mailbox identity and local cross-host byte parity.
- Word SWP and SWPB now execute through bounded ordered bus slots with lane-correct results and atomic whole-unit refusal.
- All ARM data-processing operations now execute through the canonical CPU path with tested NZCV behavior, Operand2 shifters, condition handling, and bounded bus-charged internal work.
- ARM7TDMI PC writes and MRS/MSR now preserve ARMv4T mode, bank, and pipeline semantics.
- The canonical ARM CPU path now executes all six multiply forms with exact result bits and source-referenced internal-cycle costs.
- ARMv4T byte/word transfers now use preflighted effects, and LDR PC refills from its actual loaded word result.
- ARMv4T halfword and signed transfers use canonical CPU slots, with GBA-specific odd-address policies tested against actual bus results.
- Normal ARM7TDMI STM/LDM transfers now cover IA/IB/DA/DB, with bounded 16-slot execution and actual-read PC refill.
- Thumb formats 1–5 now execute their legal ARMv4T arithmetic/register forms through the existing decode, preflight and bus-owned execution path.
- Thumb formats 6–13 now use the canonical bounded CPU path for transfers, address generation and stack adjustment.
- Thumb PUSH/POP and STMIA/LDMIA now use bounded ordered slots, architectural base rules, and preflighted bus work.
- Thumb BL halves now retire independently through the canonical CPU unit, with banked LR and a real API IRQ boundary between halves.
- The CPU distinguishes unpredictable operands, recognized later encodings, UND and absent coprocessors; profile5 describes the implemented families and their explicit project policies.
- 244 original ARM pass/fail pairs and four honest stop witnesses execute through the production runner, with pinned build outputs and independent trace controls.
- 117 original Thumb pass/fail pairs and eight honest stop witnesses execute through the production runner, with separately observed BL halves and independently checked opcode forms.
- All722 final original diagnostic images have actual cross-host evidence and immutable production bindings;355 semantic pairs and12 named guest stops pass their distinct contracts.
- Actual CPU mutants fail independent semantic assertions, and maximum ARM/Thumb units preserve complete state on capacity refusal while exact-fit work completes.
- Production runner JSON and complete traces now match across same-process and fresh-process runs, with explicit controls for changed and missing evidence.
- Canonical ARM and Thumb-bearing machines now match their isolated results when alternated, including distinct pass/fail outcomes, lifecycle changes, and trace-contamination detection.
- Batched observations now compare exact O0/O2 guest records and trace hashes across all 722 admitted owned diagnostics, with strict GNU/Clang qualification reserved for Linux.
- A canonical CPU sequence now binds the complete trace, terminal status, guest progress, and final machine time; disposable ALU-path mutations fail the gate for the named time-authority mismatch.
- Typed register-evaluation bindings now pass through the canonical manifest validator into immutable generated indexes, with legacy mailbox metadata preserved.
- The runner and checker now bind pre-display ARM and Thumb results to exact fixture metadata while keeping mailbox observations independent.

---
