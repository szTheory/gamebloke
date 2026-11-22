# Requirements: Gamebloke v4.0 Audio

**Defined:** 2026-11-22  
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior, and every result says exactly what it proves and what it does not.

This is the committed scope for GSD milestone v4.0; it is not a software release claim.

## v1 Requirements

### Audio Registers and PSG

- [ ] **AUD-01**: A guest can access the supported GBA sound registers through the canonical bus, with declared access masks, readback, trigger/status, reset, and master-enable effects.
- [ ] **PSG-01**: A guest can configure and trigger PSG channel 1's square-wave and sweep behavior, including its declared frequency, duty, length, envelope, and status behavior on guest time.
- [ ] **PSG-02**: A guest can configure and trigger PSG channel 2's square-wave behavior, including its declared frequency, duty, length, envelope, and status behavior on guest time.
- [ ] **PSG-03**: A guest can configure and trigger PSG channel 3's wave playback, including bounded Wave RAM access and its declared bank, frequency, volume, length, and status behavior on guest time.
- [ ] **PSG-04**: A guest can configure and trigger PSG channel 4's noise behavior, including its declared frequency, width, length, envelope, and status behavior on guest time.

### Direct Sound and Mixing

- [ ] **DS-01**: Direct Sound FIFO A and B maintain independent bounded sample queues and consume one signed 8-bit sample on each selected Timer 0 or Timer 1 overflow, with empty-FIFO behavior named as model policy.
- [ ] **DS-02**: FIFO refill requests use the sound-specific DMA 1/2 transfer rules and shared machine arbitration; refused bounded work leaves machine state unchanged.
- [ ] **MIX-01**: Guest routing, gain, ratio, and SOUNDBIAS controls produce deterministic stereo samples using declared integer arithmetic, rounding, and clipping behavior.

### Raw Capture and Evidence

- [ ] **CAP-01**: Raw capture uses interleaved stereo signed 16-bit little-endian PCM at a fixed 32,768 frames per second, one frame every 512 `agb` master cycles, with a declared first-sample epoch and capture bounds.
- [ ] **CAP-02**: The production headless runner can capture bounded raw samples and report the profile and audio-policy identity, cycle interval, cadence and encoding, sample and byte counts, validity, guest result, and digest; missing, skipped, harness-failed, timed-out, cancelled, and guest-failed results remain distinguishable.
- [ ] **CAP-03**: Fresh runs advanced as one interval or irregular sub-intervals produce identical concatenated audio and machine observations for the same timed guest input.
- [ ] **EVID-01**: Canonical verification runs an original, rights-cleared, BIOS-free audio diagnostic with independent semantic expectations for guest progress and audio behavior, and records its source, revision, build recipe, digest, notices, BIOS needs, and permitted use.
- [ ] **CLAIM-01**: Profile and result metadata name implemented audio capabilities, fixed capture policy, selected model policies, unsupported edges, and evidence limits; hardware qualification remains UNKNOWN.

## v2 Requirements

Deferred audio capabilities that are not part of v4.0:

- **CAP-04**: Raw capture can represent guest programs that change SOUNDBIAS cadence through a segmented or otherwise explicitly variable-rate stream.
- **HOST-01**: A separate host adapter can play captured or streamed samples through selected devices with host-clock buffering and resampling.
- **QUAL-01**: Gamebloke can compare a named model/profile against an independently captured physical GBA signal using authorized hardware and rights-cleared diagnostics.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Host playback, device selection, and host resampling | Host timing belongs behind a future adapter; v4.0 ends at raw machine samples. |
| Physical hardware qualification | Requires separately authorized hardware, capture equipment, and a reviewed diagnostic; software replay evidence does not establish it. |
| Universal or broad retail-game compatibility claims | Every claim must name the exact profile, BIOS mode, cartridge and save device, and evidence. |
| Commercial ROMs, Nintendo BIOS bytes, private saves, or unlicensed fixtures | Rights policy; none may enter source control or public CI. |
| Replacing Playstead's managed mGBA route | This milestone adds a reproducible headless audio-evidence contract; it does not establish a fidelity advantage or authorize integration changes. |

## Traceability

The roadmapper will assign each requirement to exactly one phase.

| Requirement | Phase | Status |
|-------------|-------|--------|

**Coverage:**
- v1 requirements: 13 total
- Mapped to phases: 0
- Unmapped: 13 (roadmap pending)

---
*Requirements defined: 2026-11-22*  
*Last updated: 2026-11-22 after owner approval of v4.0 scope and capture policy*
