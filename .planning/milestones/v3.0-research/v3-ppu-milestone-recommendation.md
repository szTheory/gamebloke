# v3.0 Scope Recommendation: PPU Modes and Composition

**Prepared:** 2026-11-19
**Purpose:** Upstream scoping input for the next OpenGSD milestone. This is a
recommendation and evidence summary, not a formal requirements set, phase plan,
hardware qualification, or permission to publish new fixtures.

## Recommendation

Keep the PPU as the next milestone, but call it **PPU Modes and Composition**
rather than “PPU Completion.” Target deterministic functional coverage of GBA
display modes 0–5, background and OBJ layers, and their common composition
features through the existing raw-video path. Name the supported profile,
sampling boundaries, access rules, and selected policies. Do not imply a
cycle-perfect PPU, universal retail compatibility, or hardware qualification.

This is the best next slice because the completed v2.0 machine has a useful
shared clock, DMA arbiter, mode-3 frame output, and owner-accepted local demo,
while modes 0–2 and 4–5 and the usual layer features remain explicitly absent.
Adding them extends the current core’s deterministic and inspectable behavior
beside Playstead’s managed mGBA route; it does not claim to replace that route.

## Evidence State

### Owner decisions and standing constraints

- The owner selected Gamebloke as a GBA emulator and accepts the local v2.0
  Playable 0 demo for its stated scope.
- Keep machine execution separate from host presentation and external effects;
  use one canonical machine implementation and a small adapter surface.
- Prefer a small dependency tree; source-built tests or a small local helper are
  preferable to adding a dependency without concrete value.
- Commercial ROMs, Nintendo BIOS bytes, private saves, and fixtures without
  per-fixture rights are not eligible for source control or public CI.
- Accuracy claims must identify exact mode/profile and evidence. Emulator
  agreement, hashes, title screens, and suite scores do not establish hardware
  accuracy or broad compatibility.
- The owner asks future milestone and phase discussions to present an evidence-
  backed recommendation and options, use relevant parallel research and an
  adversarial pass, and stop when more work is unlikely to change the result.
  Verification should be automated early where it provides useful feedback;
  one-off checks may remain local, and CI requires recurring value.

### Current project observations

- Phase 10 implements PPU line/status events, IRQ and blank-trigger DMA
  integration, mode-3 BGR555 output, a selected visible-line-start row sample,
  invalid/incomplete frame reporting, and a local source-built guest.
- Existing tests already check line boundaries, semantic pixels, frame validity,
  refusal postconditions, exact selectors/omission controls, and fresh/reset/
  interleaved repeatability. Extend these patterns rather than create a parallel
  evidence system.
- HBlank is selected at 1006 master cycles for deterministic project behavior;
  `disp-hblank-flag-edge` remains disputed and hardware qualification remains
  `UNKNOWN`. The current row-sample timing and DMA latch policy also have
  explicit limitations. Game Pak prefetch is not modeled.
- The existing PPU draws from canonical VRAM, palette RAM, and OAM through the
  shared machine and bus. Keep this ownership boundary; do not add duplicate
  mutable video memory or a second clock.
- The v1.0 `DIAG-04` and `DET-02` gaps, the G4 BIOS-authoring gate, G8 mGBA-suite
  and fixture review, publication, and physical qualification remain separate
  carryovers. This recommendation does not renew any of them.

### Source facts

- Nintendo’s public hardware page confirms 240×160 output, 32,768 possible
  colors, 511 simultaneous colors in character mode, and 32,768 in bitmap mode.
- Nintendo’s *AGB Programming Manual v1.1* describes BG modes 0–2 as character
  formats and 3–5 as bitmap formats; it specifies text and affine backgrounds,
  OBJ attributes and mapping, window masks, palettes, mosaic, and color effects.
  The manual is primary-source content but the available copies are hosted on
  third-party mirrors. The linked copy marks itself confidential and restricts
  reproduction. Use it as a cited reference only; do not copy or redistribute
  its text or figures.
- The manual does not resolve the project’s disputed HBlank status offset,
  exact within-line pixel/write visibility, DMA latch behavior, or full silicon
  contention. The project must preserve uncertainty instead of extrapolating
  undocumented behavior from a functional register description.

### Inference and recommendation

Modes 0–5 and their layer pipeline are a coherent functional boundary: modes
0–2 combine tiled and affine backgrounds, while modes 3–5 use bitmap layouts;
OBJ and composition features sit across those display modes. A shared bounded
scanline renderer can cover common scenes without enqueuing every dot or pixel.
That is only a project-model choice. If a specific scene requires finer timing,
measure and name that requirement before choosing a finer-grained implementation.

## Recommended Scope

### In scope

1. **Background modes 0–5.** Text/tile layers, affine layers, bitmap formats,
   mode-specific dimensions and page selection, palette lookup, scroll and
   affine reference parameters, transparent/outside behavior, and bounded
   address calculations.
2. **Layer and OBJ composition.** Backdrop, enable masks, transparency,
   priorities and documented tie-breaking; regular and affine objects, shape
   and size, 1D/2D tile mapping, palette modes, OBJ order, and an explicit
   bounded per-line selection policy.
3. **Display composition features.** Rectangular and OBJ windows with explicit
   precedence, per-region layer/effect masks, alpha blending, brightness
   effects, BG/OBJ mosaic, forced blank, and invalid-mode/control behavior.
4. **Machine/bus integration.** Keep the shared scheduler, DMA arbiter, event
   ordering, canonical VRAM/palette/OAM, preview/commit refusal contract, and
   raw output contract. Specify how completed writes become visible at selected
   scanline boundaries. Model access behavior for supported line/blank states
   without claiming unsupported sub-cycle contention.
5. **Evidence.** Hand-derived expected pixels and machine observations for each
   supported feature, integrated with the real machine/runner path. Keep guest
   progress, machine cycles, raw video bytes, result metadata, and host display
   separate. Preserve exact source, build recipe, digest, BIOS mode, notices,
   license, and permitted use for any fixture that crosses a fixture boundary.

### Out of scope

- Dot-by-dot or cycle-perfect PPU emulation and an event per pixel.
- Resolving `disp-hblank-flag-edge`, DMA-latch, or within-line latch disputes;
  do not change the selected 1006-cycle project policy or its evidence labels
  without new independent evidence.
- Broad retail compatibility, commercial ROMs, third-party suite admission,
  Nintendo BIOS bytes, or any new redistribution claim.
- Audio, cartridge backup devices, physical hardware/controller qualification,
  stable public API, package publication, or replacing Playstead’s mGBA path.
- Green swap and other low-value/uncertain controls unless a concrete named
  use case and source-backed test justify adding them.

### Planning boundaries

Use this sequence as input, not a prewritten OpenGSD plan:

1. Lock the mode/profile/output contract and register/pixel vectors.
2. Add background modes and address/palette behavior.
3. Add OBJ rendering and shared priorities/composition.
4. Add windows, color effects, mosaic, and forced-blank interactions.
5. Close memory-access policies and production-path integration evidence.

The formal roadmapper should group these into phases with independent outcomes,
map each requirement once, and avoid a single phase that implements every PPU
feature before any useful integration result exists.

## Verification Direction

Run checks from cheapest and most selective to most integrated:

1. Validate profile/register policy, evidence/dispute links, fixture manifests,
   build/source identity, exact test selection, and missing-test negative
   controls before longer work.
2. Use synthetic source-owned machine states and semantic pixel vectors for
   colors, tile and bitmap addresses, page selection, palettes, flips, affine
   transforms, transparency, priority ties, OBJ overlap/limits, window edges,
   effect masks, coefficients, mosaic boundaries, and forced blank.
3. Add before/at/after timing and write-visibility cases at each selected
   scanline boundary. Check same-cycle event ordering, line/frame rollover,
   split-run replay, independent instances, invalid output, and failure-atomic
   refusal behavior.
4. Add a bounded production-runner scene that writes through the real bus and
   reports semantics through existing result/trace surfaces. A new guest ROM
   is optional; synthetic instructions or direct test-owned setup can provide
   integration proof without importing unreviewed ROM assets.
5. Use raw frame digests only for repeatability, paired with independent
   hand-derived pixel expectations. Emulator comparisons may help find
   differences, but neither agreement nor a suite’s label is a hardware oracle.
6. Promote stable checks to CI only when they catch a distinct recurring
   failure class at worthwhile runtime and maintenance cost. Measure incremental
   critical-path time; use local `act` only where it faithfully reproduces a
   relevant Actions behavior sooner than the existing local command.

Objective output correctness should not require owner UAT. Keep human work for
judgments automation cannot supply, such as physical hardware measurements,
publication/rights authority, or a new subjective product decision.

## Options and Tradeoffs

| Option | Recommendation | Rationale and cost |
|---|---|---|
| **PPU Modes and Composition next** | **Recommend** | Addresses the largest visible functional gap after mode 3 and extends the existing machine timing/output path. Scope needs several dependent phases; line-level policy must remain explicit. |
| Audio next | Keep as v4.0 | Makes the accepted local demo richer and creates the raw-audio contract. It leaves common GBA visuals unsupported. Promote only if audio becomes the primary user outcome. |
| Cartridge and saves next | Keep as v5.0 | Makes locally supplied content more useful and adds persistence. Save protocols and content rights create distinct scope; do not guess device type. |
| API/consumer/package next | Keep as v6.0, but retain earlier seam checks | A stable boundary should follow clearer A/V/media semantics. Waiting creates integration risk, so keep a small installed-consumer proof earlier when it is useful. |

Revisit the sequence at the next boundary with current product and consumer
evidence. Do not treat these horizon milestones as locked scope.

## Sources and Limits

- [Nintendo Game Boy Advance technical details](https://www.nintendo.com/en-gb/Hardware/Nintendo-History/Game-Boy-Advance/Game-Boy-Advance-627139.html) — official high-level display dimensions and color modes.
- [Nintendo AGB Programming Manual v1.1 (PDF mirror)](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) — primary-source technical text: §§5.1–5.2 display modes; §§6.1–6.4 BG/OBJ behavior and priority; §7 palettes; §8 windows; §9 color effects. This mirror is not the publisher; observe the manual’s restrictions and do not reproduce it.
- [mGBA Game Boy Advance Test Suite](https://github.com/mgba-emu/suite) — candidate test source. The repository labels its source as MIT, but inspect each generated fixture, embedded asset, BIOS requirement, and permitted use before admitting it.
- [jsmolka GBA tests](https://github.com/jsmolka/gba-tests) and [NanoBoyAdvance hardware tests](https://github.com/nba-emu/hw-test) — candidate observations only. Review the exact cases and terms; test descriptions and matching emulator output do not establish independent hardware truth.
- [GBATEK](https://rust-console.github.io/gbatek-gbaonly/) and [Tonc](https://gbadev.net/tonc/) — useful secondary implementation references, not primary hardware measurements.

No third-party ROM or copied emulator implementation is proposed. No dependency,
external fixture, hardware run, hosted CI run, release, or publication was
performed for this scoping work.
