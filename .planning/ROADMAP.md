# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived with Phase 6 incomplete and two evidence gaps carried forward. See [archive](milestones/v1.0-ROADMAP.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed 2026-11-19 through Phase 10; the owner accepted the scoped local Playable 0 demo. See [archive](milestones/v2.0-ROADMAP.md) and [audit](milestones/v2.0-MILESTONE-AUDIT.md).
- 🚧 **v3.0 PPU Modes and Composition** — Phases 11–15 below.
- 📋 **v4.0 Audio**, **v5.0 Cartridge and Saves**, **v6.0 API, Consumer, Packaging** — Recommended horizon only; revisit at each milestone boundary.

GSD milestone numbers are planning labels, not Gamebloke software versions.
Milestone completion does not authorize a software release or establish hardware
qualification or broad compatibility.

## v3.0 — PPU Modes and Composition

**Milestone Goal:** Functional display modes 0–5 and common composition through
the deterministic canonical raw-video path for the named `agb` profile.

## Overview

Extend the existing deterministic mode-3 output into functional display modes
0–5 and common BG/OBJ composition for the named `agb`, direct-start/no-BIOS,
ROM-only profile. First make sampled rows and transactional writes reliable,
then deliver all background families, objects and priorities, display effects,
and finally the complete CPU/DMA access and runner outcome. All output uses the
canonical machine-owned 240×160 `BGR555_LE` raw-video path.

The five phases group the 18 current requirements into observable delivery
boundaries. Affine backgrounds belong with the other background families;
source-owned semantic vectors accompany every feature rather than waiting for
the final integration phase. That phase owns completeness of the cross-feature
evidence and the production bus/runner result.

## Phases

**Phase numbering:** Continue from completed Phase 10. Integer phases are planned
work; decimal IDs are reserved for later insertions.

- [x] **Phase 11: Sampled Rows and Transactional Visibility** - Callers can identify supported controls and observe ordered, failure-atomic raw-video rows.
- [ ] **Phase 12: Background Modes 0–5** - Text, affine, and bitmap backgrounds produce their declared semantic pixels from canonical video memory.
- [ ] **Phase 13: Objects and Layer Priorities** - Regular and affine objects combine with backgrounds and backdrop through one bounded composition policy.
- [ ] **Phase 14: Windows and Display Effects** - Region masks, blending, brightness, mosaic, and forced blank interact predictably.
- [ ] **Phase 15: Production Access and Runner Evidence** - CPU/DMA video-memory behavior and complete PPU scenes agree through the shared clock and real runner.

## Phase Details

### Phase 11: Sampled Rows and Transactional Visibility
**Goal**: Callers can determine the current display capabilities and obtain raw rows that reflect the declared ordered register/memory state without partial effects on refusal.
**Depends on**: Phase 10 (completed v2.0 raw-video path)
**Requirements**: PPU-03, PPU-04, PPU-06
**Success Criteria** (what must be TRUE):
  1. A caller can identify modes 0–5, their background sources and dimensions, current implementation status, and raw `BGR555_LE` output from the `agb` capability contract; invalid or unsupported controls have a deterministic observable result.
  2. Writes to registers, VRAM, palette RAM, and OAM before, at, and after the declared row boundary produce the expected sampled row from the ordered visible prefix, including same-cycle ordering.
  3. A caller can distinguish a complete frame from an invalid or incomplete epoch through the existing raw-video contract.
  4. Refused or capacity-limited work preserves machine state, video memory, frame contents, and caller output buffers, while an admitted exact-fit operation commits its complete result.
**Plans**: 8/8 execution plans complete; final local canonical acceptance and independent phase verification passed. See [verification](phases/11-sampled-rows-and-transactional-visibility/11-VERIFICATION.md) and [evidence](phases/11-sampled-rows-and-transactional-visibility/11-EVIDENCE.md).
**Wave 1**
- [x] 11-01-PLAN.md
- [x] 11-02-PLAN.md

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 11-03-PLAN.md
- [x] 11-04-PLAN.md

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 11-05-PLAN.md — focused checks complete; final canonical acceptance recorded under Plan 11-08

**Wave 4** *(recovery wave; consumes Plan 11-05 evidence and starts from last completed prerequisite 11-04)*
- [x] 11-06-PLAN.md — exact baseline triage and focused CPU/core/scheduler repairs

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 11-07-PLAN.md — portable Phase 10 plan references with byte-preservation checks

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 11-08-PLAN.md — exact plan-path proof, green canonical CI/JUnit/inventory, and measured iteration review

### Phase 12: Background Modes 0–5
**Goal**: Users of the canonical raw-video path can obtain expected text, affine, and bitmap background pixels across display modes 0–5.
**Depends on**: Phase 11
**Requirements**: BG-01, BG-02, BG-03
**Success Criteria** (what must be TRUE):
  1. Text backgrounds in modes 0 and 1 produce expected tile/map-size, scroll, flip, palette, transparency, and address-edge pixels from canonical video memory.
  2. Affine backgrounds in modes 1 and 2 produce expected signed and fractional transformed samples under the declared reference-update policy, with deterministic wrapping or out-of-area output.
  3. Bitmap modes 3, 4, and 5 produce expected direct/indexed colors, palette use, page selection, mode extents, and out-of-area pixels; the existing mode-3 result remains a regression anchor.
  4. Boundary and malformed-address vectors yield the declared output without reading neighboring memory outside the permitted source extent.
**Plans**: 8 execution plans across 8 waves.
**Wave 1**
- [x] 12-01-PLAN.md — Implement and expand mode 0–1 text backgrounds.

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 12-02-PLAN.md — Publish text support in profile and runner metadata.

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 12-03-PLAN.md — Add exact background selection controls and fast helper.

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 12-04-PLAN.md — Implement affine backgrounds and deterministic reference policy.

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 12-05-PLAN.md — Publish affine support in profile and runner metadata.

**Wave 6** *(blocked on Wave 5 completion)*
- [ ] 12-06-PLAN.md — Implement bitmap backgrounds for modes 3–5.

**Wave 7** *(blocked on Wave 6 completion)*
- [ ] 12-07-PLAN.md — Publish bitmap support in profile and runner metadata.

**Wave 8** *(blocked on Wave 7 completion)*
- [ ] 12-08-PLAN.md — Run final acceptance and measure iteration cost.

**Cross-cutting constraints:** <!-- gsd:generated -->
- A stale status, source, dimension or profile version is rejected with a causal diagnostic.
- Hardware qualification remains UNKNOWN and mode capability remains distinct from complete-frame validity.

Overlapping layers receive their complete shared priority contract in Phase 13.

### Phase 13: Objects and Layer Priorities
**Goal**: Raw frames combine regular and affine objects with enabled backgrounds and backdrop using stable priorities and bounded object work.
**Depends on**: Phase 12
**Requirements**: OBJ-01, OBJ-02, OBJ-03, COMP-01
**Success Criteria** (what must be TRUE):
  1. Regular objects produce expected shape/size, 1D/2D tile mapping, flip, palette, clipping, and transparent pixels, including mode-dependent OBJ tile availability.
  2. Affine objects produce expected transformed pixels from their selected matrices, with the declared signed fixed-point, bounding-field, clipping, transparency, and address limits.
  3. Overlapping objects, backgrounds, and backdrop resolve to expected colors under the declared enable, transparency, priority, and tie-breaking rules.
  4. A line that exceeds the declared object-work bound yields repeatable, inspectable selection and output under the stable ordering policy; that bound is labeled project policy where silicon behavior is unknown.
**Plans**: TBD

### Phase 14: Windows and Display Effects
**Goal**: Users can observe predictable region-specific layers, color effects, mosaic, and blanking in canonical raw frames.
**Depends on**: Phase 13
**Requirements**: COMP-02, COMP-03, COMP-04, COMP-05
**Success Criteria** (what must be TRUE):
  1. Rectangular and object-window overlaps select the declared region precedence and background/object/effect masks at interior, boundary, and outside coordinates.
  2. Alpha blending and brightness affect only eligible targets, with bounded validated coefficients and expected colors for target, transparency, and window interactions.
  3. Background and object mosaic repeat the declared anchor samples and dimensions, including clipping, window, and region-edge interactions.
  4. Forced blank produces its declared raw pixels and interacts consistently with sampled rows, transitions, and complete-frame validity.
**Plans**: TBD

### Phase 15: Production Access and Runner Evidence
**Goal**: A bounded production-runner scene exercises the supported PPU through CPU/DMA video-memory access and produces independently expected pixels with separate guest, time, and result observations.
**Depends on**: Phase 14
**Requirements**: PPU-05, VERIF-09, VERIF-10, VERIF-11
**Success Criteria** (what must be TRUE):
  1. CPU and DMA accesses to VRAM, palette RAM, and OAM follow one declared policy in drawing, HBlank, VBlank, and forced blank; before/at/after-boundary and same-cycle results agree with the canonical renderer and shared scheduler.
  2. Source-owned vectors assert expected colors and coordinates for every supported BG mode and composition feature, including bounds, priority, transparency, and feature interactions; a meaningful negative control detects incorrect pixels and hashes remain supplemental repeatability evidence.
  3. A bounded scenario through the real production bus and runner reports guest progress, machine cycles, raw pixels, and result metadata separately, with complete-frame status and failure-atomic refusal behavior visible.
  4. Fresh, split-run, reset, and interleaved-instance executions reproduce expected semantic pixels and raw output without shared mutable PPU state.
  5. Actual result metadata identifies the exact profile, no-BIOS/ROM-only scope, selected access/sampling policies, and unresolved evidence limits without claiming hardware qualification or broad retail compatibility.
**Plans**: TBD

## Verification and Delivery Constraints

- Every feature phase includes semantic pixel expectations and bounds/refusal checks at its smallest useful canonical seam. Phase 15 owns full vector coverage and integrated runner evidence, not the first tests.
- Choose and document the CPU/DMA access-policy matrix during Phase 11 contract planning; Phase 15 owns complete enforcement and production-path acceptance. Earlier feature phases must state any unenforced access behavior.
- Keep one canonical machine, shared scheduler/DMA arbiter, canonical VRAM/palette/OAM, and raw frame. Use bounded scanline work; add no second core, host renderer, host clock, or event per pixel.
- Preserve visible-line sampling and the selected 1006-cycle HBlank policy. HBlank-edge, DMA-latch, and within-line visibility disputes remain open with hardware qualification `UNKNOWN` unless new independent evidence resolves them.
- Use the existing C17/CMake/CTest and GCC/Clang paths, with no new dependency presumed. Cheap policy, identity, rights, and exact-selection checks precede longer work; important gates have meaningful negative controls and visible missing/skipped results.
- Keep one canonical local verification command aligned with the required CI path. Add CI work for distinct recurring value, inspect measured critical-path cost, and reuse unchanged valid evidence rather than rerunning whole matrices by habit.
- New fixture admission requires source, revision, recipe, digest, license, notices, BIOS needs, and permitted-use review. Original source ownership or a digest alone does not authorize redistribution. The existing generated guest remains local-only with redistribution unknown.
- No commercial ROMs, Nintendo BIOS, private saves, unreviewed fixtures, third-party suite admission, BIOS-stub authoring, hosted rerun, or publication is authorized by this roadmap. Existing exact budgets and no-retry stops remain in [STATE.md](STATE.md#blockersconcerns).
- Audio, cartridge saves, stable public API/package, physical hardware/controller qualification, and replacing Playstead's managed mGBA route remain outside this milestone. Publication, licensing, rights, physical evidence, and new subjective judgment remain explicit owner gates.

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 11. Sampled Rows and Transactional Visibility | 7/8 | In Progress — bounded repair sequence is underway; canonical CI acceptance remains blocked pending execution | - |
| 12. Background Modes 0–5 | 5/8 | In Progress | - |
| 13. Objects and Layer Priorities | 0/TBD | Not started | - |
| 14. Windows and Display Effects | 0/TBD | Not started | - |
| 15. Production Access and Runner Evidence | 0/TBD | Not started | - |

**Coverage:** 18/18 current v3.0 requirements assigned exactly once in
[REQUIREMENTS.md](REQUIREMENTS.md#traceability). No orphan or duplicate assignments.

## Open Carryovers and Next Milestones

- `DIAG-04` and `DET-02` remain open v1.0 evidence gaps; Phase 6 remains incomplete. They are not v3.0 requirement assignments or completion claims.
- G4 still gates BIOS-stub authoring; G8 and individual fixture review still gate the deferred mGBA deep lane. G6 remains accepted only for the original local v2.0 terminal demo; its managed RetroArch denial remains historical with no retry or policy bypass.
- Audio is the recommended next milestone, then cartridge/save devices, then API/consumer/package. These are horizon options, not approved requirements; a concrete adopter can justify revisiting integration order.
- See [PROJECT.md](PROJECT.md), [research summary](research/SUMMARY.md), and [scope recommendation](research/v3-ppu-milestone-recommendation.md) for decisions, provenance, tradeoffs, and evidence limits.

## Next Action

Execute Phase 11 Plan 11-08. Plans 11-06 and 11-07 completed their bounded
recovery work; the owner authorized this sequence through `$gsd-autonomous`.
Phase 12 remains unplanned until the canonical workflow and its complete
post-inventory are green.

---
*Roadmap created: 2026-11-19 for v3.0 PPU Modes and Composition.*
