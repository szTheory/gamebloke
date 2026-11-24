# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed locally 2026-11-19; scoped Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed locally 2026-11-21; deterministic modes 0–5 and composition model verified. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- ✅ **v4.0 Audio** — Completed locally 2026-11-23; 13/13 requirements, four phases, and five integration flows verified. Audit verdict: documented tech debt only; no software release or hardware qualification claimed. See [archive](milestones/v4.0-ROADMAP.md) and [audit](milestones/v4.0-MILESTONE-AUDIT.md).
- ✅ **v5.0 Deterministic Core API and Installed Consumer Proof** — Completed locally 2026-11-24; all 9 requirements satisfied and both phases verified. Audit retains one non-blocking relocated-consumer evidence limit. No software release or hardware qualification claimed. See [archive](milestones/v5.0-ROADMAP.md) and [audit](milestones/v5.0-MILESTONE-AUDIT.md).

## Phases

<details>
<summary>✅ v5.0 Deterministic Core API and Installed Consumer Proof — COMPLETED LOCALLY 2026-11-24</summary>

- [x] Phase 20: Bounded Core API (1/1 plan) — completed 2026-11-23
- [x] Phase 21: Installed Consumer Proof and LibFuturo Fit (2/2 plans) — completed 2026-11-24

</details>

<details>
<summary>✅ v4.0 Audio phases — completed 2026-11-23</summary>

- [x] Phase 16: Audio Registers and Capture Contract — 3/3 plans
- [x] Phase 17: PSG Channels and Sequencer — 4/4 plans
- [x] Phase 18: Direct Sound and FIFO DMA — 4/4 plans
- [x] Phase 19: Stereo Mixing and Runner Evidence — 6/6 plans

</details>

<details>
<summary>✅ v3.0 PPU phases — completed 2026-11-21</summary>

- [x] Phase 11: Sampled Rows and Transactional Visibility — 8/8 plans
- [x] Phase 12: Background Modes 0–5 — 8/8 plans
- [x] Phase 13: Objects and Layer Priorities — 8/8 plans
- [x] Phase 14: Windows and Display Effects — 11/11 plans
- [x] Phase 15: Production Access and Runner Evidence — 5/5 plans

</details>

## Rolling Horizon

No next milestone scope has been selected. Use these alternatives as the next
milestone discussion input, not as committed scope:

### Near term

- Revisit cartridge backup-device profiles when a consumer demonstrates a
  concrete need. Bind each device to declared identity and host-owned
  persistence; never infer save hardware from a title or filename.

### Mid term

- Consider a side-by-side Gamebloke route in Playstead only after the public API
  and consumer establish useful capability. Keep the managed mGBA route unless
  the owner separately approves a change.
- Prioritize exact-profile portability and compatibility evidence, keeping
  `DIAG-04`, `DET-02`, hardware timing disputes, rights, hosted runs, and
  physical qualification as separate gates.

### Long term

- Keep installed-consumer packaging tests separate from public package or
  software release work. Licensing, rights, package readiness, qualification,
  and publication authority remain owner gates.

GSD milestone numbers are planning labels, not Gamebloke software versions.
