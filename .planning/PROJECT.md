# Gamebloke

## What This Is

Gamebloke is a new, owner-controlled Game Boy Advance emulator. It aims to be
high-quality, accurate, deterministic, and observable. Its canonical machine
core is portable and has no host dependencies. A deterministic headless runner
and small host adapters sit around it. It is for the owner's emulator portfolio
and for developers and integrators who need a GBA machine they can test,
trace, and embed. Every compatibility claim names an exact machine profile,
BIOS mode, cartridge and save device, and the evidence behind it.

## Core Value

A fresh Gamebloke instance, given a named GBA profile, content, and timed
logical input, produces deterministic, observable, evidence-backed machine
behavior. That behavior covers guest progress, machine time, CPU/device
effects, raw video, and raw audio, all observed separately, and every result
says exactly what it proves and what it does not.

## Most Recent Completed Milestone: v2.0 Time, Interrupts, DMA, First Pixels

v2.0 completed a shared deterministic event path for timers, IRQ/HALT/input,
WAITCNT-aware bus access, DMA arbitration, PPU boundaries, and raw mode-3 video.
An original BIOS-free diagnostic guest runs through the headless runner and a
local Libretro terminal host. The owner accepted the visible demo for the
scoped Playable 0 judgment (G6) after running it locally.

This is a GSD milestone label, not a Gamebloke software release. Selected
timing policies, prefetch, the DMA latch dispute, and the HBlank edge remain
unqualified or unresolved; the terminal preview is downsampled and below real
time. The generated guest remains local-only with redistribution rights
unknown. BIOS-stub authoring remains behind G4, the mGBA deep lane remains
behind G8 and fixture review, and v1.0 `DIAG-04`/`DET-02` remain open
carryovers. None of these limits turns the scoped v2.0 result into a hardware,
retail-compatibility, or publication claim.

## Current Milestone: v3.0 PPU Modes and Composition

**Goal:** Extend Gamebloke's deterministic mode-3 path into functional display
modes 0–5 and common layer/composition behavior through the canonical raw-video
path for the `agb` profile. This is a deterministic software model, not a claim
of cycle-perfect PPU timing, hardware qualification, or broad retail
compatibility.

**Target features:**
- Text, affine, and bitmap backgrounds with mode-appropriate addressing,
  palettes, scrolling/reference parameters, and defined bounds behavior.
- Regular and affine OBJ rendering, shared layer priorities, transparency,
  windows, color effects, mosaic, and forced blank.
- Explicit line/blank access policies for canonical VRAM, palette RAM, and OAM,
  integrated with the shared scheduler and DMA arbiter.
- Semantic pixel and machine-boundary evidence through the existing raw-video
  and runner paths, with fixture rights reviewed individually.

Keep the selected 1006-cycle HBlank policy and the HBlank-edge, DMA-latch, and
pixel-visibility disputes open unless new independent evidence resolves them.
See the [scope recommendation](research/v3-ppu-milestone-recommendation.md) for
provenance and detailed boundaries; formal requirements and phases are defined
in REQUIREMENTS.md and ROADMAP.md.

## Requirements

### Validated

- ✓ Named `agb` machine profile with direct-start/no-BIOS boot policy, ROM-only cartridge scope, and explicit evidence labels — Phase 3, extended through Phase 10. The profile does not claim backup devices or hardware-qualified timing.
- ✓ Portable canonical machine core with caller-owned deterministic state and host services kept behind adapters — Phase 3 and Phases 7–10.
- ✓ A deterministic headless runner reports guest semantics, machine time, device/CPU observations, and raw supported mode-3 video separately — Phases 4, 7, and 10. Raw audio is not implemented.
- ✓ One canonical local verification command (`cmake --workflow --preset ci`), identical to the required `CI required` check, with a negative control for every gate — Phase 1 (verified 2026-11-10: 120 tests ran/2 visible skips locally and on Linux GCC + macOS arm64; ASan/UBSan lane; hygiene and rights-signature scans; ruleset requires the check). The manifest-lint clause joins in Phase 2.

- ✓ Caller-owned deterministic machine state, timing-bearing checked bus, declared GBA map/mirror/width policies, and transactional copied-ROM loading with bounded advisory diagnostics — Phase 3. Timing/fill/latch policies are not hardware qualification.
- ✓ Owner-authorized logo-window authority and normal/sanitized production-header comparison witnesses — Phase 3. Only the digest is published; no provenance, redistribution or game-compatibility claim follows.

- ✓ Bus-backed bounded ARM/Thumb subset, explicit pipeline/modes/exceptions, neutral mailbox inspection, category traces, exact machine-time JSON results and installed CLI relocation — Phase 4. Full ISA diagnostics remain Phase 5; timing/latch policies are unverified and trace writes currently retain the bus result zero.
- ✓ Profile version 4 names the direct-start/no-BIOS CPU policy and implemented/excluded operand scope; actual producer results preserve dispute disclosure — Phase 4.
- ✓ One shared machine-time path, four timers, explicit bus/internal cycle accounting, and an original BIOS-free Timer 0 polling guest — Phase 7. Selected timing costs remain project-model evidence, not hardware qualification.
- ✓ Device IRQ requests, CPU eligibility/acceptance/entry, HALT wake, and timestamped logical keypad input remain separately observable — Phase 8. These deterministic policies do not establish GBA IRQ latency or physical-controller behavior.
- ✓ WAITCNT-aware bus costs, four-channel DMA arbitration, CPU pause/resume, and device events on the shared clock — Phase 9. Startup and latch policies remain project-selected or disputed, not hardware qualification.
- ✓ Deterministic PPU line events and raw mode-3 BGR555 frames with semantic pixel checks, complete-frame validity, and explicit latch/dispute evidence — Phase 10. Other display modes and effects remain out of scope.
- ✓ Original BIOS-free Playable 0 diagnostic guest runs through the Gamebloke Libretro module and a dependency-free terminal host; the owner accepted the visible local demo — Phase 10. This is not retail compatibility, hardware qualification, or redistribution approval.
- ✓ The Libretro adapter uses the official ABI header and canonical machine implementation; the terminal fallback adds no runtime dependency or second emulator core — Phase 10.
- ✓ Cheap event-boundary and exact test-selection checks run before dependent longer suites, with negative controls for missing or empty selection — Phase 7 and the current Phase 9/10 fast paths.

### Active

These are hypotheses until shipped and verified. Formal REQ-IDs live in
REQUIREMENTS.md.

- [ ] Expand ARM7TDMI ARM/Thumb coverage and source-qualified timing beyond the implemented subset; prefetch and incomplete hardware cost mappings remain explicit gaps
- [ ] Audio event scheduling and PSG/Direct Sound FIFO behavior in guest-time units
- [ ] v3.0: Deterministic software-model raw output for display modes 0–5, common BG/OBJ layers, composition, windows, effects, mosaic, forced blank, and declared video-memory access policies for `agb`; no cycle-accuracy or broad compatibility claim
- [ ] Audio (PSG + Direct Sound FIFO) produced as raw machine samples, kept separate from host resampling and playback
- [ ] Explicitly modeled cartridge backup devices. Save type is never inferred from a filename or title.
- [ ] Raw audio capture in the deterministic runner; missing, skipped, harness-failed, timed-out, cancelled, and guest-failed results stay distinct.
- [ ] A fixture manifest for every test ROM (source, revision, build recipe, digest, license, permitted use, BIOS mode, expected result)
- [ ] Complete per-fixture rights review for any scenario intended for redistribution; the current generated guest remains local-only
- [ ] A defined public API boundary (lifecycle, load/unload, input timing, bounded advancement, buffer ownership, reset, errors, refusal postconditions) before it is exposed
- [ ] An installed, relocatable external consumer once packaging exists
- [ ] Named benchmark workloads measured only after a correctness baseline exists

### Out of Scope

These are deferred, not abandoned. Each records what evidence would promote it.

- Bundling Nintendo BIOS bytes, commercial ROMs, private saves, or unlicensed fixtures. Rights boundary; never in source control or public CI.
- GB/CGB backward-compatibility mode. It is a distinct machine/CPU mode, and GabbaBoy owns the separate GB/CGB core. Promote only with an explicit owner decision and a profile.
- Link cable multiplayer, Wireless Adapter, e-Reader, and other accessories. Deferred profiles. Promote when a concrete user outcome and test evidence exist.
- Specialty cartridge devices (RTC, solar, tilt/gyro, rumble). Deferred until a cartridge profile requires them and fixtures exist.
- Universal "all retail games" compatibility claims. Compatibility is stated per profile, per evidence.
- Physical hardware qualification and physical controller qualification. Human gate; needs owner hardware and authority.
- Public binary releases and announcements. Repo visibility is settled (public), but publishing release artifacts stays an owner gate.
- Automatic replacement of Playstead's managed mGBA route. Not assumed; any Playstead integration needs its own decision.
- Choosing the project license. Owner gate (see Key Decisions).

## Context

- **Portfolio:** The owner keeps several emulator projects, all C/CMake with MIT licenses where present (GabbaBoy GB/CGB, Nesturbator, Cogmaster, Muhtari, Glueyneo, Superfriendo, DuoSpool, Lynxplosion, iamverysmartvision, Schwaystation). These are engineering lessons only. Their hardware, APIs, gates, and stops do not transfer.
- **Playstead:** It currently uses a managed mGBA process adapter for GB/GBC/GBA (pinned macOS 26/arm64). Its persistent-save qualification is GB/GBC-focused, and its GBA regression is limited to restore/Continue-menu. Gamebloke must find its own value beside this route.
- **LibFuturo** (`~/projects/libfuturo`): an experimental synchronous C API (frame-oriented advance, logical buttons, bounded work, caller-owned software A/V buffers, profile-defined media/state). It has no GBA profile. It is a candidate integration, not an accepted dependency.
- **Historical reach:** Nintendo reports 81.51M GBA units and 377.42M software units. This is distribution history, not current demand.
- **Hardware references:** AGB Programming Manual (mirror), ARM7TDMI TRM, GBATEK, Tonc/gbadoc. Candidate test suites: mGBA suite, jsmolka gba-tests, NanoBoyAdvance hw-test. Each needs a per-fixture rights/BIOS/output review.
- **Bootstrap pack:** `.planning/bootstrap/` (dated 2026-11-09) is research input, not specification.
- **Repository state at init:** No source code. Public GitHub repo `szTheory/gamebloke` was created on 2026-11-09 at the owner's direction so CI can run early.
- **Current implementation:** C17/CMake core, deterministic headless runner, Libretro module, and dependency-free terminal frontend through Phase 10. One machine-owned time path drives timers, CPU IRQ/HALT sampling, timestamped logical input, WAITCNT-aware CPU/DMA bus costs, DMA arbitration, PPU boundaries, and raw mode-3 output. The owner accepted the local diagnostic demo for G6 only. The terminal preview is downsampled and below real time. Game Pak prefetch remains unmodeled; DMA latch and selected HBlank behavior remain disputed, with hardware qualification UNKNOWN. The generated guest is local-only and redistribution rights are unknown. Phase 6 `DIAG-04` and `DET-02` remain open; no hosted rerun or publication is authorized by this milestone.

## Constraints

- **Rights:** No Nintendo BIOS, commercial ROMs, private saves, or unlicensed fixtures in source control or public CI. A matching hash proves identity, not redistribution rights.
- **Portability:** The canonical core is independent of host files, wall-clock scheduling, device discovery, UI, A/V devices, and durable save policy.
- **Determinism:** Fresh isolated state plus explicit timed input must produce repeatable results. Hashes alone prove repeatability, not accuracy.
- **Dependencies:** Keep the tree small and flat. Each dependency needs a documented reason (lifetime, platform, license, supply chain).
- **Security:** ROMs, BIOS, saves, and metadata are untrusted input. Validate lengths and extents before allocation or mutation. Refused operations have documented postconditions.
- **Evidence:** Owner decisions, source facts, observations, measurements, inferences, recommendations, and unknowns are labeled distinctly.
- **Human gates:** Publication, copyrighted content, physical qualification, licensing, and subjective product review stay explicit owner gates.
- **Performance:** Establish correctness and named workloads first. Never trade correct output for speed.

## Verification and Iteration Policy

- Shift verification left. For each requirement, automate observable behavior at the smallest useful seam, then add integration, end-to-end, or smoke coverage only when it proves a distinct cross-component risk. Use deterministic inputs, semantic assertions, and meaningful negative controls for important gates.
- Prefer useful automated evidence over manual UAT for repeatable behavior. Before handing work to the owner, inspect the actual local result and its limits. Convert repeatable manual checks into automated checks when they provide trustworthy feedback. Keep owner-only decisions—rights, physical hardware, subjective playability, product preference, and publication—explicit; automation must not impersonate owner acceptance.
- Do not put every check in CI. Add or retain CI work when it catches a distinct recurring failure class at worthwhile runtime and maintenance cost. Keep one-off or exploratory checks local when they provide useful feedback without recurring CI value.
- Recreate GitHub Actions locally with `act` when a workflow-specific behavior needs checking and the local run meaningfully shortens feedback. Prefer the existing canonical local verification for native checks; do not add an `act` dependency or maintain a duplicate lane without measurable value.
- Put cheap, deterministic consistency and selection checks before long suites when they can reject known failures early. After each CI run, inspect measured critical-path time for duplicate setup/builds and other low-risk savings. Optimize measured bottlenecks without weakening evidence; keep emulator throughput separate from CI wait/build/test time.
- Reuse valid results when their source, configuration, fixture, and toolchain inputs are unchanged. Refresh only affected evidence; do not rerun a whole matrix by habit. Record what each result proves, skips, and leaves unknown.
- At verification checkpoints, first determine whether a human-needed item has a useful local, deterministic, or contract-level check. Run and inspect that check when authorized, persist the evidence, and ask the owner only for a remaining material judgment or authority gate. Do not reopen a resolved judgment without new evidence.
- Apply the same cost and evidence discipline to release checks: run cheap integrity, rights, and package-consistency checks before expensive workflows; measure release/CI critical paths; keep publication and release authority with the owner.

## Milestone and Phase Scoping Policy

- At each new-milestone boundary, start from the current rolling roadmap and a concrete recommended next scope with alternatives. Do not restart with an open-ended “what next?” question when the roadmap and current evidence already support a recommendation. Surface a material choice only when it changes product direction, rights, authority, risk, cost, or an irreversible action.
- Apply the same evidence-first discovery to phase discussions. Identify the actual decision, then use only the relevant technical, product, QA, security, performance, design, and operations lenses for it. Fan out read-only research with subagents when independent work saves time or context; include an adversarial review and stop when another pass is unlikely to change the recommendation, material risk, or evidence plan.
- Prefer primary and authoritative sources, and inspect current repository evidence. Keep owner decisions, source facts, project observations, measured results, inferences, recommendations, and unknowns distinct. Capture provenance and the practical limit of each source; agreement with another emulator or repeatable output alone is not hardware truth.
- Return one recommended path, the scope and exclusions, main tradeoffs, a verification direction, and a small set of roadmap alternatives. Update the rolling roadmap and retain high-signal lessons in `.planning/` so future discussions can start from them without repeating settled discovery.
- This upstream scoping supports OpenGSD; it does not replace its formal requirements, roadmap validation, phase planning, execution, or verification workflows.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Project name "Gamebloke" (owner) | Owner reviewed the old Apple IIe name collision and accepted reuse. Not trademark clearance. | ✓ Settled |
| Build a new GBA emulator (owner) | Broad console coverage with an owner-controlled core | ✓ Settled |
| Autonomous OpenGSD with a rolling roadmap (owner) | Routine reversible work proceeds; material choices are surfaced | ✓ Settled |
| Gamebloke does not assume it replaces Playstead's mGBA route (owner) | Value beside mGBA must be identified and measured | ✓ Settled |
| Differentiator beside managed mGBA | Research → requirements | — Pending (research) |
| First machine profile (model/region/BIOS mode) | Phase 3 established `agb`, direct-start/no-BIOS, and ROM-only scope; later profiles require separate evidence | ✓ Initial profile established; no broad compatibility claim |
| BIOS and boot policy | Phase 3/4 selected direct-start with no bundled BIOS; G4 still gates any authored stub | ✓ No-BIOS policy is explicit; other boot modes remain future scope |
| CPU/system timing architecture | Phases 7–10 use one machine-owned event/time path and shared bus accounting | ✓ Architecture established; exact hardware timing and prefetch remain incomplete or unqualified |
| Cartridge format and backup-media scope | Current profile accepts ROM-only content; backup devices remain deferred | ✓ Current scope documented; CART-01 and other save media remain future work |
| First diagnostic and playable fixture | Phase 10 original BIOS-free demo ran locally and was accepted under G6; manifest keeps it local-only | ✓ Scoped demo accepted; redistribution rights remain unknown |
| Implementation language, build, API/ABI | C17/CMake core; official Libretro ABI adapter added; public Gamebloke API remains later scope | ✓ Implementation/build and Libretro boundary established; stable public API remains pending |
| Host platform and installed consumer | Phase 10 local terminal host runs the Libretro module; installed external consumer remains future scope | ✓ Local host established; relocatable consumer/package remains pending |
| LibFuturo fit | Prove with a real core/consumer before adoption | — Pending (later phase) |
| Public GitHub repo `szTheory/gamebloke` (owner, 2026-11-09) | Owner wants CI running early. No PII or credentials committed; noreply git identity. | ✓ Settled |
| Required check `CI required` on `ubuntu-26.04` + `macos-26` (arm64), actions SHA-pinned, `main` protected by a PR-only ruleset with no bypass actors (owner-authorized, 2026-11-10) | One command, same verdict locally and in CI; nothing reaches main except through a green PR | ✓ Good (Phase 1) |
| Standing PR policy for Phases 2-6: one PR per phase, squash-merged through the ruleset when CI required is green; stop on red; no bypass or settings changes (owner, 2026-11-10) | Ruleset only accepts PRs; keeps CI proof per phase | Remains the policy; Phase 6 is local and incomplete, with hosted evidence still open |
| Owner-authorized logo authority (2026-11-10) | Owner supplied two existing local files and delegated ignored copies, hashing and checks; only the shared digest is published | Verified, owner-accepted and activated on main through PR #6. No rights/provenance inference. |
| Canonical CPU/runner evidence boundary (Phase 4) | One bus-backed interpreter, whole-unit budget/trace preflight, distinct status/semantic/time/claim/limits, build-time fixture authority | Verified in local/hosted checks and independent reviews; later diagnostic evidence remains scoped, with DIAG-04/DET-02 open as recorded in Phase 6 |
| IRQ, HALT, and timestamped input boundary (Phase 8) | Keep device requests separate from CPU sampling/entry and HALT wake; keep input deterministic and model-scoped | Verified locally; no BIOS handler, hardware latency, physical-controller qualification, or carryover closure is claimed |
| WAITCNT and DMA evidence boundary (Phase 9) | Route bus costs and DMA through shared machine time; label selected startup/latch behavior and preserve disputes | Verified locally; no hardware timing, latch qualification, hosted evidence, or fixture-rights conclusion is claimed |
| Time and timer evidence boundary (Phase 7) | Keep one-cycle non-ROM costs and timer edge choices project-selected; separate semantic guest completion, IF request, and model cycles | Verified locally as model evidence; no GBA timing qualification is claimed |
| IRQ/HALT and logical input boundary (Phase 8) | Keep device IF, CPU eligibility/entry, HALT wake, and timestamped host-neutral input distinct | Verified locally; no BIOS handler, hardware IRQ latency, or physical-controller behavior is claimed |
| PPU and Playable 0 boundary (Phase 10) | Use explicit PPU events, complete supported mode-3 frame capture, semantic pixel checks, and a local host; preserve HBlank evidence and dispute labels | Verified locally; the owner accepted the original diagnostic demo only; no hardware, retail-game, audio/save, or redistribution claim is implied |
| Local interactive host boundary (Phase 10) | Reuse the canonical machine through the official Libretro ABI; use a dependency-free terminal fallback after managed RetroArch launch denial | Verified locally; no policy bypass, second core, or runtime dependency was added |
| v2.0 owner judgment (2026-11-19) | The owner ran the terminal demo and accepted the visible original BIOS-free diagnostic behavior | ✓ Accepted for scoped Playable 0 demo; broader compatibility, hardware, physical-controller, and rights decisions remain open |
| v3.0 PPU scope (owner-prepared recommendation; 2026-11-19) | Extend the mode-3 path to functional modes 0–5 and common composition through raw video, preserving explicit project-model and hardware-evidence limits | In progress; detailed requirements and phases are generated by OpenGSD |
| Evidence guide clarity (owner, 2026-11-11) | Owner explicitly accepted the explanation after software-evidence and rights/qualification limits were stated | ✓ Accepted; no rights grant or hardware/game claim |
| Automation-first verification and CI efficiency (owner, 2026-11-16) | Shift repeatable verification left; minimize UAT through useful automated evidence; use CI for recurring value; preflight long work and inspect measured critical paths without weakening checks | ✓ Adopted as the default in Verification and Iteration Policy above |
| Evidence-first milestone and phase discussions (owner, 2026-11-19) | Start from roadmap recommendations and options; use relevant parallel research, primary sources, an adversarial pass, provenance, and a diminishing-returns stop; apply the same approach to milestone and phase scope | ✓ Adopted in Milestone and Phase Scoping Policy above |
| Carry v1.0 Phase 6 evidence gaps into v2.0 (owner, 2026-11-15) | Archive v1.0 while preserving ARM/Thumb source-bound evaluation (`DIAG-04`) and hosted Linux x64 GCC/Clang (`DET-02`) as open; do not relabel local results as passes for those claims | Recorded in `.planning/STATE.md` and the archived v1.0 audit; no hosted run or publication authorized |
| Project license | Owner authority. The repo is public with no LICENSE file yet, so default copyright applies until the owner chooses one. | — Pending (owner gate) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check: still the right priority?
3. Audit Out of Scope: reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-11-19 after v3.0 milestone start*
