# Phase 2: Profile, Fixture Manifest, and Result Contract - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Results and fixtures get a defined, machine-checked vocabulary. This phase delivers
the versioned `agb` profile (PROF-01), the claim tuple on every result (PROF-02),
the fixture manifest schema and linter (EVID-01, EVID-02), the SHA-256 pinned fetch
(EVID-03), the nine-state result taxonomy (EVID-05), and the dispute registry
(EVID-06). It also completes the "lints the manifest" clause of VERIF-01 by
registering the manifest linter in the canonical `cmake --workflow --preset ci`
command at the `manifest.lint` extension point Phase 1 left in `CMakeLists.txt` and
`docs/verify.md`.

Not in scope: the runner emitting result JSON (Phase 4), any fixture content or
toolchain (Phase 5), third-party fixtures (Phase 6), PROF-03 enforcement (Phase 4),
and any bus/CPU behavior.

</domain>

<decisions>
## Implementation Decisions

### Contract formats and validation tooling
- The profile, manifest, result contract, and dispute registry are JSON files.
  They are machine-readable authority files; the Phase 4 runner will emit JSON.
  `string(JSON)` is available at the CMake 3.25 floor.
- Validation is hand-written CMake `-P` validators with stable reason codes, in the
  style of `cmake/CheckSignatureDigests.cmake`. No JSON Schema tool or other new
  dependency.
- `profiles/agb.json` is the authority for the profile. `docs/profile-agb.md` is
  narrative that cites it; a check keeps the unmodeled list in the JSON and the
  document in sync.
- Every validator reason code has an `ExpectFail` negative control built from
  synthesized mutations at test time (no committed bad-example fixtures), registered
  in `tests/gates.cmake` with matching lines in the sorted `tests/inventory/<lane>.txt`
  files, and a positive control per validator.

### The `agb` profile content
- Every timing constant carries exactly one source label from the project's evidence
  vocabulary: `REF`, `SRC`, `IMPL`, `INF`, `UNK`. `calibrated:<fixture>` is reserved
  for later phases.
- The profile id is `agb` only. The SP is `agb` plus a host presentation preset,
  claimed only with evidence. `agb-micro` is deferred until owner hardware exists
  (G12). SP/AGS naming follows research position R8.
- The profile declares `schema: gamebloke.profile/1` and an integer `profile_version`
  that is bumped on any claim-affecting change.
- The unmodeled list has the three named items plus module-level entries. Named:
  SP/Micro machine-visible differences (the 0x04000800 EWRAM-wait register, the Micro
  lacking the cartridge switch), prefetch until modeled, and the HBlank flag edge (U1;
  sources disagree at 1,006, 1,007, and 1,008 cycles, so never hard-code 960).
  Module-level not-yet-modeled: PPU, audio, DMA, timers, IRQ controller, backup media.

### Claim tuple, result taxonomy, and dispute registry
- The claim tuple is `{machine, boot, bios_provider, cart, input_timeline, evidence}`.
  `cart` is `{kind: "rom-only", rom_sha256}`. `input_timeline` is `{kind: "none"}` or a
  script digest. `evidence` is a list of `{fixture, provenance_class}`.
- `bios_provider` follows exactly the R1 grammar: `none`, `project-stub`,
  `hle:<swi,...>` (list non-empty), `replacement:<name>@<commit>`,
  `user-official@<digest>`. Anything else is rejected; SWI names are not checked
  against a table. STACK's M0/M1/M1b/M2 BIOS labels stay retired.
- The nine states are data: PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT,
  CANCELLED, MISSING, SKIPPED, REFUSED. The result validator rejects `PASS` unless the
  fixture ran with a digest present; `SKIPPED` and `MISSING` require a reason;
  `REFUSED` requires a refusal code and a postcondition note; an unknown state is
  rejected. A fixture that did not run is never PASS.
- `registry/disputes.json` holds entries with an id, a summary, labeled sources, a
  status of `open`, and affected areas. Seeded with: the DMA-latch dispute, the GBATEK
  errata, the HBlank edge sources, and mGBA-suite issues #22, #24, #27. A fixture whose
  expected value points at an entry yields a result labeled `disputed`.

### Manifest linter and pinned fetch
- Git sources must pin a 40-hex commit (a tag may appear only as an extra field).
  Non-git sources must pin a SHA-256. Branch names, `HEAD`, and `main` are rejected.
- Expected-value provenance classes: `project-authored`, `hardware-observed`,
  `reference-emulator`, `suite-self-checking`, `disputed`. An unknown class is rejected.
- `pins.txt` rows are `<sha256> <url> <dest>`. A POSIX `scripts/fetch-pin.sh` verifies
  the digest before placing the file in the untracked `.fixtures/` directory (added to
  `.gitignore`). A mismatch exits non-zero with a loud error. No third-party binary is
  ever committed.
- Tests never use the network. The digest-mismatch control uses a locally synthesized
  file through a `file://` URL. No third-party fixture is fetched in this phase.

### Claude's Discretion
File layout beyond the named paths, exact reason-code names (stable once chosen),
manifest row field names (must cover every EVID-01 field: source URL, pinned revision,
license and notices, build recipe, toolchain identity/digest, BIOS requirement,
expected-value provenance class, permitted use), JSON key ordering, and the number of
synthesized mutations per validator.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `cmake/ExpectFail.cmake` (`gbl_expect_fail`, `gbl_expect_pass`, `EXPECT=pass` mode) for
  every negative and positive control.
- `cmake/GblTests.cmake` (`gbl_add_test` with name and label rules) and the per-lane
  sorted inventories `tests/inventory/{ci,hygiene,asan}.txt` checked at configure time
  and against junit (`scripts/check-junit.sh`).
- `tests/gates.cmake` is the registration point for gate tests; `tests/gates/*_case.cmake`
  and `tests/ci/*_cases.cmake` show the synthesized-mutation case-driver style.
- `cmake/CheckSignatureDigests.cmake` and `cmake/RulesetCheck.cmake` show `string(JSON)`
  based validators with reason codes under the 3.25 floor; every `-P` entry script
  starts with `cmake_minimum_required(VERSION 3.25)`.
- `scripts/hygiene.sh` and `scripts/signature-scan.sh` must stay clean on every new file
  (no home paths, non-noreply emails, credential shapes, ROM/BIOS/logo bytes).

### Established Patterns
- POSIX sh scripts with `set -eu`, NUL-safe iteration, EXIT cleanup plus separate
  HUP/INT/TERM exit traps (`scripts/check-traps.sh` enforces the idiom).
- Findings print `path:line` or reason codes only, never the matched text.
- Controls assert a non-zero exit plus a stable reason text, never an exact exit code.
- Counts: ci lane 120 expected (118 ran, 2 skipped), hygiene 50, asan 16 before this phase.

### Integration Points
- `CMakeLists.txt` `manifest.lint` extension point and `docs/verify.md` ("manifest lint
  joins in Phase 2").
- `.gitignore` gains `.fixtures/`.
- CI classifier treats source changes as heavy, so this phase's PR runs every lane.

</code_context>

<specifics>
## Specific Ideas

- The same reason-code and control discipline as Phase 1: a validator that cannot fail
  is not a gate.
- Profile unmodeled entries should read as honest limits, not TODOs.
- Publication follows the owner's standing policy: one PR for this phase, squash-merged
  through the ruleset when `CI required` is green (stop on red, no bypass).

</specifics>

<deferred>
## Deferred Ideas

- Runner emitting the result JSON, and PROF-03 enforcement: Phase 4.
- Fixture content, toolchain identity, and manifest rows for owned diagnostics: Phase 5.
- jsmolka third-party manifest rows and pinned fetch of real suites: Phase 6.
- A JSON Schema document for external consumers: revisit when an external consumer exists.
- Validating SWI names against a table: revisit when the project-stub or HLE exists.

</deferred>
