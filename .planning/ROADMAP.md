# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed 2026-11-19; scoped local Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed 2026-11-21; deterministic modes 0–5 and composition model verified locally. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- 🚧 **v4.0 Audio** — Approved 2026-11-22; phase work has not started. Phases 16–19 are scoped below.
- 📋 **Future milestones:** Cartridge and Saves, then API, Consumer, and Packaging. Revisit scope at each milestone boundary.

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

## v4.0 — Audio

**Milestone Goal:** The canonical `agb` machine produces deterministic sound
from all four PSG channels and Direct Sound A/B on guest time. The headless
runner captures raw audio and reports behavior from an original, rights-cleared
diagnostic. The reproducible capture and evidence contract is Gamebloke's
intended addition beside Playstead's managed mGBA route.

Gamebloke captures interleaved stereo signed 16-bit little-endian PCM at
32,768 frames per second, one frame every 512 `agb` master cycles. The fixed
cadence is the project's capture policy, aligned to reset cycle 0 with the
first frame at cycle 512. This is a tap before PWM conversion, not an analog
output model. The profile states the SOUNDBIAS register policy; writes do not
retime or quantize this stream. Host playback, device selection, resampling,
variable-rate capture, PWM-carrier emulation, and analog output are outside
v4.0. Hardware qualification remains UNKNOWN.

## Phases

- [x] **Phase 16: Audio Registers and Capture Contract** - Guests can exercise supported sound control registers and callers can observe the fixed sample grid with bounded capture. (completed 2026-11-22)
- [x] **Phase 17: PSG Channels and Sequencer** - Guests can drive all four PSG channels through supported register controls on guest time. (completed 2026-11-22)
- [ ] **Phase 18: Direct Sound and FIFO DMA** - Guests can consume FIFO A/B samples on timer overflow and refill both FIFOs through shared DMA arbitration.
- [ ] **Phase 19: Stereo Mixing and Runner Evidence** - Callers can capture the combined stereo signal with repeatable samples, guest results, and explicit support limits.

## Phase Details

### Phase 16: Audio Registers and Capture Contract

**Goal**: Guests can control supported audio state through the canonical bus, and callers can request bounded raw capture on the declared pre-PWM sample grid before sound sources are added.
**Depends on**: Phase 15 (completed canonical machine and runner baseline)
**Requirements**: AUD-01, CAP-01
**Success Criteria** (what must be TRUE):
  1. Guest accesses through the canonical bus observe the declared masks, readback, trigger and status effects, and reset and master-enable behavior for the registers supported in this phase.
  2. The profile declares the pre-PWM tap, cycle-512 first-sample epoch, capture byte/duration bounds, explicit SOUNDBIAS access policy, and same-cycle ordering for writes, timers, DMA, and samples; it labels PWM and analog behavior unmodeled.
  3. A PCM serialization vector with distinct left/right samples verifies channel order and S16LE bytes. Muted capture is signed zero, and interval edges yield exact counts and bytes from the declared epoch.
  4. Bounded capture refusal preserves machine state and caller buffers. Sample and oscillator progression do not add an event per output frame or waveform edge, so a guest second does not exhaust the 4,096-event dispatch allowance.

**Plans**: 3/3 plans complete

- [x] 16-01-PLAN.md — Wire timed SOUNDBIAS and the first sample through the canonical core
- [x] 16-02-PLAN.md — Complete audio registers, capture bounds, and semantic vectors
- [x] 16-03-PLAN.md — Bind the audio profile contract and run canonical verification

### Phase 17: PSG Channels and Sequencer

**Goal**: Guests can configure and trigger all four PSG channels, with independently verified waveform and sequencer behavior on the existing machine clock.
**Depends on**: Phase 16
**Requirements**: PSG-01, PSG-02, PSG-03, PSG-04
**Success Criteria** (what must be TRUE):
  1. Independent cycle-boundary vectors verify channel 1's square period/duty, sweep, envelope, length, and status behavior.
  2. Independent cycle-boundary vectors verify channel 2's square period/duty, envelope, length, and status behavior.
  3. Wave RAM writes obey declared bank/access bounds, and channel 3's playback phase, waveform, volume, length, and status match source-level vectors.
  4. Independent cycle-boundary vectors verify channel 4's noise sequence in both width modes and its frequency, envelope, length, and status behavior. Fixed-rate PCM is not the sole oracle for oscillator edges shorter than one capture interval.

**Plans**: 4/4 plans complete

- [x] 17-01-PLAN.md — Run channel 2 and the shared PSG sequencer on canonical machine time
- [x] 17-02-PLAN.md — Implement and verify channel 1 sweep and both channel 4 noise widths
- [x] 17-03-PLAN.md — Implement bounded Wave RAM access and channel 3 playback
- [x] 17-04-PLAN.md — Synchronize the declared PSG policy and validate the integrated phase

### Phase 18: Direct Sound and FIFO DMA

**Goal**: Guests can feed independent Direct Sound FIFOs and observe their timer-driven playback and DMA refill through the canonical machine.
**Depends on**: Phase 16 (shared register and timing contract)
**Requirements**: DS-01, DS-02
**Success Criteria** (what must be TRUE):
  1. FIFOs A and B accept declared guest writes and resets, hold at most eight 32-bit words (32 signed sample bytes), and consume one sample in declared byte order per selected timer overflow, including when both use the same timer.
  2. When a pop leaves four words (16 bytes), FIFO A requests DMA1 and FIFO B requests DMA2; each sound-DMA request transfers four 32-bit words to its fixed FIFO destination through shared arbitration, regardless of programmed count/width.
  3. Empty and full FIFOs, unsupported DMA setups, and coincident consumption and refill follow the declared model policy. A refusal caused by bounded work leaves machine state unchanged.

**Plans**: 4/4 plans executed

- [x] 18-01-PLAN.md — Clock bounded FIFO A/B source samples through the canonical guest bus and timers
- [x] 18-02-PLAN.md — Refill through per-word shared DMA arbitration with whole-candidate refusal
- [x] 18-03-PLAN.md — Declare FIFO and sound DMA behavior in the profile and reject policy drift
- [x] 18-04-PLAN.md — Register exact Direct Sound checks and obtain measured local CI evidence

### Phase 19: Stereo Mixing and Runner Evidence

**Goal**: Callers can obtain a reproducible six-source stereo capture from the production runner and interpret its guest results and evidence limits.
**Depends on**: Phases 17 and 18
**Requirements**: MIX-01, CAP-02, CAP-03, EVID-01, CLAIM-01
**Success Criteria** (what must be TRUE):
  1. Guest routing, gain, ratios, master enable, and the declared signed-mix arithmetic produce expected left/right pre-PWM samples; SOUNDBIAS access follows profile policy without changing the fixed grid or PCM quantization.
  2. The runner returns bounded raw audio with profile/policy identity, actual cycle interval, format/cadence, frame/byte counts, digest, and an audio capture state separate from guest and runner status. Incomplete or failed capture never claims a valid complete artifact.
  3. Independent expected-sample vectors validate content and byte order; sample digests establish identity/replay equality only. Fresh runs advanced as one interval or irregular subdivisions produce identical machine observations and concatenated raw audio across register, timer, DMA, and sample boundaries.
  4. Canonical verification runs an original, rights-cleared BIOS-free guest that asserts guest progress and sample semantics while exercising PSG and Direct Sound/FIFO-DMA integration. Fixture source, revision, build recipe, digest, license, notices, BIOS needs, and permitted use are bound to the exact run.
  5. Profile and result metadata agree on the implemented pre-PWM capabilities, fixed capture policy, unsupported edges, and evidence limits. Hardware qualification remains UNKNOWN.

**Plans**: 5/6 plans executed
**Wave 1**
- [x] 19-01-PLAN.md — Implement and verify stereo mixing on the fixed pre-PWM sample grid (Wave 1)

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 19-02-PLAN.md — Capture committed CPU, DMA, and wait samples and prove irregular replay (Wave 2)

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 19-03-PLAN.md — Add bounded runner audio output and independent result states (Wave 3)

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 19-04-PLAN.md — Synchronize audio profile, result validation, and documentation (Wave 4)

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 19-05-PLAN.md — Verify a source-bound BIOS-free diagnostic in a local-only lane (Wave 5)

**Wave 6** *(blocked on Wave 5 completion)*
- [ ] 19-06-PLAN.md — Reconcile exact selection and measure canonical local verification (Wave 6)

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 16. Audio Registers and Capture Contract | 3/3 | Complete    | 2026-11-22 |
| 17. PSG Channels and Sequencer | 4/4 | Complete    | 2026-11-22 |
| 18. Direct Sound and FIFO DMA | 4/4 | In Progress | — |
| 19. Stereo Mixing and Runner Evidence | 5/6 | In Progress | — |

## Open Carryovers and Evidence Limits

- v1.0 `DIAG-04` and `DET-02` remain open. Local results do not substitute for the missing source-bound diagnostic or hosted Linux x64 evidence.
- The selected 1006-cycle HBlank policy and HBlank-edge, DMA-latch, and within-line visibility disputes remain unresolved; hardware qualification is UNKNOWN.
- The generated diagnostic guest remains local-only with redistribution rights unknown. G4 still gates BIOS-stub authoring; G8 and per-fixture review gate the deferred mGBA deep lane.
- Physical hardware/controller qualification, publication, licensing, copyrighted content, and replacement of Playstead's managed mGBA route remain explicit owner gates.

See [PROJECT.md](PROJECT.md), [STATE.md](STATE.md#deferred-items), and the milestone archives for detailed decisions, evidence, and limits.
