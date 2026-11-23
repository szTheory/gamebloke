# Gamebloke project instructions

## Start here

This is a pre-GSD setup. Read .planning/bootstrap/START-HERE.md before changing
planning state. The bootstrap seed and research are kickoff input, not a frozen
specification. Let the installed GSD workflow create and validate
.planning/PROJECT.md, REQUIREMENTS.md, ROADMAP.md, and STATE.md.

The owner selected **Gamebloke** as the working name for a Game Boy Advance
emulator. A small old project uses the same name for a handheld Apple IIe
emulator; the owner has reviewed it and accepts reusing the name because it has
no meaningful traction. This is not trademark clearance. Do not reopen the name
question without materially new collision evidence.

## Product and engineering direction

- Aim for an accurate, useful, portable GBA emulator. Keep the long-term
  capability goal ambitious while making the first profile and delivery
  sequence explicit.
- Identify what Gamebloke should add beyond the existing managed mGBA GBA route
  in Playstead. The owner has chosen to start this project; that question should
  shape product requirements and integration priorities, not become a reason to
  undo the project start.
- Keep deterministic machine behavior separate from host clocks, files,
  physical controller discovery and mapping, windowing, audio/video presentation,
  persistent file writes, and multiplayer transport.
- Use one canonical machine implementation with small adapters. Evaluate
  libfuturo against a real Gamebloke core and consumer before adopting it,
  extending its contract, or describing the route as qualified.
- Prefer a small, flat dependency tree. Copy a small understandable helper only
  when its provenance and maintenance owner are clear. Add a dependency when
  its concrete value outweighs its lifetime, platform, license, and
  supply-chain cost.
- Do not put commercial game ROMs, Nintendo BIOS bytes, private saves,
  credentials, or unlicensed test content in source control or public CI.
  Record each fixture's source, revision, build recipe, digest, license,
  notices, BIOS requirements, and permitted use. A matching hash proves
  identity, not redistribution rights.
- Describe accuracy and compatibility by exact machine, BIOS mode, cartridge
  and save device, optional peripheral, and test evidence. A boot, title screen,
  test-suite score, matching hash, or agreement with another emulator alone
  does not establish broad compatibility or hardware accuracy.

## GSD discussion and project learning

Use Codex for OpenGSD until the owner explicitly switches runtimes. The owner
switches between Codex and Claude regularly; preserve both runtime model mappings
in `.planning/config.json`, which is the source of truth for model and effort
routing. Resolve each agent's model and effort together before dispatch.

At each new-milestone boundary, first refresh the rolling roadmap across near-,
mid-, and long-term horizons, with concrete next-scope alternatives and the
evidence that could promote or defer them. Read current project-owned state,
requirements, prior decisions, and lessons. Fan out around the actual decision
using the relevant stakeholder and technical lenses (for this project as
applicable: GBA hardware, CPU/bus, graphics, audio, cartridge, security/rights,
API/host, product, QA, performance, design, and DevOps). Use subagents for
independent questions when that saves time or context. Prefer primary and
authoritative sources, compare options and failure modes, include an adversarial
pass, then synthesize one recommended path. Stop when another pass is unlikely
to change the recommendation, material risk, or evidence plan.

After the owner selects or confirms a direction, carry the accepted scope into
formal OpenGSD requirements and roadmap work; do not repeat settled research or
take over downstream phase execution. Update the rolling roadmap and retain
only high-signal takeaways in `.planning/`, with provenance and clear labels
for owner decisions, source observations, measurements, inferences,
recommendations, and unknowns. Keep these durable instructions aligned with
the owner's corrections and standing preferences.

Keep accepted owner decisions, source observations, measured results,
historical reports, inferences, recommendations, and unknowns distinct. Refresh
only evidence that could change the current decision. Do not repeat generic
research or transfer another emulator's hardware assumptions, API layout,
benchmark, license, unresolved gate, or stop as if it applied to Gamebloke.

Proceed autonomously on routine reversible decisions. Ask the owner only when
the answer changes material product direction, rights, authority, risk, cost,
or an irreversible action. Preserve external publication, copyrighted content,
physical hardware qualification, and subjective judgment as explicit human
gates. Do not turn routine plan steps or useful local verification into
approval requests.

## Verification, CI, and releases

- When a required compiler, SDK, or verification tool is missing on the host,
  check whether a pinned Docker lane can provide it before installing host
  dependencies. Keep the container path easy to invoke and use it as a normal
  developer workflow where practical.
- Automate useful verification as early as it gives trustworthy feedback.
  Maintain one canonical local command aligned with the fast required CI path.
  Give important gates a meaningful negative control; missing and skipped
  checks must be visible.
- Before waiting on a long or costly hosted CI/CD run, run the canonical local
  workflow and cheap deterministic consistency/selection checks first. Use
  `act` as a best-effort local GitHub Actions replay when it supports the
  relevant workflow path and materially shortens feedback; record unsupported
  runner, event, service, and secret differences. Local emulation does not
  replace host-specific or hosted qualification.
- Put cheap, deterministic consistency and dependency checks before long suites
  when they can reject a known failure early. Check final-scale bottlenecks,
  shared policy fields, evidence bindings and actual changed inputs before
  repeating expensive work. Step back when progress stalls, choose the least
  disruptive measured remedy, and record the lesson. Preserve valid evidence
  when its inputs are unchanged; do not repeat a whole matrix by habit.
- Use deterministic, timed input and fresh machine state. Keep guest progress,
  emulated time, logical input, raw video, raw audio, and host presentation as
  separate observations. Assert semantic guest behavior alongside byte-level
  output checks. Hashes establish repeatability unless backed by an independent
  oracle.
- Test component, integration, headless guest, installed-consumer, and host
  boundaries when each exists. A virtual host or adapter check does not claim
  physical-controller behavior.
- Add CI work only for a distinct recurring failure class at useful
  runtime/maintenance cost. After every CI or release (CD) run, inspect
  measured critical-path time across setup, configure, build, test, packaging,
  and artifact steps. Look for duplicate work and low-risk savings without
  weakening evidence, and record useful lessons. Benchmark emulator throughput
  separately from CI/CD wait and processing time; hold correct output constant
  while measuring.
- For Docker- or `act`-based CI/CD reproduction or release checks, give
  task-owned images, containers, networks, and temporary volumes a clear
  `gamebloke-<task>-` name (and project/task labels where supported). Avoid
  generic names. Afterwards, inspect and remove only resources created for
  that run, including disposable images and volumes; never use broad prune
  commands that could delete shared caches or owner data. Record resources
  intentionally kept.
- At milestone close, keep main green, triage open pull requests, update the
  rolling roadmap and lessons, and target a release of the exact tested
  artifact when its contents, rights, package readiness, qualification, and
  publication authority permit it. GSD milestone numbers and software version
  numbers are separate.
- Keep any unresolved external, legal, physical, or subjective gate visible
  with its owner and next action. Do not claim a release or qualification that
  did not occur.

## Rights and security

Treat ROMs, BIOS, save data, firmware, memory cards, and cartridge metadata as
untrusted or rights-bearing inputs. Validate lengths and declared extents
before allocation or state mutation. Refused operations should have documented
status and state/buffer postconditions. Never infer permission from a public
download, a test-suite license, or a digest. Review source and license terms
for each redistributed fixture or dependency.

## Owner-authorized local content

The owner authorized agent-managed local copies of the two supplied GBA files
and agent-run logo-digest extraction/verification. Keep them under the existing
ignored `/private/owner-roms/` directory. This local permission supersedes the
earlier owner-must-run-the-helper procedure for these files; it grants no
permission to commit or upload their bytes or use them in public CI.
