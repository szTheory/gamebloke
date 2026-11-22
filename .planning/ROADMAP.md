# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed 2026-11-19; scoped local Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed 2026-11-21; deterministic modes 0–5 and composition model verified locally. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- 📋 **Next milestone:** Audio is the recommended direction, followed by Cartridge and Saves, then API, Consumer, and Packaging. These are horizon options; no next milestone scope or requirements have been selected.

GSD milestone numbers are planning labels, not Gamebloke software versions.

## Completed v3.0 Phases

<details>
<summary>✅ v3.0 PPU Modes and Composition — completed 2026-11-21</summary>

- [x] Phase 11: Sampled Rows and Transactional Visibility — 8/8 plans
- [x] Phase 12: Background Modes 0–5 — 8/8 plans
- [x] Phase 13: Objects and Layer Priorities — 8/8 plans
- [x] Phase 14: Windows and Display Effects — 11/11 plans
- [x] Phase 15: Production Access and Runner Evidence — 5/5 plans

</details>

## Open Carryovers and Evidence Limits

- v1.0 `DIAG-04` and `DET-02` remain open. Local results do not substitute for the missing source-bound diagnostic or hosted Linux x64 evidence.
- The selected 1006-cycle HBlank policy and HBlank-edge, DMA-latch, and within-line visibility disputes remain unresolved; hardware qualification is UNKNOWN.
- The generated diagnostic guest remains local-only with redistribution rights unknown. G4 still gates BIOS-stub authoring; G8 and per-fixture review gate the deferred mGBA deep lane.
- Physical hardware/controller qualification, publication, licensing, copyrighted content, and replacement of Playstead's managed mGBA route remain explicit owner gates.

See [PROJECT.md](PROJECT.md), [STATE.md](STATE.md#deferred-items), and the milestone archives for detailed decisions, evidence, and limits.
