# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — Evidence Spine

**Closeout:** Completed with carried evidence gaps on 2026-11-15; no software release claimed.
**Phases:** 5 of 6 complete | **Plans:** 68 of 71 recorded complete | **Sessions:** not reliably recorded

### What Was Built
- Canonical CI and local verification, fixture provenance, result contracts, and negative controls.
- A portable C17 core, timing-bearing bus, safe content loader, ARM7TDMI runner, and original diagnostics.
- Determinism and source-bound fixture paths, with the Phase 6 audit identifying the remaining evidence boundaries.

### What Worked
- Keeping local observations, hosted evidence, guest outcomes, and claims distinct made the closeout gaps explicit.
- Cheap policy checks and focused test selection helped expose workflow errors before longer suites.
- Archiving the phase artifacts and audit preserves the full rationale for the carryovers.

### What Was Inefficient
- Phase 6 reached its closeout checkpoint before source-bound ARM/Thumb results and hosted Linux x64 evidence were complete.
- Generated milestone summaries used “shipped” wording that needed correction to match the incomplete phase evidence.

### Patterns Established
- A stop before a source-bound observation remains `GUEST_STOP`; it cannot become a pass by inference.
- A local architecture/compiler result does not substitute for a separately required hosted x64 result.
- Event-boundary and test-selection checks belong before long suites.

### Key Lessons
1. Bind a guest verdict to the exact source evaluator and result register before treating zero as success.
2. Carry missing evidence forward by requirement ID and keep it visibly open in every milestone summary.
3. Before building timers, resolve how bus accesses advance machine time; a one-cycle placeholder can make timer results look more precise than they are.

### Cost Observations
- Model mix and session count: not reliably recorded.
- No hosted x64 run or publication was performed for Phase 6 closeout.

---

## Milestone: v2.0 — Time, Interrupts, DMA, First Pixels

**Closeout:** Completed 2026-11-19; no software release claimed.
**Phases:** 4 complete | **Plans:** 33 | **Tasks:** 63 | **Requirements:** 13/13 verified

### What Was Built
- One deterministic machine clock now coordinates timers, interrupts, HALT, logical input, DMA, PPU events, and CPU/bus timing observations.
- The core produces raw mode-3 frames and runs an original BIOS-free Playable 0 diagnostic guest through a real Libretro module.
- A dependency-free terminal host provides visible output and live controls. The owner ran it and accepted this scoped demo after managed macOS policy denied RetroArch launch.
- Current phase verification, security, fast-path, and cross-phase integration evidence was refreshed and archived.

### What Worked
- Cheap deterministic selection and fixture checks before longer suites caught setup and selection errors early.
- The owner judgment used the actual running guest, making the acceptance decision concrete. A markdown review file alone would not have answered whether the demo behaved as expected.
- Timing-model limits, unresolved DMA/PPU disputes, and local-only rights limits stayed visible in the runtime evidence and milestone record.

### What Was Inefficient
- Generated phase summaries and verification state needed reconciliation after final UAT and evidence refresh; the auto-generated milestone list initially still said G6 was pending.
- An installed RetroArch launch was blocked by managed macOS policy, so the planned host trial needed a terminal fallback. The fallback supplied useful behavior evidence, but remains downsampled and below real time.

### Patterns Established
- Keep semantic guest progress, elapsed machine time, device requests, CPU effects, raw video, and host presentation as separate observations.
- Repeatable output and agreement with another emulator remain weaker evidence than an independent hardware-backed oracle.
- Show an owner the runnable artifact for subjective acceptance, and write the accepted scope and exclusions beside the result.
- Preserve disputed or unmodeled hardware behavior as an explicit limit when a project model is useful for deterministic development.

### Key Lessons
1. Validate generated closeout summaries against the final UAT and verification reports before archiving; phase-time status can become stale after the last refresh.
2. Keep a dependency-free local host available as a practical fallback when managed desktop policy blocks an installed frontend.
3. A successful local demo is a product-scope judgment, not evidence of retail compatibility, hardware timing, physical-controller behavior, or redistribution rights.

### Cost Observations
- Model mix and session count: not reliably recorded.
- The terminal host introduced no runtime dependency. Exact aggregate test/runtime cost is available in phase evidence, not summarized here.

---

## Milestone: v3.0 — PPU Modes and Composition

**Closeout:** Completed 2026-11-21; local milestone archive and tag only, no software release or hardware qualification claimed.
**Phases:** 5 complete | **Plans:** 40 | **Tasks:** 76 | **Requirements:** 18/18 verified

### What Was Built
- A deterministic raw-video software model for modes 0–5, with text, affine, and bitmap backgrounds, regular and affine OBJ, shared priorities, windows, color effects, mosaic, and forced blank.
- Capability and output metadata through the `agb` profile and production runner, plus bounded CPU/DMA scene evidence over canonical video memory.
- An explicit project access policy that retains current VRAM, palette RAM, and OAM bus behavior without adding PPU stalls or access denial.
- Semantic pixel expectations and integration evidence through the shared machine clock and canonical runner. Hardware qualification remains UNKNOWN; HBlank, DMA-latch, and within-line visibility disputes remain open.

### What Worked
- Feature-level semantic vectors caught wrong colors and coordinates directly; deterministic hashes remained supplemental repeatability evidence.
- Exact, cheap selector and consistency checks ran ahead of long suites and provided useful failure signals before spending time on the full workflow.
- Cross-phase integration followed the actual data path from CPU/DMA and canonical memory through row capture, composition, raw frames, and runner metadata.
- The final canonical local workflow passed without duplicate configure/build work. Its measured critical path was about 19 minutes, with no evidence that changing the two-worker test setting would safely reduce it.

### What Was Inefficient
- A final-run preflight initially exposed an absolute-path assumption; resolving it before the full canonical workflow avoided spending the long suite on a known setup failure.
- An independent production-accounting review found a mismatch in observed wait/DMA cycle reconciliation. The implementation and focused controls were corrected before the final canonical run.
- Two-worker CTest remained the supported setting. No coverage-preserving speedup was evidenced, so no speculative concurrency, cache, or dependency was added.

### Patterns Established
- Compare production behavior against semantic expected pixels and machine-time observations; matching hashes establish repeatability only.
- Keep selected software policies separate from hardware facts, and carry unresolved timing disputes and unknown qualification into every closeout summary.
- Keep capability metadata synchronized across profile and runner boundaries, with integration checks that detect drift.
- Preserve existing access costs and behavior when the evidence does not support introducing a new restriction or stall.

### Key Lessons
1. Validate cycle totals across internal work, observed waits, DMA pause accounting, and elapsed machine time; each subtotal needs an independent failure control.
2. Run path, selection, identity, and documentation hygiene checks before the expensive suite, then measure the full CI critical path after each run.
3. Retain complete passing coverage and negative controls; only shorten future runs when measured evidence identifies duplicate work or a safe, distinct gate.
4. A deterministic software model is a useful implementation result, but it does not settle hardware timing, rights, or broad compatibility.

### Cost Observations
- Final canonical local workflow: 1,159.66 seconds overall; CTest accounted for 1,151.66 seconds. 1,238 checks ran and passed; one permitted owner-local signature check was skipped because its local input was not configured.
- No hosted CI rerun, `act`, Docker lane, software release, or publication occurred for this closeout. The slowest tests and measured cost are detailed in the archived phase evidence.
- Model mix and session count: not reliably recorded.

---

## Milestone: v4.0 — Audio

**Closeout:** Completed locally 2026-11-23; planning archive only. No software release, hosted CI run, publication, or hardware qualification claimed.
**Phases:** 4 complete | **Plans:** 17 | **Tasks:** 42 | **Requirements:** 13/13 verified

### What Was Built
- Guest-time PSG channels 1–4 and Direct Sound FIFO A/B now share the canonical machine clock, timers, and DMA arbitration.
- The machine produces deterministic six-source pre-PWM stereo S16LE on a fixed reset-aligned 512-cycle grid.
- The production runner captures bounded audio transactionally and keeps guest, capture, and artifact states separate.
- A source-bound BIOS-free diagnostic verifies literal PCM and guest progress in a local-only lane; redistribution remains unknown.

### What Worked
- Exact selectors, profile/result checks, and fixture-exclusion controls ran before the final 19m47s canonical workflow.
- The independent integration pass followed actual bus, timer, FIFO, DMA, mixer, runner, and fixture paths and found all five required flows wired.
- Independent source and security reviews caught a destination replacement race; descriptor-relative cleanup and exclusive publication were fixed and re-audited before final verification.
- The focused Docker Linux audio lane found a GCC 15 portability warning and missing fixture prerequisites before the full canonical run. The named container and temporary source snapshot were removed, while the pinned base image was kept as a reusable cache.

### What Was Inefficient
- Initial Linux lane runs failed on an affine sign-conversion warning and missing build prerequisites. The targeted fixes made the lane self-contained and gave it a generated synthetic CLI fixture, avoiding an ARM toolchain dependency.
- The final canonical run spent 1,176.23 of 1,187.20 seconds in CTest. Five longest controls covered distinct mutation paths, so no coverage-preserving reduction or worker-count change was justified from one run.
- The initial phase review had an artifact-doc mismatch for the literal model ID and sanitizer/local-guest evidence predating the last security fix. Both were resolved with a documentation edit and focused reruns before final verification.

### Patterns Established
- Keep a short, documented platform lane for compiler- and OS-specific file-publication behavior, using generated fixtures and exact resource ownership.
- Name Docker containers with project/task identity, remove only the created container ID and temporary snapshot, and record reusable images explicitly.
- Record critical-path time after every CI run, preserve distinct negative controls, and refresh only evidence affected by a source change.
- Keep CI/release efficiency guidance in both `.planning/PROJECT.md` and the developer-facing `docs/verify.md`.

### Key Lessons
1. Run cheap policy, manifest, inventory, and source-consistency checks before expensive suites; they provide earlier failure signals without weakening acceptance.
2. Re-run focused sanitizer and guest evidence when a reviewed fix changes the code path they exercise, even when the canonical suite is green.
3. Change concurrency only when runner-representative timing shows a safe improvement; distinct long mutation checks remain part of passing coverage.
4. Local fixture authorization does not imply redistribution or publication rights; keep the local lane and external gates explicit.

### Cost Observations
- Final native canonical workflow: 1,187.20 seconds end-to-end; CTest was 1,176.23 seconds (99.1%). 547 tests ran, 726 declared skips were reconciled, and zero failed.
- Pinned Docker `runner-audio` lane: about 32 seconds after 24 seconds setup. The task container and source snapshot were removed; the pinned Ubuntu 26.04 base image was retained for reuse.
- ASan/UBSan refresh passed 14/14 in 10.72 seconds; the final local guest passed 1/1 in 18.75 seconds. No hosted run or `act` run applied.
- Model mix and session count: not reliably recorded.

---

## Milestone: v5.0 — Deterministic Core API and Installed Consumer Proof

**Closeout:** Completed locally 2026-11-24; no software release or hardware qualification claimed. The audit retains one non-blocking relocated-consumer evidence limit, and one verification gap from v1.0 remains acknowledged from its earlier close.
**Phases:** 2 complete | **Plans:** 3 | **Tasks:** 7 | **Requirements:** 9/9 verified

### What Was Built
- A versioned opaque C API over the one canonical machine, with sized extensible structs, caller allocation callbacks, bounded advancement, caller-owned raw media, and documented refusal behavior.
- An installable static API component and a relocated public-only C consumer that checks guest semantics, timed input, literal BGR555LE video, and pre-PWM S16LE audio.
- Failure-atomic refusal and clean retry checks, plus a LibFuturo fit report tied to current consumer measurements and read-only source identities.

### What Worked
- The installed consumer made the public boundary and its exact evidence claims observable without access to private machine structures.
- Literal guest and media assertions stayed independent from hashes, and refusal tests compared corrected retries against clean references.
- The final canonical workflow passed through `ci-post` with 551 tests passed, 726 explicitly allowed skips, and zero failures. A prior hygiene failure from absolute local paths was repaired and kept as historical evidence.
- The fit evaluation identified timing and caller-selected cycle-budget gaps against the real consumer before any dependency or contract change.

### What Was Inefficient
- The first canonical workflow attempt stopped at the historical-path hygiene check and required a focused repair followed by a full rerun.
- The inherited fast regression exceeded its original 600-second limit without a failing test; its one authorized longer retry completed the Phase 7–11 closure and exact Phase 12 selectors in 279 seconds.
- Verification fingerprints and milestone audit records needed refreshing after the validation metadata was reconciled. No implementation files changed for that refresh.

### Patterns Established
- Keep public ABI evolution and allocation policy explicit at the C boundary while retaining one canonical machine implementation.
- Pair API status, guest verdict, stop outcome, and media validity as separate observations.
- Bind consumer and integration claims to exact local artifacts and source identities; keep fixture permission local and hardware qualification unknown.

### Key Lessons
1. Evaluate integration contracts through a real consumer; similar names for frames, input, or capacity do not prove equivalent timing and work limits.
2. Use independent semantic guest checks and literal output expectations; hashes establish identity or repeatability only.
3. Keep accepted evidence limits visible: Phase 20 covers reset, unload, and unsupported-request refusal, while the relocated Phase 21 consumer does not repeat those cases.
4. A measured fit report can justify deferring a dependency or contract change without treating that decision as permanent.

### Cost Observations
- The final canonical workflow took 2,986 seconds in CTest plus 3.8 seconds configure and 0.47 seconds for the `ci-post` inventory; 551 checks passed, 726 allowed skips were accounted for, and none failed.
- The relocated-consumer negative-control check took 20 seconds. The inherited regression retry took 279 seconds.
- Model mix and session count were not reliably recorded. No hosted run, release, or publication occurred.

---

## Cross-Milestone Trends

Four milestones are recorded. Each established that evidence must be bound to the exact claim and that missing observations stay open instead of being inferred from adjacent results.

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | Unknown | 5 complete, 1 incomplete | Evidence claims and carryover gaps are recorded separately. |
| v2.0 | Unknown | 4 complete | One shared deterministic clock integrates device behavior while preserving explicit timing and compatibility limits. |
| v3.0 | Unknown | 5 complete | Semantic raw-pixel behavior now spans modes 0–5 and composition, with timing disputes and qualification limits still explicit. |
| v4.0 | Unknown | 4 complete | Guest-time audio and bounded raw capture now integrate through the canonical runner, with critical-path measurements and local-only rights limits retained. |
| v5.0 | Unknown | 2 complete | A versioned opaque API and relocated public consumer provide measurable integration evidence; adoption remains separate from fit. |

### Cumulative Quality

| Milestone | Tests | Coverage | Zero-Dep Additions |
|-----------|-------|----------|-------------------|
| v1.0 | See archived phase evidence | Not summarized | Not summarized |
| v2.0 | See archived phase evidence | 13/13 requirements; 4/4 phase verifications | Dependency-free terminal host |
| v3.0 | 1,238 passed; one allowed local-only skip | 18/18 requirements; 5/5 phase verifications | None |
| v4.0 | 547 passed; 726 explicitly allowed skips; zero failures | 13/13 requirements; 4/4 phase verifications; 5/5 integration flows | None |
| v5.0 | 551 passed; 726 explicitly allowed skips; zero failures | 9/9 requirements; 2/2 phase verifications; 1/1 E2E flow | None |

### Top Lessons (Verified Across Milestones)

1. Bind each pass to the exact guest, source, profile, and evidence path it proves; keep missing observations open.
2. Run cheap deterministic checks before longer suites when they catch known selection or setup failures.
3. Separate local project-model results from hosted, hardware, rights, and compatibility claims.
4. Measure long-run bottlenecks before changing concurrency; keep all distinct required controls and use focused reruns after fixes.
5. Measure required integration semantics through the real consumer before inferring that another library's frame-level contract preserves timed input or a strict caller-selected cycle budget.
