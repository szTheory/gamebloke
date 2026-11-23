# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed locally 2026-11-19; scoped Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed locally 2026-11-21; deterministic modes 0–5 and composition model verified. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- ✅ **v4.0 Audio** — Completed locally 2026-11-23; 13/13 requirements, four phases, and five integration flows verified. Audit verdict: documented tech debt only; no software release or hardware qualification claimed. See [archive](milestones/v4.0-ROADMAP.md) and [audit](milestones/v4.0-MILESTONE-AUDIT.md).
- 📋 **Future milestones:** Cartridge and Saves, then API, Consumer, and Packaging. Revisit scope at each milestone boundary.

GSD milestone numbers are planning labels, not Gamebloke software versions.

## Phases

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

## Open Carryovers and Evidence Limits

- v1.0 `DIAG-04` and `DET-02` remain open; local results do not substitute for the missing source-bound diagnostic or hosted Linux x64 evidence.
- The selected PPU HBlank policy and HBlank-edge, DMA-latch, and within-line visibility disputes remain unresolved; hardware qualification is UNKNOWN.
- The v4.0 audio diagnostic is permitted for local verification only; redistribution rights remain unknown. Host playback, PWM/analog output, and physical audio qualification are outside the completed scope.
- Physical hardware/controller qualification, licensing, copyrighted content, external publication, and replacement of Playstead's managed mGBA route remain owner gates.

See [PROJECT.md](PROJECT.md), [STATE.md](STATE.md#deferred-items), and the milestone archives for detailed scope, evidence, and limits.
