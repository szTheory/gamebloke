# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and two evidence gaps carried forward. See [archive](milestones/v1.0-ROADMAP.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed 2026-11-19; all four phases verified and the scoped local Playable 0 demo accepted. See [archive](milestones/v2.0-ROADMAP.md) and [closeout audit](milestones/v2.0-MILESTONE-AUDIT.md).
- 📋 **v3.0 PPU Completion** — Planned: remaining display modes, OBJ, windows, blending, mosaic, and video-memory contention/latch behavior.
- 📋 **v4.0 Audio** — Planned: PSG, Direct Sound FIFO, and canonical raw model audio.
- 📋 **v5.0 Cartridge and Saves** — Planned: declared cartridge and save-device behavior.
- 📋 **v6.0 API, Consumer, Packaging** — Planned: stable API, relocatable package, installed consumers, and optional user-supplied official BIOS mode.

GSD milestone numbers are planning labels, not Gamebloke software versions. Completion here does not mean a software release, hardware qualification, or broad compatibility.

## Open Carryovers and Gates

- v1.0 evidence gaps `DIAG-04` and `DET-02` remain open; see the archived v1.0 audit and [current state](STATE.md#deferred-items).
- BIOS-stub authoring (`BIOS-01`) remains behind owner approval. The mGBA deep lane (`EVID-07`) remains deferred pending terms and per-fixture rights/provenance review.
- External publication, physical hardware/controller qualification, and redistribution remain separate owner decisions.

## Next Action

Start the next milestone with `$gsd-new-milestone` when ready. No v3.0 scope or requirements have been started.

---

*Roadmap updated: 2026-11-19 after v2.0 closeout and scoped owner acceptance.*
