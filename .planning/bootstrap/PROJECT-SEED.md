# Project idea: Gamebloke

## What to build

Build a high-quality, accurate, deterministic Game Boy Advance emulator under
the owner-selected working name **Gamebloke**. Keep the machine testable and
observable, make it suitable for useful host integrations, and carry forward
relevant lessons from earlier emulator work and LibFuturo.

This seed is kickoff input, not a final hardware specification, compatibility
claim, API/ABI, implementation language, license, release promise, or fixed
first milestone. Use GSD to create formal project context, evidence-based
requirements, and a rolling roadmap. Keep owner decisions, source facts,
implementation observations, measured results, recommendations, inferences,
and unknowns distinct.

## Why this project exists

The owner wants broad console coverage for systems people care about and
wants each new emulator to start with the strongest relevant engineering
lessons from prior projects. The GBA has substantial historical reach:
Nintendo's sales page reports 81.51 million systems and 377.42 million
software units. That is historical distribution evidence, not a measure of
current players or demand for a second emulator. [Nintendo lifetime sales](https://www.nintendo.co.jp/ir/en/finance/hard_soft/index.html?country=66)

Playstead already has a managed mGBA route for GBA. GabbaBoy is a separate
Game Boy/Game Boy Color core and does not establish GBA coverage. GSD should
identify Gamebloke's concrete value beside the current mGBA route—such as an
owner-controlled core, a distinct accuracy/verification goal, a useful
integration, or another measured gap—without reopening the owner's decision
to start this project or assuming that Gamebloke must replace Playstead's
current route.

## North Star

- Emulate clearly named Game Boy Advance machine and cartridge profiles
  faithfully, with support claims tied to specific evidence.
- Make machine execution deterministic and observable. Treat CPU/device
  behavior, machine time, logical input, guest progress, raw video, raw audio,
  and host presentation as connected but distinct observations.
- Aim for strong runtime performance without compromising correct behavior.
  Establish a correctness baseline and named workloads before optimizing.
- Keep the canonical machine implementation portable and independent of host
  files, wall-clock scheduling, device discovery, UI, audio/video devices, and
  durable save-file policy.
- Make building, debugging, testing, packaging, and integration fast and
  repeatable, with a small dependency surface where practical.
- Grow compatibility through verified slices while preserving the larger
  quality goal. A narrow first profile is delivery sequence, not a permanent
  limit on capability.

## Recommended discovery and first-slice direction

Evaluate an early, independently verifiable vertical path that combines a
named GBA profile, an original or rights-reviewed source-built diagnostic,
deterministic headless execution, machine-readable semantic results, raw
video/audio capture, and an independent installed consumer when packaging
exists. Keep a legally cleared playable homebrew scenario visible early so the
project does not become research-only.

A cycle-aware diagnostic can prove useful CPU, memory, interrupt, timer, DMA,
graphics, audio, and input behavior in stages. It does not replace a guest
playability goal or show broad compatibility. Before committing to the first
test path, verify the fixture's exact source, build recipe, license, permitted
use, output format, and whether it requires Nintendo BIOS data. Do not ship a
BIOS dump, commercial ROM, private save, or unlicensed test fixture.

This is sequencing advice for GSD to evaluate. It does not select a CPU timing
model, BIOS strategy, cartridge backup device, content format, test ROM, game,
host, or milestone scope.

## Product and hardware scope to resolve

Treat this as a Game Boy Advance project, not an already specified universal
Game Boy-family emulator. GSD should make an explicit baseline and roadmap
distinctions for:

- Standard GBA mode and any chosen model/region/profile details.
- GBA SP and Game Boy Micro physical form factors, power/display behavior, and
  which differences belong to the host rather than the machine core.
- Backward-compatible Game Boy/Game Boy Color mode, which uses a distinct
  machine CPU/mode and is not automatic GBA scope. Existing GabbaBoy owns a
  separate GB/CGB core.
- Cartridge ROM/container validation, supported sizes, header handling, and
  cartridge-specific hardware.
- Backup devices such as SRAM/FRAM, EEPROM, and flash variants. They have
  different protocols and state; do not assume every save is flat RAM or
  infer behavior only from a filename or title string.
- Cartridge GPIO and optional devices such as RTC, solar sensor, tilt/gyro,
  rumble, and other specialized cartridge behavior.
- Link-port serial modes, multiplayer/link cable, Wireless Adapter, e-Reader,
  and other accessories. These may be valuable later profiles, not silent
  baseline promises.
- BIOS/boot behavior, software BIOS replacements, and which diagnostic/game
  paths can run without an external BIOS.
- CPU, memory bus, prefetch/wait states, timers, interrupts, DMA, video
  scheduling, audio timing, and input sampling boundaries.
- Core language/build system, public API/ABI, error and ownership model,
  headless runner, debug/trace interfaces, save-state policy, installed host,
  supported platforms, dependency set, project license, visibility, and
  release artifacts.

Do not commit to all cartridge types, accessories, all retail games, every
host platform, link multiplayer, or a firmware policy merely because another
emulator implements them. State what is deferred and what evidence would
promote it. A first profile is not the ceiling for the project's ambition.

## Core, host, and integration

Keep one canonical GBA machine implementation. The core should own deterministic
CPU/device state and guest behavior. Host layers should own file picking,
wall-clock pacing, physical controls and mapping, windows, audio/video output,
and durable save paths. Define instance lifetime, load/unload, BIOS/content
inputs, input timing, bounded advancement, output buffer ownership, reset,
errors, and refusal postconditions before exposing a public API.

LibFuturo is an integration candidate, not an accepted dependency or qualified
GBA route. Its current contract is experimental and has no GBA profile. It
offers synchronous frame-oriented advancement, logical-button input, bounded
work, caller-owned software A/V buffers, and profile-defined media/state
operations. A real Gamebloke adapter and consumer must check whether frame
boundary input and advancement, video/audio formats, BIOS plus cartridge
content, and cartridge backup/state semantics fit the selected GBA profile.
Prove a concrete mismatch before proposing a shared contract change. An adapter
build alone proves neither hardware accuracy nor an installed product route.

The new project should ask which user or developer outcome Gamebloke owns beside
Playstead's existing managed mGBA route. It should not assume an automatic
migration, a new Playstead integration, or a permanent dual-core product.

## Development and verification intent

- Use original diagnostics or fixtures with explicit rights for the intended
  use. Preserve source/revision, build recipe, digest, license/notices,
  permitted use, BIOS mode, and expected result in a fixture manifest.
- Drive deterministic logical input at explicit machine-time boundaries from
  fresh isolated state. Verify guest semantics and machine progress as well
  as CPU/device effects, raw video, and raw audio.
- Keep missing, skipped, harness-failed, timed-out, cancelled, and
  guest-failed results distinct. Never treat a runner that did not execute a
  required test as a pass.
- Include malformed/truncated content, unsupported configuration, bounds,
  lifecycle, reset, refused-operation state preservation, missing/wrong input,
  and no-input negative controls where those surfaces exist.
- Treat hardware-test ROMs and emulator comparisons as evidence for the
  properties they exercise, not a blanket accuracy score. Where a result is
  important, prefer a documented hardware-derived expectation or an
  independent oracle.
- Verify source code, component seams, headless runs, an installed consumer,
  adapters, and physical behavior as separate layers. A simulated controller
  does not qualify a physical controller path.
- Keep a fast local/PR gate and deeper scheduled or release checks. Add CI
  only for distinct recurring failure classes at reasonable wait and
  maintenance cost. After every CI run, inspect measured critical-path
  inefficiencies without removing proof.
- Benchmark a named workload/build/profile and hold correct outputs constant.
  Separate emulator throughput and end-to-end latency/resource use from CI
  queue, setup, build, and test time.
- At milestone close, keep main green, triage open PRs, update roadmap and
  lessons, and target a release from the exact tested artifact when rights,
  package readiness, qualification, and authority permit it.

## Owner decisions and open choices

**Accepted owner direction:** project name Gamebloke; new Game Boy Advance
emulator; use relevant past-emulator and LibFuturo lessons; favor autonomous
OpenGSD and useful automated verification; fan out around material milestone
decisions and keep a rolling roadmap.

**Still open for GSD to research or keep explicit:** Gamebloke's differentiator
beside managed mGBA; first model/profile; BIOS and boot policy; CPU and system
timing architecture; cartridge format and backup-media policies; first
diagnostic and playable fixture; coverage of specialty cartridge devices and
link peripherals; host platform and installed consumer; API/ABI and language;
LibFuturo fit; security and dependency choices; state/persistence scope; project
license and repository visibility; performance targets; release artifacts and
versioning.

Prefer evidence-supported reversible defaults for routine implementation
choices. Ask the owner only when an answer changes material product direction,
rights, authority, risk, cost, or an irreversible action. Public publication,
unlicensed content, external qualification, physical hardware measurements,
and subjective product review remain explicit boundaries.
