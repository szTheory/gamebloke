# Phase 10 PPU and Playable 0 verification

This guide describes the selected local PPU model and its original, BIOS-free
polling scenario. For the general CI workflow, lanes, and test-inventory rules,
see [Verifying Gamebloke](verify.md). For result JSON fields and validation
rules, see [the evidence contract](evidence-contract.md).

## Run the bounded checks

Configure the synthetic-only CI build once if `build/ci` is absent or belongs to
another source tree:

```sh
cmake --preset ci
```

Run the cheap selection before longer frame, DMA, runner, or local guest work:

```sh
sh scripts/verify-phase10-fast.sh
cmake -DSRC=. -DBIN=build/ci -DCASE=order -P tests/gates/phase10_selection.cmake
```

The fast command requires `build/ci/CMakeCache.txt` to bind to this source root,
builds only the focused `bus_test` target, runs the Phase 7/8/9 exact fast
closures on this same build, validates Phase 10 selection and runs exactly six
cases: `phase10.policy`, `phase10.policy.control`, `ppu.boundary`,
`ppu.registers`, `ppu.refusal`, and `phase10.selection.control`. A skipped or
unrun selected case is a failure. The order command checks the registered
fixture graph and its failing-predecessor control before longer cases.

The local Playable 0 command repeats the fast gate before local generation,
build, admission, guest evaluation, and controls:

```sh
sh scripts/verify-playable0-local.sh
```

This command refuses when `CI` is set. Its generated images and results stay in
the ignored `build/local-playable0/` tree and are not added to the ordinary CI
or ASan inventories. It uses only the in-tree original guest; it does not fetch
or admit a third-party fixture.

## Reproduce the Playable 0 invocation

The local gate generates the guest at
`build/local-playable0/generated/fixtures/owned-playable0.gba`. During its
`verify` case, it writes this exact input file:

```text
0 0
1450000 16
1730000 17
2010000 1
2290000 0
```

The rows mean neutral, RIGHT press, RIGHT+A hold, release RIGHT while A stays
pressed, then release A. These are logical pressed-key masks (`0` means no keys,
bit 0 is A, and bit 4 is RIGHT). The emulated KEYINPUT register is a separate
active-low value; the final neutral baseline reports `KEYINPUT=1023`.

The production CLI invocation uses a 2,808,960-cycle budget and writes a
headerless raw frame. The CMake gate parses the CLI's one-line JSON from stdout
and checks the output file's length and digest:

```sh
build/local-playable0/gamebloke-run \
  --rom build/local-playable0/generated/fixtures/owned-playable0.gba \
  --fixture owned-playable0 \
  --cycles 2808960 \
  --input-script build/local-playable0/playable0-verify/neutral-right-hold-release.txt \
  --video-output build/local-playable0/playable0-verify/neutral-manual-1.raw
```

The output writer creates files exclusively. For a later direct run, choose a
new output filename that does not already exist; the wrapper command manages its
own fresh verification directory.

For the baseline input, the recorded model result is `PASS`, mailbox case
`ABE80000` (decimal `2884108288`), five input events applied, zero pending,
final KEYINPUT `1023`, and 2,725,239 cycles. The frame is valid at 240×160,
with 76,800 bytes; its sample cycle is 2,528,064 and completion cycle is
2,725,184. The baseline generated guest SHA-256 is
`9114432ee8b722080709c7171fde5fe56f2300a7deaf0c77588f85defc5ee899`; the
baseline raw frame SHA-256 recorded by the prior verification is
`738607b7b0066b36b01d86f446dddfe5400d51d4ecf2b941babebe384e4bded0`.

The semantic evaluator checks an 8×8 marker at `(64, 80)` in BGR555 color
`0x03E0`. A changed final input of RIGHT (`16`) produces mailbox case
`ABF00000`, final KEYINPUT `1007`, and the marker at `(128, 80)` with the
expected color. Exactly 256 bytes differ between the two frames: the old and
new 8×8 marker regions, two bytes per pixel. Input-disabled, wrong-address,
wrong-color, and one-byte frame-tamper controls each fail with a named semantic
reason. The runner checks the full raw frame bytes and repeats fresh and
interleaved baseline runs.

The input file has press, hold, and release events, but this bounded scenario
returns one final frame rather than a presentation stream. Its evidence proves
the final guest mailbox, input accounting, and pixel response; it does not claim
that each intermediate input transition was displayed as a separately captured
frame.

## Inspect raw pixels and model limits

The frame is headerless little-endian BGR555: 240 pixels × 2 bytes = 480 bytes
per row, 160 rows, 76,800 bytes total. Pixel `(x, y)` starts at byte offset
`2 * (240 * y + x)`. For example, inspect the baseline marker at `(64, 80)`
without a host renderer:

```sh
od -An -tx1 -j 38528 -N 2 build/local-playable0/playable0-verify/neutral.raw
shasum -a 256 build/local-playable0/playable0-verify/neutral.raw
```

The marker color `0x03E0` appears as bytes `e0 03`. The raw file is separate
from the runner's JSON result. Use the JSON fields for guest status, mailbox
semantics, machine cycles, input counts, frame validity, and video digest; use
the raw file for pixel bytes. A direct CLI invocation prints one JSON line on
stdout. The CMake gate captures and validates that line in memory, then prints
a concise PASS summary; it does not persist the JSON line as a separate file.
Redirect stdout to a new file under the ignored build tree when preserving a
separate result artifact is useful.

Only mode 3 with BG2 enabled and forced blank disabled is supported. Other
display modes, disabled BG2, forced blank, OBJ, windows, effects, and unmodeled
BG2 transforms produce `invalid` frames with a named reason. A frame with fewer
than 160 captured rows is `incomplete`. Neither state exposes old or partial
bytes as a fresh valid frame. A writer error is `HARNESS_FAIL` with
`video-output-failed`; the result names the writer failure and does not claim a
complete artifact. An unrequested frame has `raw_video.status: not-requested`.

Rows are sampled from canonical VRAM at the visible-line start boundary. When a
timed bus write and a row sample share a cycle, due PPU events are processed
before the write's register/memory effect, so that sample sees the prior
value. These are explicit project policies, not hardware latch claims. The
selected HBlank event is 1006 cycles after line start; it is classed as a
reference candidate and project-selected policy, linked to
[`disp-hblank-flag-edge`](../registry/disputes.json), and remains disputed with
hardware qualification `UNKNOWN`. See the [Phase 10 research](../.planning/phases/10-ppu-timing-and-first-pixels/10-RESEARCH.md)
for the 1006/1007/1008 evidence limits.

Playable 0 is a direct-start polling guest. It does not use a BIOS, audio,
saves, host rendering, a physical controller, a display, or host-clock timing.
Its generated content is marked `local-only`, and the manifest leaves
redistribution `unknown`; do not commit or upload its bytes. G6 is the owner's
subjective Playable 0 judgment and remains pending until the owner reviews the
[evidence](../.planning/phases/10-ppu-timing-and-first-pixels/10-EVIDENCE.md)
and [owner review packet](../.planning/phases/10-ppu-timing-and-first-pixels/10-OWNER-REVIEW.md).
G4 BIOS-stub approval, G8 third-party fixture terms, hosted full-current-input
evidence, physical qualification, and publication authority remain separate
closed gates.
