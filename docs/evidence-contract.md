# The evidence contract

This guide explains the data files that decide what a fixture run may claim: the
machine profile, the fixture manifest, the pins list, the dispute registry, and the
result document. Each file has one authority and one validator; the validators run
inside `cmake --workflow --preset ci` (see [verify.md](verify.md)). The reason codes
each validator prints are listed in the "Reason codes" section of verify.md.

A matching hash proves identity, not redistribution rights. A pin or a digest says
"these are the bytes that were reviewed"; it never says the bytes may be copied,
shipped, or published. Rights live in the manifest `license` and `permitted_use`
fields and are decided by the owner.

## Authority files

| Path | Schema id | Holds | Checked by |
| ---- | --------- | ----- | ---------- |
| `contracts/evidence.json` | `gamebloke.evidence/1` | The closed vocabularies: source labels, provenance classes, claim fields, and the nine result states with their rules | `cmake/CheckContract.cmake`, gate `contract.lint` |
| `profiles/agb.json` | `gamebloke.profile/1` | The machine profile: boot, BIOS provider, cart, eleven timing constants, the unmodeled list | `cmake/CheckProfile.cmake`, gate `profile.lint` |
| `registry/disputes.json` | `gamebloke.disputes/1` | Known disagreements between references, implementations, and reports | `cmake/CheckRegistry.cmake`, gate `registry.lint` |
| `tests/fixtures/manifest.json` | `gamebloke.manifest/1` | One row per fixture: source, license, build, BIOS need, provenance, permitted use | `cmake/CheckManifest.cmake`, gate `manifest.lint` |
| `pins.txt` | `gamebloke.pins/1` (header line) | Reviewed pinned fetches, one row per downloaded file | `cmake/CheckManifest.cmake` (pins mode), run by gate `manifest.lint` |
| `contracts/examples/*.json` | `gamebloke.manifest/1`, `gamebloke.result/1` | Illustrative documents used as positive inputs for the controls | `cmake/CheckManifest.cmake`, `cmake/CheckResult.cmake` |

Every JSON file is read by `cmake/GblJson.cmake`, a strict loader. CMake's own parser
accepts comments, trailing commas, content after the root object, and duplicate keys;
the loader rejects all four, so a hand edit fails the gate instead of passing silently.

The profile narrative is [profile-agb.md](profile-agb.md). `profiles/agb.json` stays
the authority; a check compares the document's unmodeled table and its
`profile_version:` line with the JSON, so the two change together.

## Claim tuple

A result never says only "PASS". It says what passed, under which claim. The claim has
six fields, named in `contracts/evidence.json` under `claim_fields`:

- `machine`: exactly `agb` in v1.0.
- `boot`: exactly `direct-start`. No result may claim boot-ROM behavior.
- `bios_provider`: one of the forms in the next section.
- `cart`: an object, `kind` exactly `rom-only`, and `rom_sha256` either null or 64
  lowercase hex.
- `input_timeline`: an object whose `kind` is `none`, or `script` with a 64-hex
  `sha256` of the input script.
- `evidence`: a non-empty array of items, each naming a `fixture` (a manifest row id)
  and a `provenance_class` from the vocabulary. The class must equal the one in that
  fixture's manifest row.

## BIOS provider

The accepted `bios_provider` forms (the R1 grammar) are:

- `none`
- `project-stub`
- `hle:` followed by one or more names of letters, digits, and underscores, separated
  by commas
- `replacement:` and a lowercase name, then an at-sign and a 40-character lowercase
  hex commit (shape `replacement:<name>@<commit>`)
- `user-official`, then an at-sign and a 64-character lowercase hex SHA-256 (shape
  `user-official@<sha256>`)

Names inside an `hle:` list are not checked against a table of BIOS calls. The older
milestone labels (M0, M1, M1b, M2) are retired and match none of the forms. The one
function that decides this is `gbl_bios_provider_valid` in `cmake/CheckContract.cmake`;
the profile validator and the result validator both call it.

## Result states

A run ends in exactly one of nine states. The table below mirrors
`contracts/evidence.json`, which stays the authority; `cmake/CheckContract.cmake` pins
the locked rows independently, so weakening the data fails the contract gate.

| State | `fixture.ran` | ROM digest | Reason | Refusal |
| ----- | ------------- | ---------- | ------ | ------- |
| `PASS` | true | required | optional | none |
| `GUEST_FAIL` | true | required | optional | none |
| `GUEST_STOP` | true | required | optional | none |
| `HARNESS_FAIL` | either | optional | optional | none |
| `TIMEOUT` | true | required | optional | none |
| `CANCELLED` | either | optional | optional | none |
| `MISSING` | false | optional | required | none |
| `SKIPPED` | false | optional | required | none |
| `REFUSED` | false | optional | optional | required |

A fixture that did not run is never PASS: `PASS` needs `ran` true and a digest, so a
missing, skipped, or refused fixture cannot be reported as passing. A no-BIOS
`GUEST_STOP` names the same exception in `reason` and `termination`. A refusal is an
object with a non-empty `code` and `postcondition`.

## Result documents

A result (`gamebloke.result/1`, an example is `contracts/examples/result-pass.json`) has
these fields:

- `fixture`: `id`, `ran` (a boolean), and `rom_sha256` (64 lowercase hex or null).
- `status`: one of the nine states, exact case.
- `reason` and `refusal`: as the state row requires.
- `claim`: the six claim fields above.
- `disputes`: registry ids. `labels`: result labels. `limits`: what the result does not
  prove, a non-empty list of non-empty strings.
- `semantic`: `{state, case_id}` from the neutral mailbox observation. `state` is
  `absent`, `pending`, `pass`, `fail`, or `malformed`. `case_id` is a canonical decimal
  string in uint32 range, or null when no case id is available. An absent observation may
  serialize its zero case id as `"0"`. Completed pass/fail mailboxes need a nonzero case id.
- `observation`: present only when the fixture's manifest row selects `register-eval`.
  It has exactly `kind` (`register-eval`), `observed` (boolean), `source_commit`, canonical
  `entry_address`, `instruction_set`, `register_index`, and `value`. The checker binds the
  identity fields to that manifest row and the result's exact runtime ROM digest. Before the
  evaluator entry is observed, `value` is null; afterward it is a raw uint32 JSON number.
  This is separate from `semantic`, which always reports the actual mailbox observation.
- `time`: `{cycles, instructions, budget_cycles, model, qualification, bus_cycles,
  internal_cycles, wait_cycles, dma_startup_cycles, dma_source_cycles,
  dma_destination_cycles, dma_completed_units, dma_pause_cycles}` for current producers.
  Counters are canonical decimal strings in uint64 range (`"0"` or a nonzero digit followed
  by digits), so JSON consumers do not round values above their exact integer range. With
  the phase 9 extension, elapsed cycles equal CPU `bus_cycles` plus CPU `internal_cycles`,
  `phase8.wait_cycles` when phase 8 is present, and DMA startup, source, and destination
  cycles. Current producer `time.wait_cycles` mirrors `phase8.wait_cycles` and is validated
  as the same observation; it is not added a second time. `dma_pause_cycles` equals the sum
  of DMA startup, source, and destination costs, describes the CPU pause window, and is not
  added to elapsed cycles a second time.
  The checker uses checked decimal arithmetic without converting the uint64 values to a
  floating point or signed machine integer. `model` is `gamebloke-timer-model-1` and
  `qualification` is `project-model-only`; these values describe implementation evidence,
  not a hardware timing claim. Observed cycles must not exceed the requested cycle budget.
  A bounded run may stop short when its next whole instruction or refill unit cannot fit;
  equality with the budget is not required for TIMEOUT, and exact-boundary mailbox completion
  is valid.
- `phase9`: the complete additive DMA observation. A present extension identifies model
  `gamebloke-dma-observation-1`, reports whether a machine snapshot was observed, and when
  observed carries WAITCNT, bus owner, CPU paused state, machine cycles, and four channels in
  channel order. Each channel reports programmed/current addresses, retained read latch,
  count/control, remaining units, completed units, and pending/armed/active/startup flags.
  Current `phase9` results also require the phase 8 timing observation; duplicated
  `time.wait_cycles` and `phase8.wait_cycles` must agree.
  When no machine execution was observed, those snapshot fields are null. The timing object
  repeats the five DMA counters in `time`; duplicate values must match and machine cycles must
  equal `time.cycles`. `time.dma_completed_units` counts every completed transfer in the run;
  each channel's `completed_units` is progress for its latest activation, so reactivating a
  channel can make the run total larger than the sum of the final channel snapshots. Channel
  count, bounds, and progress flags are checked together.
  The terminal `cpu_paused` and owner fields describe the observed machine state; instruction
  totals do not claim CPU retirement during paused DMA cycles.
- Phase 9 provenance stays explicit. Documented Game Pak wait values point to
  `nintendo-agb-programming-manual-v1.1-sections-3.3-3.3.2`; values for other regions are
  `project-selected`. The 128 KiB Game Pak boundary cites
  `gbatek-3.06-community-documentation`, which is community documentation rather than
  hardware qualification. The two-master-cycle startup value and retained-latch behavior are
  selected project policy, and hardware qualification remains `UNKNOWN`. A latch fallback
  observation is `disputed`, links `disp-dma-latch` in the result's top-level dispute list,
  and must remain open in the registry. DMA Prefetch is explicitly `unmodeled`.
- Whole-extension absence is the historical result/1 compatibility path. A current producer
  emits the complete `phase9` object and all `time.dma_*` fields, including zero values. If
  `phase9` is present, every required observation, policy, timing, and provenance field must
  have its declared type. Any `time.dma_*` field without `phase9`, or a partial `phase9`
  object, is rejected. The illustrative `result-pass.json` shows a current producer shape;
  tests remove the whole extension and its DMA time fields to verify historical compatibility.
- `phase10` and `raw_video` are a paired additive result extension. Historical result/1
  records may omit both; a record with only one is rejected. Current producers include both,
  including runs where PPU state was not observed or raw output was not requested. PPU
  observations bind their raster machine cycle to `time.cycles`, constrain line and pixel
  layout to the AGB profile, and keep unavailable frame epoch/sample/completion values null.
  A valid frame requires supported Mode 3/BG2 controls, reason `none`, one started epoch,
  all 160 visible row samples, and a completion cycle 160 profile line lengths after the
  sample cycle. Unsupported controls cannot yield a valid frame. Row sampling follows the
  profile's visible-line-start point; serialized pixels use a 480-byte row stride.
- Phase 10 HBlank evidence stays qualified as a reference candidate, with selected event
  offset 1006, source class `REF`, project-selected policy, dispute
  `disp-hblank-flag-edge`, and hardware qualification `UNKNOWN`. If an observation depends
  on an HBlank trigger, its classification and dispute id must match the profile and the
  result must disclose that dispute. This timing evidence is separate from Phase 9 DMA latch
  evidence. Automated PASS remains a fixture result; playable acceptance stays pending for
  owner evaluation and is not inferred from automation.
- Mode 3 with BG2 enabled, forced blank disabled, no OBJ/windows/effects, and neutral
  BG2 transform registers is the only supported capture state. Non-neutral BG2
  transforms report `bg2-transform-unmodeled`. Frame `state` is `valid`,
  `incomplete`, or `invalid`; `incomplete` has reason `incomplete`, while `invalid`
  must name an unsupported control reason such as `unsupported-mode`, `bg2-disabled`,
  `forced-blank`, `obj-enabled`, `window-enabled`, `effect-enabled`, or
  `bg2-transform-unmodeled`. A non-valid frame keeps epoch/sample/completion fields
  null and cannot emit stale or partial bytes. For requested output, incomplete and
  invalid frame statuses match the frame state and carry no artifact; a valid frame
  without an output artifact is `not-emitted`. Writer failure is `HARNESS_FAIL`
  with reason `video-output-failed` and a named failure, and does not claim a
  complete artifact. When output is not requested, the status is `not-requested`
  with no artifact.
- Visible rows are sampled from canonical VRAM at line start. At a shared cycle,
  scheduled machine/PPU events run before the timed write's memory/register effect,
  so the sample observes the prior value. This is project policy; it does not claim
  a hardware latch point. The selected 1006-cycle HBlank edge remains disputed and
  hardware qualification remains `UNKNOWN`.
- Raw video is headerless: the layout is 160 rows of 240 little-endian BGR555 pixels,
  480 bytes per row and 76,800 bytes total. The output
  digest is SHA-256 over those raw video bytes, independent from the ROM digest and input
  script digest. `requested: false` requires `not-requested`, `emitted: false`, and null
  artifact/failure metadata. `emitted` requires a valid frame, the exact byte count, and a
  lowercase SHA-256 digest. A named writer failure is reported as `HARNESS_FAIL` with
  `reason: video-output-failed`; no incomplete file is claimed as emitted. The writer uses
  exclusive create and removes a partial file after a write/close failure when cleanup
  succeeds. The result may retain a digest computed before an output-open failure, but its
  byte length remains null because no artifact was emitted. CLI argument/path refusals occur
  before guest execution and before file creation.
- A repeatable raw-video digest establishes byte repeatability for the same inputs and
  machine state. It does not establish that the pixels match physical GBA behavior; that
  needs a separately identified semantic oracle or hardware observation.
- `timer0`: `{if_request}` reports Timer 0's device interrupt-request bit independently from
  the semantic mailbox and CPU state. It is a boolean after at least one committed CPU unit;
  `false` means the executed machine was observed with the request bit clear. It is `null`
  when no CPU unit committed or execution was skipped, refused, or stopped before a request
  observation. A true IF request does not establish CPU interrupt eligibility, exception
  entry, or handler execution. A raw unregistered guest can therefore carry a semantic
  mailbox observation while retaining diagnostic `GUEST_STOP` status.
- `termination`: a named lowercase token describing why bounded execution stopped.
- `trace`: null when no trace is selected, or a trace object with a boolean `complete` field.

`cmake/CheckResult.cmake` cross-checks the result against the manifest and the registry:
`fixture.id` and every evidence fixture must be a manifest row id; the evidence class
must equal the row's class; `claim.cart.rom_sha256` must equal `fixture.rom_sha256`
(two nulls are equal); some evidence item must name `fixture.id`; and when the manifest
row is a `url` or `in-tree` row, a non-null `fixture.rom_sha256` must equal the row's
`source.sha256` (`claim-mismatch`). A source-built `git` row may instead carry an optional
`build.artifact` object with exactly `path` and `sha256`; that SHA is the generated ROM
identity checked by the result validator and projected into the runner's existing
`rom_sha256` slot. The git `source.commit` remains the reviewed source revision, and
`build.toolchain` remains tool identity. A legacy git row without `build.artifact` still
has no output digest and the production runner refuses it with
`fixture-digest-unavailable`.

Dispute disclosure: when the manifest row for the fixture, or for any evidence fixture,
carries a dispute id, the result's `disputes` list must contain it, and a non-empty
`disputes` list requires the `disputed` label. The `disputed` label with an empty list
is rejected. Every id in `disputes` must exist in the registry. `disputed` is the only
label; a new label would arrive with its own rule and a `gamebloke.result/2` bump.

The result validator also requires semantic, time, termination, and trace fields. For a
mailbox fixture, PASS still requires `semantic.state: "pass"`, a nonzero decimal-string
`semantic.case_id`, positive guest progress, `termination: "mailbox-complete"`, and cycles
within budget. GUEST_FAIL still requires `semantic.state: "fail"` and the same
complete-mailbox termination. A register fixture instead requires an observed register
result bound to its manifest source revision, exact runtime digest, entry address, instruction
set, and register index. Zero with positive guest progress and
`termination: "register-eval-complete"` is PASS; a nonzero uint32 value with the same
termination and progress is GUEST_FAIL. An unobserved value is pending and cannot produce
either verdict. The manifest selects the route, so adding an observation object cannot relax
the legacy mailbox rules. For both routes, `GUEST_STOP` no-BIOS exceptions require `reason`
and `termination` to name the same stop. `HARNESS_FAIL` with `reason: "trace-output-full"`
must use the same termination and report `trace.complete: false`. An incomplete trace cannot
be PASS. Other optional top-level notes remain informational.

The semantic/time/termination findings are:

- `missing-field`: required `semantic`, `semantic.state`, `semantic.case_id`, `time`, one of
  its three counters, `termination`, or `trace` is absent (null is allowed only for `trace`
  and the documented no-case-id forms).
- `bad-semantic`: semantic object/state/case id has the wrong type, unknown value, or an
  invalid uint32 decimal representation.
- `status-semantic-mismatch`: a mailbox PASS lacks a completed PASS mailbox and positive
  guest progress, or mailbox GUEST_FAIL does not carry a completed FAIL mailbox.
- `bad-observation`: register observation is missing, malformed, unbound to the manifest
  channel or exact ROM digest, or contradicts the observed value, status, progress, or
  termination; mailbox rows cannot carry a register observation.
- `bad-time`: a counter is not a canonical decimal string or `time` is not an object.
- `time-overflow`: a canonical decimal time counter is greater than
  `18446744073709551615`.
- `partial-phase9-extension`: one or more `time.dma_*` fields is present without `phase9`.
- `bad-phase9-type`, `bad-phase9-owner`, `bad-phase9-channel`, `bad-phase9-progress`,
  `bad-phase9-timing`, `bad-phase9-policy`, `bad-phase9-latch`, and
  `bad-phase9-prefetch`: a current DMA extension has an incomplete or ill-typed observation,
  inconsistent ownership/channel state, mismatched timing contributions, unsupported
  provenance/qualification, missing dispute disclosure, or a modeled Prefetch claim.
- `partial-phase10-extension`: `phase10` and `raw_video` do not appear together.
- `bad-phase10-type`, `bad-phase10-raster`, `bad-phase10-machine-time`,
  `bad-phase10-frame`, `bad-phase10-frame-validity`, `bad-phase10-policy`,
  `bad-phase10-dependency`, `bad-phase10-video-output`, and `bad-phase10-video-digest`:
  Phase 10 has an incomplete or ill-typed PPU/video observation, inconsistent raster/frame
  timing, unsupported policy or hardware claims, missing HBlank dispute disclosure, or
  contradictory raw artifact metadata.
- `time-sum-overflow` and `bad-time-components`: the checked CPU/HALT/DMA sum exceeds uint64
  or differs from `time.cycles`.
- `cycles-exceed-budget`: `time.cycles` is greater than `time.budget_cycles`.
- `bad-termination`: the token is non-string/invalid or conflicts with status; a missing
  top-level token is `missing-field`. A no-BIOS GUEST_STOP needs a reason equal to its
  termination.
- `bad-trace`: `trace` is neither null nor an object with a boolean `complete` field.
- `incomplete-trace`: PASS claims an incomplete trace, or trace-output-full does not report
  an incomplete trace.

## Runner and mailbox boundary

The installed `gamebloke-run` interface is:

```text
gamebloke-run --rom PATH (--cycles DECIMAL | --frames DECIMAL) \
  [--fixture ID] [--trace-mask INTEGER] [--trace-capacity INTEGER] \
  [--trace-output PATH] [--video-output PATH]
```

`--rom` supplies one bounded input file. Exactly one of `--cycles` or `--frames` is
required; decimal arguments are checked uint64 values, and frame budgets convert using the
profile frame-cycle constant. `--fixture` selects validated metadata embedded at build time.
The trace flags select implemented instruction, bus, or exception records, cap storage at
65,536 records, and optionally write JSON Lines to the explicit output path. `--video-output`
requests the raw BGR555 frame at the explicit path; it follows the same whole-cycle/frame
budget and input script as guest execution, with no host-clock pacing. Repeating the flag or
providing it without a path is rejected before execution, as are aliases to the ROM, input,
trace, or JSON result destination. Output is created exclusively. Usage errors exit 2, PASS exits 0, and valid
non-PASS results exit 1. The executable does not accept a runtime CMake, manifest, or
fixture-index path and does not need the source tree after it is built.

For example, `gamebloke-run --rom PATH --cycles 280896 --video-output frame.raw` requests a
single bounded frame's raw bytes. The selected guest must reach a complete supported frame;
unsupported modes remain a reported output failure rather than a successful empty capture.

The production fixture index is generated only from `tests/fixtures/manifest.json`, which
now admits the source-owned ARMv4T diagnostics against their exact artifact hashes
and immutable producer revision. The separate `gamebloke-run-control` binary and its generated manifest
exist only for local validation mechanics. Their rows and generated instruction images are
artificial, `local-only`, and non-redistributable with unknown rights; they are not actual
fixture approvals and must never be copied into the production index or described as grants.
The same artificial control output is rejected by the canonical production manifest as an
unknown fixture. Running a raw path without a registered `--fixture` can report an honest
diagnostic such as `GUEST_STOP` with `unregistered-content`, semantic observation, and
machine time, but it has no claim evidence and cannot validate as a manifest-backed result.

The mailbox is a state/time-neutral 20-byte peek at IWRAM address `0x03007FC0`: five
little-endian 32-bit words at offsets 0, 4, 8, 12, and 16 hold magic `0x47424C4B`, version
1, state (`absent=0`, `running=1`, `pass=2`, `fail=3`), case id, and completion marker
`0x434F4D50`. Guest code writes the completion marker last. PASS/FAIL is complete only when
magic, version, terminal state, and marker all match. A zero-filled mailbox is absent; a
running state or terminal state without the marker is pending; malformed fields do not
produce a verdict. The producer checks the mailbox at completed execution-unit boundaries
and gives a complete mailbox precedence over coincident budget exhaustion. It serializes the
observed state and case id separately from result status, cycles, instruction count, and
budget.

Dispute ids and the exact `disputed` label are copied from the validated build-time metadata
into producer output. The producer-to-validator test feeds actual CLI stdout to
`CheckResult.cmake`, then mutates copies of that output to prove missing semantic fields,
pending PASS, malformed case id/time, overflow, budget overrun, removed dispute disclosure,
and incomplete trace are rejected for their named reasons. This proves the configured
producer/checker path only; synthetic metadata remains an artificial validator input.

Current evidence is limited to the direct-start profile and the implemented
ARM7TDMI ARMv4T families in the version 6 scope/policy tables in
[profile-agb.md](profile-agb.md#cpu-instruction-scope). The original ARM/Thumb
corpus has admitted production fixtures and actual local execution receipts;
current-source verification, hosted qualification and owner acceptance are
separate gates. See [the verification guide](verify.md#original-armv4t-diagnostics)
and the current Phase 5 final-check record for their exact status. Unpredictable operand
stops, recognized later extensions, reserved UND and absent-coprocessor UND have
separate causes. Explicit executing project policies and static target refusals
are listed in the profile. Public unsupported values remain compatible without
inventing a legal unimplemented instruction. There is no BIOS execution/HLE, audio,
physical-controller discovery or host presentation. DMA, WAITCNT timing, and the limited
Mode 3/BG2 display path have project-model behavior; DMA Prefetch and other display modes,
transforms, objects, windows, and effects remain unmodeled. Timer 0 and bus cycle observations
use project-selected model behavior; region-specific hardware waits, display timing, and
physical-device behavior remain unqualified.

The CPU exception API, shared run-reason classifier, and JSON mapping cover reset,
undefined, SWI, prefetch abort, data abort, IRQ, FIQ, and coprocessor stop reasons. Under the
current guest profile, guest instruction/CLI runs directly trigger only SWI, undefined, and
coprocessor stops. Reset, IRQ, FIQ, prefetch-abort, and data-abort mappings are verified
through the CPU API and JSON producer tests, but are not currently guest- or CLI-triggerable.
This distinction does not imply BIOS exception handling or hardware qualification.

Instruction and read bus trace values record observed bus results. A successful CPU write
currently receives `value: 0` from `gbl_bus_write`, and the trace copies that result value;
write records therefore do not preserve the attempted payload. This is an implementation
limitation, not evidence of a physical write latch value.

The result is not hardware qualification, broad game compatibility, a BIOS-backed boot, or
a rights determination. The phase's unresolved spec-less probe classifications and the
standing restrictions on repurposing rights-bearing inputs or inflating compatibility
claims remain for phase verification; no human judgment is inferred from these controls.

### Reading Phase 5 CPU evidence

The canonical crosswalk is `diagnostics/armv4t/cases.json`. Each row names its
instruction/form, independent derivation, fixed ID, execution boundary, and
positive/broken fixtures or native API evidence. `cmake/CheckArmCoverage.cmake`
checks the complete legal family/form set, including all 19 Thumb formats;
the execution-set gate binds those rows to actual results. A complete ledger
and a passing execution set answer different questions and both are needed.

| Named area | What to inspect | Limit |
| --- | --- | --- |
| CPU-1 | ARMv4 version/classification boundaries and explicit operand policies | Unpredictable behavior uses a documented project choice. |
| CPU-2 | Shifter counts, carry, immediate/register boundaries and defined flags | Preserved unpredictable flags are policy, not measured silicon. |
| CPU-3 | Instruction-specific visible PC, alignment, branch/refill and stored PC | Direct-start profile and modeled bus only. |
| CPU-4 | Empty/base-in-list block and stack transfers, bank/order and writeback | Invalid-list policies are distinguished from defined transfers. |
| CPU-5 | Byte/halfword/signed accesses and misaligned-load behavior | The cited exact-core rules and explicit GBA policies remain separate. |
| CPU-6 | All six multiply forms, defined N/Z and non-S preservation | Short C and long C/V treatment is deterministic policy. |
| CPU-7 | Separate Thumb BL fetch/retirement and intermediate LR/PC | Injected IRQ is a native CPU API boundary, with no live interrupt controller. |
| CPU-8 | PSR masks, privilege/banks, SPSR and exception returns | Absent-SPSR and invalid-mode handling remains explicit. |
| CPU-9 | ARM 24-bit and Thumb 8-bit SWI comments, UND and coprocessor causes | Named BIOS-none stops do not execute a handler. |

Semantic fixture pairs execute the tested instruction and the same assertion
path; the broken member changes an expected operand and must report the same
case ID with `GUEST_FAIL`. That proves the diagnostic detects its deliberately
broken expectation. Native semantic mutants separately test selected core
faults. Neither is an independent physical GBA oracle. Guest-stop rows report
their named cause with pending semantics; API-only rows have no invented ROM
or CLI PASS. Trace writes record bus return values, so stored data assertions
use actual guest reads rather than treating write trace values as payloads.

The original fixture notice permits only necessary compilation/execution in
Gamebloke public CI. Generated ROMs remain local build outputs, with exact-name
signature scans and original-object-only link-map checks. Source/tool/output
pins establish identity; they grant no additional redistribution permission.
The installed production relocation gate checks immutable admitted identity
and actual observations with explicit owned input, alongside the raw-image and
missing-executable controls; it does not qualify a stable public ABI or release.

The probe audit surfaced 11 rows, and all 11 remain unresolved. Five have category
labels: CPU-04 bank adjacency, empty User/System SPSR, and exception-entry ordering;
RUN-01 unit-budget boundaries and uint64 precision. The other six are unclassified:
CPU-03, CPU-05, PROF-03, RUN-02, RUN-03, and RUN-04. A category label does not resolve a
row. The exact audit snapshot is retained in
[04-PROBE-STATUS.json](../.planning/phases/04-cpu-execution-core-and-headless-runner/04-PROBE-STATUS.json).
Concrete implementation tests do not silently resolve those taxonomy rows. Two descriptor-less project-policy items also remain flagged:
rights-bearing/private inputs must not be repurposed and synthetic metadata grants no rights;
result documents must not inflate hardware, BIOS, or compatibility claims. No checker or
human-pass status is fabricated for them. Full ISA/toolchain diagnostics are Phase 5 work;
deeper determinism work is Phase 6 work.

## Dispute registry

`registry/disputes.json` holds entries with `id`, `summary`, `status`, `affects`, and
`sources`. An id matches `disp-` followed by lowercase letters, digits, and dashes. The
only status is `open`. Each source has a `label` from the vocabulary, a `ref`, and a
`note`. Six seed ids are pinned by the validator, so deleting or renaming one fails the
gate: `disp-dma-latch`, `disp-gbatek-errata`, `disp-hblank-flag-edge`,
`disp-mgba-suite-22`, `disp-mgba-suite-24`, and `disp-mgba-suite-27`. A profile value
that is not settled carries the label `UNK` and a dispute id, never a number.

## Manifest rows

`tests/fixtures/manifest.json` has a `fixtures` array. Phase 5's source-owned
diagnostics supply its first admitted rows. Source revision, actual ROM digest,
expected-value provenance and permitted use are separate facts. The row fields and
the EVID-01 item each one satisfies:

| Field | Satisfies | Rule |
| ----- | --------- | ---- |
| `id` | identity | `^[a-z0-9][a-z0-9._-]*$`, unique |
| `source` | source and revision | `kind` is `git`, `url`, or `in-tree`; see below |
| `license` | license and notices | `spdx`, a non-empty `notices` array, and `redistributable` of `yes`, `no`, or `unknown` |
| `build` | build recipe, toolchain and optional output identity | `recipe`, `toolchain.identity` plus `toolchain.sha256` (null allowed), and optional `artifact: {path, sha256}` |
| `bios_requirement` | BIOS need | `none`, `project-stub`, `hle`, `replacement`, or `user-official` |
| `expected` | expected-value provenance | `provenance_class` from the vocabulary; `dispute` is null or a registry id |
| `permitted_use` | where it may run | a non-empty unique array of `public-ci`, `local-only`, `differential-only` |
| `fetch` | how it is obtained | optional; when present, `pin_dest` is a relative path |

Source kinds and pinning rules (D-13, R-3): a `git` row carries a `url` (https only) and
a `commit` of exactly 40 lowercase hex; a branch or tag name is not a pin. For a built
git fixture, optional `build.artifact.path` is a normalized, nonempty path relative to the
explicit current `GBL_ARTIFACT_ROOT`, and `build.artifact.sha256` is the lowercase SHA-256
of the generated ROM bytes. The artifact object has exactly those two keys and is valid
only for a git source row. Index generation can use the committed metadata pin when an
optional local output is absent; when the lane supplies `GBL_ARTIFACT_ROOT` and the output
exists, generation checks its containment and bytes. The runtime runner always compares
the ROM it is asked to execute with the immutable indexed digest. A `url` row carries an
https `url` and a 64-hex `sha256`. An `in-tree` row carries a relative `path` and a
`sha256`, and has no `fetch`. A `disputed` provenance class needs a dispute id, and a
dispute id under any other class is rejected.

The provenance classes are `project-authored`, `hardware-observed`,
`reference-emulator`, `suite-self-checking`, and `disputed`.

## Pins and fetch

`pins.txt` starts with the header line `# gamebloke.pins/1 <sha256> <url> <dest>`. Each
row after it has three single-space-separated fields: a 64-hex digest, an `https://` URL,
and a destination relative to `.fixtures/`. A pin names one raw file at a 40-hex commit
or a maintainer-uploaded release asset, never a generated archive. A header-only file is
the stated zero-row state. A manifest row's `fetch.pin_dest` must match a pins row, and
the digest or commit must agree (`pin-mismatch` otherwise). So must the origin: a `url`
row's pinned URL equals its `source.url`, and a `git` row's pinned URL sits on the
`source.url` host (`raw.githubusercontent.com` counts as `github.com`) under its
repository path and holds the commit. A pins row that no manifest row names is an
orphan pin and also `pin-mismatch`. A `git` or `url` row with no `fetch` is allowed; it
is just not fetched by `fetch-pin.sh`. URLs carry no userinfo (`bad-url`).

A row that lists `public-ci` in `permitted_use` must have `license.redistributable` set to
`yes` and a `bios_requirement` other than `user-official` (`rights-mismatch`). A matching
hash proves identity, not redistribution rights; this rule only stops a row from
contradicting its own rights fields.

Fetch a pinned file with:

```sh
sh scripts/fetch-pin.sh [--pins <file>] [--root <dir>] <dest>
```

The script downloads into a temporary directory on the same filesystem as the
destination, hashes the payload, and moves it into place only when the digest equals the
pin. A mismatch leaves nothing at the destination. Exit status 0 means
`fetch-pin: ok: <dest>`; exit 2 covers usage, a missing pins file, a missing curl, and a
missing cmake; every other failure exits 1. curl runs with `-q` (no user curlrc) and does
not follow redirects, so a pin names the direct URL and a 3xx answer is refused as
`bad-status`; an empty download is `empty-response`. The script never disables
certificate verification, and the SHA-256 compare, not the transport, is the integrity
guarantee. The destination's existing parent directories are resolved before anything is
created, so a symlinked parent cannot make the script create directories outside the root.
`file://` URLs work only with `GBL_FETCH_ALLOW_FILE=1`, which exists for the tests;
the committed `pins.txt` never contains one, and the tests never use the network.

Fetched bytes land in `.fixtures/`, which git ignores. The gate `fetch.untracked`
(`cmake/CheckFixtureDir.cmake`) proves the ignore rule is present and that nothing under
`.fixtures/` is tracked, so a forced add fails.

## Reading further

- [verify.md](verify.md): the verify command, lanes, and every reason code.
- [verification.md](verification.md): Phase 10's exact fast gates, local Playable 0
  input, raw frame layout, and model/evidence limits.
- [profile-agb.md](profile-agb.md): the profile narrative and its sync check.
