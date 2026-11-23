# Requirements: Gamebloke v4.0 Audio

**Defined:** 2026-11-22  
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior, and every result says exactly what it proves and what it does not.

This is the committed scope for GSD milestone v4.0; it is not a software release claim.

## v1 Requirements

### Audio Registers and PSG

- [x] **AUD-01**: A guest can access the supported GBA sound registers through the canonical bus, with declared access masks, readback, trigger/status, reset, and master-enable effects.
- [x] **PSG-01**: A guest can configure and trigger PSG channel 1's square-wave and sweep behavior, including its declared frequency, duty, length, envelope, and status behavior on guest time.
- [x] **PSG-02**: A guest can configure and trigger PSG channel 2's square-wave behavior, including its declared frequency, duty, length, envelope, and status behavior on guest time.
- [x] **PSG-03**: A guest can configure and trigger PSG channel 3's wave playback, including bounded Wave RAM access and its declared bank, frequency, volume, length, and status behavior on guest time.
- [x] **PSG-04**: A guest can configure and trigger PSG channel 4's noise behavior, including its declared frequency, width, length, envelope, and status behavior on guest time.

### Direct Sound and Mixing

- [x] **DS-01**: Direct Sound FIFO A and B each maintain a bounded 32-byte queue and consume one signed 8-bit sample on each selected Timer 0 or Timer 1 overflow, with empty-FIFO behavior named as model policy.
- [x] **DS-02**: FIFO A requests DMA1 and FIFO B requests DMA2 after a timer pop leaves four 32-bit words (16 bytes); each request transfers four 32-bit words to its fixed FIFO destination through shared machine arbitration, regardless of programmed DMA word count; refused bounded work leaves machine state unchanged.
- [x] **MIX-01**: Guest routing, gain, ratio, and master-enable controls produce deterministic pre-PWM stereo PCM using declared integer arithmetic, rounding, and clipping. SOUNDBIAS access/effects follow an explicit profile policy and cannot retime or quantize the fixed PCM stream.

### Raw Capture and Evidence

- [x] **CAP-01**: Raw capture is an interleaved stereo signed 16-bit little-endian tap before PWM conversion, at a fixed 32,768 frames per second aligned to reset cycle 0; its first frame is at cycle 512, subsequent frames are every 512 `agb` master cycles, and capture has explicit bounds.
- [x] **CAP-02**: The production headless runner captures bounded raw samples and reports profile/policy identity, cycle interval, cadence/encoding, sample/byte counts, and digest. Audio capture validity is separate from overall runner status and guest result; missing, skipped, harness-failed, timed-out, cancelled, and guest-failed results remain distinguishable.
- [x] **CAP-03**: Fresh runs advanced as one interval or irregular sub-intervals produce identical concatenated audio and machine observations for the same timed guest input.
- [x] **EVID-01**: Canonical verification runs an original, project-authored, BIOS-free audio diagnostic authorized for local verification, with independent guest-progress and expected-sample assertions bound to its exact build and run; a digest is used only for identity and replay equality. Fixture metadata records source, revision, build recipe, digest, license, notices, BIOS needs, and permitted use. Redistribution rights remain unknown.
- [x] **CLAIM-01**: Profile and result metadata name implemented audio capabilities, the fixed pre-PWM capture policy, selected model policies, unsupported edges, and evidence limits; hardware qualification remains UNKNOWN.

## v2 Requirements

Deferred audio capabilities that are not part of v4.0:

- **CAP-04**: Raw capture can model SOUNDBIAS-dependent PWM timebases/resolution or represent them in an explicitly variable-rate result when a concrete consumer requires it.
- **HOST-01**: A separate host adapter can play captured or streamed samples through selected devices with host-clock buffering and resampling.
- **QUAL-01**: Gamebloke can compare a named model/profile against an independently captured physical GBA signal using authorized hardware and rights-cleared diagnostics.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Host playback, device selection, and host resampling | Host timing belongs behind a future adapter; v4.0 ends at raw machine samples. |
| PWM carrier, analog filtering, and physical audio output | The v4.0 stream is a deterministic digital mixer tap before PWM conversion, not a hardware capture. |
| Physical hardware qualification | Requires separately authorized hardware, capture equipment, and a reviewed diagnostic; software replay evidence does not establish it. |
| Universal or broad retail-game compatibility claims | Every claim must name the exact profile, BIOS mode, cartridge and save device, and evidence. |
| Commercial ROMs, Nintendo BIOS bytes, private saves, or unlicensed fixtures | Rights policy; none may enter source control or public CI. |
| Replacing Playstead's managed mGBA route | This milestone adds a reproducible headless audio-evidence contract; it does not establish a fidelity advantage or authorize integration changes. |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| AUD-01 | Phase 16 | Complete |
| PSG-01 | Phase 17 | Complete |
| PSG-02 | Phase 17 | Complete |
| PSG-03 | Phase 17 | Complete |
| PSG-04 | Phase 17 | Complete |
| DS-01 | Phase 18 | Complete |
| DS-02 | Phase 18 | Complete |
| MIX-01 | Phase 19 | Complete |
| CAP-01 | Phase 16 | Complete |
| CAP-02 | Phase 19 | Complete |
| CAP-03 | Phase 19 | Complete |
| EVID-01 | Phase 19 | Complete |
| CLAIM-01 | Phase 19 | Complete |

**Coverage:**
- v1 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-11-22*  
*Last updated: 2026-11-22 after mapping the approved requirements to roadmap phases*
