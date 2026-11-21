# Requirements: Gamebloke

**Defined:** 2026-11-19
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior. Every result says exactly what it proves and what it does not.

## Current Milestone: v3.0 PPU Modes and Composition

Requirements use the prepared owner scope and the research summary. They describe
deterministic software-model behavior for the named `agb` profile; they do not
claim cycle accuracy, hardware qualification, or broad compatibility.

### PPU Contract and Integration

- [x] **PPU-03**: The `agb` profile declares the supported display-mode capability matrix, per-mode background sources and dimensions, raw `BGR555_LE` output, and deterministic handling of invalid or unsupported display controls.
- [x] **PPU-04**: Each captured row is rendered from an explicitly sampled register and memory view at a declared machine boundary; frame completeness and invalidation remain observable through the existing raw-video contract.
- [ ] **PPU-05**: The machine uses one documented, deterministic policy for CPU and DMA access to VRAM, palette RAM, and OAM across supported drawing, HBlank, VBlank, and forced-blank states; results identify this as project policy where hardware behavior is not independently established.
- [x] **PPU-06**: Ordered register, VRAM, palette-RAM, and OAM writes visible at a row boundary are reflected consistently by the canonical renderer, and refused or capacity-limited operations leave machine state and output unchanged.

### Backgrounds

- [x] **BG-01**: Text backgrounds in modes 0 and 1 render supported tile/map sizes, scrolling, flips, tile/palette selection, transparency, and bounded address behavior from canonical video memory.
- [x] **BG-02**: Affine backgrounds in modes 1 and 2 render using declared signed fixed-point transform and reference-update policies, with deterministic wrap/out-of-area behavior and bounded address calculations.
- [x] **BG-03**: Bitmap backgrounds in modes 3, 4, and 5 render their declared direct or indexed color formats, mode extents, palette use, page selection, format-appropriate transparency, and deterministic out-of-area behavior.

### Objects

- [x] **OBJ-01**: Regular objects render supported shape/size, 1D/2D mapping, flips, palette modes, clipping, transparency, and mode-dependent object tile availability from canonical OAM and video memory.
- [x] **OBJ-02**: Affine objects render from their selected transform matrices with declared signed fixed-point, bounding-field, clipping, transparency, and address-bound behavior.
- [x] **OBJ-03**: Object selection and composition use a documented stable ordering and explicit bounded per-line work policy; exceeding that policy produces deterministic, inspectable output.

### Composition

- [x] **COMP-01**: A shared compositor combines enabled backgrounds, objects, and backdrop using declared transparency, priority, and tie-breaking rules.
- [ ] **COMP-02**: Rectangular and object windows apply declared precedence and per-region background, object, and effect masks to the shared composition path.
- [ ] **COMP-03**: Alpha blending and brightness effects apply only to declared eligible layer targets and use bounded, validated coefficients.
- [ ] **COMP-04**: Background and object mosaic use declared sampling anchors and dimensions and produce deterministic boundary behavior.
- [ ] **COMP-05**: Forced blank has explicit, deterministic raw-output behavior and composes correctly with the declared line-sampling and frame-validity rules.

### Verification and Production Path

- [ ] **VERIF-09**: Source-owned semantic pixel vectors assert expected colors and coordinates for every supported background mode and composition feature, including important bounds, priority, transparency, and interaction cases; repeatability hashes supplement these independent expectations.
- [ ] **VERIF-10**: A bounded machine/runner scenario exercises supported PPU features through the production bus and canonical renderer and reports guest progress, machine cycles, raw pixels, and result metadata as separate observations.
- [ ] **VERIF-11**: Fresh, split-run, reset, and interleaved-instance cases produce the expected semantic pixels and repeatable raw output without sharing mutable PPU state.

## Future Requirements

- **AUD-01**: Deterministic PSG and Direct Sound/FIFO output in machine-time units, separate from host resampling and playback — later audio milestone.
- **CART-01**: Declared 32 KiB SRAM backup-device behavior and safe persistent save import/export — later cartridge and saves milestone.
- **API-01**: A stable public lifecycle, content, input, advancement, output ownership, reset, error, and refusal contract — later API/consumer milestone.
- **PKG-01**: A relocatable package and installed consumer that exercises the public contract — later API/consumer milestone.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Dot-by-dot or cycle-perfect PPU timing | This milestone targets a deterministic functional software model. The selected 1006-cycle HBlank policy remains unchanged, and the HBlank edge, DMA-latch, and within-line visibility disputes remain open. |
| Hardware qualification or broad retail-game compatibility | Current source and semantic vectors do not provide independent silicon evidence or per-game coverage. |
| Commercial ROMs, Nintendo BIOS bytes, private saves, or unreviewed third-party fixtures | Rights and security require per-fixture provenance, license, notice, BIOS, digest, and permitted-use review; hashes alone do not grant permission. |
| Audio, cartridge saves, public API/package publication, and physical hardware/controller qualification | These have distinct product scope, integration evidence, rights, or owner authority gates. |
| Replacing Playstead's managed mGBA route | This milestone adds an inspectable canonical raw-video path; it does not establish a replacement decision. |

## Traceability

The roadmap assigns every current v3.0 requirement to exactly one phase.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PPU-03 | Phase 11 | Complete |
| PPU-04 | Phase 11 | Complete |
| PPU-05 | Phase 15 | Pending |
| PPU-06 | Phase 11 | Complete |
| BG-01 | Phase 12 | Complete |
| BG-02 | Phase 12 | Complete |
| BG-03 | Phase 12 | Complete |
| OBJ-01 | Phase 13 | Complete |
| OBJ-02 | Phase 13 | Complete |
| OBJ-03 | Phase 13 | Complete |
| COMP-01 | Phase 13 | Complete |
| COMP-02 | Phase 14 | Pending |
| COMP-03 | Phase 14 | Pending |
| COMP-04 | Phase 14 | Pending |
| COMP-05 | Phase 14 | Pending |
| VERIF-09 | Phase 15 | Pending |
| VERIF-10 | Phase 15 | Pending |
| VERIF-11 | Phase 15 | Pending |

**Coverage:** 18/18 current requirements mapped exactly once; 0 orphaned and 0 duplicate assignments. Future requirements and historical carryovers are outside this milestone's mapping.

---
*Requirements defined: 2026-11-19*
*Last updated: 2026-11-20 after Phase 12 verification*
