# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed locally 2026-11-19; scoped Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed locally 2026-11-21; deterministic modes 0–5 and composition model verified. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- ✅ **v4.0 Audio** — Completed locally 2026-11-23; 13/13 requirements, four phases, and five integration flows verified. See [archive](milestones/v4.0-ROADMAP.md) and [audit](milestones/v4.0-MILESTONE-AUDIT.md).
- ✅ **v5.0 Deterministic Core API and Installed Consumer Proof** — Completed locally 2026-11-24; 9/9 requirements and both phases verified. See [archive](milestones/v5.0-ROADMAP.md) and [audit](milestones/v5.0-MILESTONE-AUDIT.md).
- ✅ **v6.0 Public API Host Session Readiness** — Completed locally 2026-11-25; 6/6 requirements, two phases, and two end-to-end flows verified. Audit records historical Playstead evidence and remote-main integration as explicit limits. See [archive](milestones/v6.0-ROADMAP.md) and [audit](milestones/v6.0-MILESTONE-AUDIT.md).

## Rolling Horizon

Refreshed after the local v6.0 closeout on 2026-11-25. No next milestone scope
has been selected. GSD milestone labels are planning identifiers, not Gamebloke
software versions.

### Near term

- **Recommended candidate — clean Playstead route-feasibility assessment.**
  Promote only after the owner selects a clean current source baseline and the
  assessment has separately authorized fixture use and an independent guest
  oracle. First establish whether Gamebloke offers a distinct host capability
  beside managed mGBA. Preserve mGBA as the default unless the owner approves a
  route change. The current v6.0 report describes its captured historical
  snapshot; its freshness guard refuses the changed live checkout.
- **Alternative — one consumer-driven save-device slice.** Promote when a
  named scenario requires save continuity and selects the exact device,
  expected outcome, permitted fixture, and persistence adapter. Do not infer
  save type from a title or filename.
- **Alternative — targeted compatibility or portability evidence.** Promote
  when a profile, observable claim, independent oracle, rights-cleared fixture,
  and runnable environment are identified. Keep legacy `DIAG-04`, `DET-02`,
  model-timing disputes, and host qualification as separate evidence gates.

### Mid term

- If a clean Playstead assessment shows a measurable value, plan an opt-in
  route that keeps mGBA available and default. Require separate route authority,
  fixture rights, and guest-oracle evidence before implementation.
- Add a save adapter only for a selected backup device and consumer result;
  keep persistent writes out of the deterministic core.
- Expand CPU, cartridge, and timing support from named evidence needs. Keep
  project-selected behavior distinct from hardware facts and compatibility
  claims.

### Long term

- Package or release an exact tested artifact only after licensing, fixture
  rights, package readiness, claim scope, and publication authority are
  resolved. Physical hardware qualification remains a separate owner gate.
- Broader compatibility claims require exact profiles, BIOS modes, cartridge
  and save devices, optional peripherals, and independent evidence.

### Decision notes

- v6.0 satisfied all six committed requirements locally. The full canonical
  CMake workflow and optional diagnostics passed; its audit records measured
  critical-path timings and explicit skips.
- A cheap hygiene check now blocks expensive CI jobs when repository
  consistency fails. The existing full test selection and required-check
  visibility remain intact; focused workflow mutation controls passed.
- No hosted CI run, `act` replay, Docker lane, or software release occurred for
  v6.0. A GitHub check of the current remote `main` found no open PR, status, or
  workflow run; the local milestone branch remains unpublished. Owner
  authorization and passing required hosted checks are needed before merge.
- The Playstead source snapshot changed after capture. The fit report remains
  bound to its recorded snapshot; current fit is unknown until a clean baseline
  is selected.
- Phase 6 `DIAG-04` and `DET-02`, hardware timing disputes, publication,
  licensing, and physical qualification remain separate evidence or owner
  gates. No result in v6.0 closes them.
