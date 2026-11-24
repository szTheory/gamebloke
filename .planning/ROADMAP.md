# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed locally 2026-11-19; scoped Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed locally 2026-11-21; deterministic modes 0–5 and composition model verified. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- ✅ **v4.0 Audio** — Completed locally 2026-11-23; 13/13 requirements, four phases, and five integration flows verified. Audit verdict: documented tech debt only; no software release or hardware qualification claimed. See [archive](milestones/v4.0-ROADMAP.md) and [audit](milestones/v4.0-MILESTONE-AUDIT.md).
- 🚧 **v5.0 Deterministic Core API and Installed Consumer Proof** — Phases 20–21.

## v5.0 — Deterministic Core API and Installed Consumer Proof

**Status:** Owner-approved scope. Phase 20 is complete locally; Phase 21 is next for discussion and planning.

**Goal:** Prove that a developer or integrator can use Gamebloke's canonical
GBA machine through a stable, portable API and obtain bounded, reproducible,
machine-level evidence from an installed consumer.

**Why this order:** Gamebloke's differentiator beside Playstead's managed mGBA
route is still a product hypothesis. Before Phase 20, `gbl_machine` was an
internal, non-stable interface and CMake installed only `gamebloke-run`. Phase
20 now provides an opaque API and a named static install component. Phase 21
will prove that installed boundary with an independent consumer and evaluate
LibFuturo against actual machine-level observations. Adding save-chip models
first would expand machine behavior before a consumer can use the core; the
current Playstead GBA route already has managed save transfer/restore, so a
save-device matrix alone would not establish a distinct Gamebloke benefit.

**Scope:**

- Define a versioned, opaque, per-instance C API for the currently supported
  `agb`, direct-start, ROM-only profile: lifecycle, content load/unload, reset,
  timestamped logical input, bounded advancement, caller-owned raw A/V output,
  result metadata, and documented refusal postconditions.
- Install the API and prove it with a small C consumer relocated away from the
  build tree. The consumer must use only installed public headers and libraries.
- Use an original, provenance-recorded BIOS-free diagnostic to assert guest
  semantics and literal video/audio observations across fresh runs. Hashes are
  repeatability data, not the correctness oracle.
- Evaluate LibFuturo against the real Gamebloke API and consumer. Adoption or
  contract changes remain a later decision based on measured fit.

**Success evidence:** The relocated consumer loads the named profile and
content, applies explicit timed input, advances within a bound, reads
caller-owned outputs and result claims, and demonstrates semantic repeatability
and failure-atomic refusal behavior without depending on Gamebloke private
structures.

**Out of scope:** Backup-device implementation, a Playstead route change,
compatibility superiority claims, hardware qualification, public release, and
public package publication. Per-phase discussion and execution remain with the
downstream OpenGSD workflows. Fixture provenance and permitted use remain
explicit; no commercial ROM, BIOS dump, or private save enters source control
or public CI.

## Rolling Horizon After v5.0

### Near term after v5.0

- **Cartridge and Saves:** Revisit explicit save-device profiles after the
  installed consumer identifies a concrete need. The historical candidate
  covers SRAM/FRAM, Flash, and EEPROM; scope each device by declared identity,
  exact evidence, and host-owned persistence. Never infer a device from a title
  or filename.

### Mid term

- **Playstead integration:** Consider a side-by-side Gamebloke route only after
  the public API and consumer establish a useful capability. Keep the managed
  mGBA route; replacement requires a separate owner decision.
- **Compatibility and portability:** Prioritize exact profile and fixture
  evidence. Keep `DIAG-04`, `DET-02`, unresolved PPU timing disputes, rights,
  hosted runs, and physical qualification as separate gates with their current
  authority and evidence limits.

### Long term

- **Packaging and release:** Keep installed-consumer packaging tests separate
  from publishing a public package or release. Licensing, rights, package
  readiness, qualification, and publication authority remain explicit owner
  gates.

The sequence beyond v5.0 is a rolling horizon, not a commitment to specific
milestone versions. GSD milestone numbers are planning labels, not Gamebloke
software versions.

## Phases

- [x] **Phase 20: Bounded Core API** - C callers can run independent machines, observe raw outputs, and handle refusals through the versioned public API. (completed 2026-11-23)
- [ ] **Phase 21: Installed Consumer Proof and LibFuturo Fit** - A relocated C consumer reproduces guest and raw A/V evidence and provides a concrete basis for evaluating LibFuturo.

<details>
<summary>✅ Completed v4.0 Audio phases — 2026-11-23</summary>

- [x] Phase 16: Audio Registers and Capture Contract — 3/3 plans
- [x] Phase 17: PSG Channels and Sequencer — 4/4 plans
- [x] Phase 18: Direct Sound and FIFO DMA — 4/4 plans
- [x] Phase 19: Stereo Mixing and Runner Evidence — 6/6 plans

</details>

<details>
<summary>✅ Completed v3.0 PPU phases — 2026-11-21</summary>

- [x] Phase 11: Sampled Rows and Transactional Visibility — 8/8 plans
- [x] Phase 12: Background Modes 0–5 — 8/8 plans
- [x] Phase 13: Objects and Layer Priorities — 8/8 plans
- [x] Phase 14: Windows and Display Effects — 11/11 plans
- [x] Phase 15: Production Access and Runner Evidence — 5/5 plans

</details>

## Phase Details

### Phase 20: Bounded Core API

**Goal**: A C caller can run and observe isolated instances of the canonical `agb` machine through a versioned, opaque API with bounded work and failure-atomic refusals.
**Depends on**: Phase 19 (completed canonical machine and raw A/V baseline)
**Requirements**: API-01, API-02, API-03, API-04, API-05
**Success Criteria** (what must be TRUE):

  1. A C caller can create and destroy independent instances using public types, select the `agb` direct-start, ROM-only profile, and load, reset, and unload content. Interleaved operations on two instances yield each instance’s isolated result.
  2. A caller can submit timestamped logical input and advance within a caller-specified work bound. Results separately report logical input observations, guest progress, machine time, and the outcome of the advance.
  3. A caller can obtain raw video and audio in caller-owned buffers and read structured metadata identifying the machine profile, observations, supported behavior, and limits of the model evidence.
  4. Invalid arguments, rejected profiles or content, unsupported requests, insufficient output capacity, and exhausted work bounds return their documented statuses. A caller can verify the documented machine and buffer postconditions and retry a valid request with the same result as an equivalent run without the refusal.

**Plans**: 1/1 plans complete

- [x] 20-01-PLAN.md — Run the opaque C API against the canonical machine and verify lifecycle, media, and atomic refusals.

### Phase 21: Installed Consumer Proof and LibFuturo Fit

**Goal**: A developer can reproduce machine-level evidence through a relocated, public-only C consumer and assess LibFuturo with results from that consumer.
**Depends on**: Phase 20
**Requirements**: CONS-01, EVID-01, EVID-02, INT-01
**Success Criteria** (what must be TRUE):

  1. A developer can build and run the standalone C consumer outside the source and build trees against only the public headers and libraries from a relocated install prefix, using documented dependencies and platform assumptions.
  2. The installed consumer runs a rights-reviewed, original BIOS-free diagnostic under its permitted use. Its manifest identifies the source, revision, build recipe, digest, license and notices, BIOS mode, expected results, and permitted use.
  3. Fresh runs with the same timed input and work bound reproduce the expected guest result, literal raw video values, and literal raw audio samples. The checks detect deviations from those expectations independently of hashes. Hashes record identity and repeatability.
  4. Through the installed API, a developer can reproduce the documented refusal statuses and compare the machine observations and buffer contents before and after invalid size, capacity, and budget requests.
  5. The LibFuturo fit report cites observations from the actual API and relocated consumer, identifies fits and gaps in input timing, work bounds, output ownership, and evidence semantics, and gives the evidence needed for a separate dependency or contract decision.

**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 20. Bounded Core API | 1/1 | Complete    | 2026-11-23 |
| 21. Installed Consumer Proof and LibFuturo Fit | 0/TBD | Not started | - |

## Open Carryovers and Evidence Limits

- v1.0 `DIAG-04` and `DET-02` remain open; local results do not substitute for the missing source-bound diagnostic or hosted Linux x64 evidence.
- The selected PPU HBlank policy and HBlank-edge, DMA-latch, and within-line visibility disputes remain unresolved; hardware qualification is UNKNOWN.
- The v4.0 audio diagnostic is permitted for local verification only; redistribution rights remain unknown. Host playback, PWM/analog output, and physical audio qualification are outside the completed scope.
- Physical hardware/controller qualification, licensing, copyrighted content, external publication, and replacement of Playstead's managed mGBA route remain owner gates.

See [PROJECT.md](PROJECT.md), [STATE.md](STATE.md#deferred-items), and the milestone archives for detailed scope, evidence, and limits.
