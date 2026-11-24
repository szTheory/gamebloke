# Installed Gamebloke C consumer

This example uses only the installed `gamebloke::api` target and
`gamebloke/gamebloke_v1.h`. Build it outside the Gamebloke source tree with the
same target architecture and native C ABI as the installed library:

```sh
cmake -S examples/installed-consumer -B /tmp/gamebloke-consumer-build \
  -Dgamebloke_DIR:PATH=/path/to/install/lib/cmake/gamebloke
cmake --build /tmp/gamebloke-consumer-build --target gamebloke-installed-consumer
/tmp/gamebloke-consumer-build/gamebloke-installed-consumer \
  /path/to/phase21-diagnostic.gba /path/to/consumer-evidence
```

The complete local proof, including fixture generation, component install,
prefix relocation, fresh external build, replay, and intentional negative
controls, is:

```sh
sh scripts/check-installed-consumer.sh --negative-controls
```

The same relocated consumer checks failure-atomic refusals and retries in each
fresh run. Its normalized receipt records the expected status, all public
observation fields before and after, result and media sentinels, request bounds,
and whether the corrected operation matches the clean reference. The cases are
an undersized advance descriptor (`invalid_argument`), an even ROM span shorter
than `0xC0` while valid content is mounted (`content_refused`), one-byte-short
audio and video destinations (`insufficient_capacity`), and a positive 2048
cycle budget that cannot complete the requested unit goal (`work_exhausted`).
Each refusal is retried with valid inputs; the audio-capacity and exhausted-work
cases compare a full successful continuation against a fresh reference. The
video case retries the complete frame read against the same valid frame.

`--negative-controls` mutates a local caller audio canary after an otherwise
valid refusal and requires the matching atomicity predicate to reject it. This
control changes neither the fixture nor the installed library.

## Requirements and evidence

The local script requires a C17 host compiler, CMake 3.25 or newer, Ninja,
Git, `shasum`, and standard POSIX shell utilities. The source-built guest uses
the host compiler; the pinned ARM toolchain is not required. The measured
fixture recipe, compiler identity, target, source revision, source digest,
output digest, BIOS mode, and local authorization are recorded in
`.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json`.

Generated ROM bytes remain under the ignored
`build/phase21-local/fixture/` directory. A successful invocation retains its
normalized observations, raw frame and audio files, package inventory,
compiler/linker inputs, hashes, control outcomes, and elapsed time under
`build/phase21-local/evidence/<run-id>/`. Hashes establish byte identity and
repeatability; the consumer and manifest checker independently assert guest
semantics and literal media values.

If the admitted ignored ROM already exists, the harness preserves its bytes and
checks the manifest-bound digest instead of regenerating it. The local refusal
receipt is part of the installed-consumer evidence, not public CI input.

The consumer selects `agb` profile version 16, direct start, a ROM-only
cartridge, and no BIOS. It checks a cycle-stamped A/B input timeline, a finite
cycle ceiling, case 21001, BGR555 little-endian pixels, and pre-PWM stereo
S16LE samples. The local control shifts A past its guest observation window
while preserving the final logical mask and expects the guest FAIL verdict.
Separate check-only mutations verify that pixel and sample mismatches fail at
their literal predicates.

This diagnostic is authorized for local Phase 21 verification only. Its
license and redistribution status are not asserted. Do not put it in public
CI, upload it, package it, redistribute it, or use it outside the recorded
scope. Audio is pre-PWM model output and hardware qualification remains
UNKNOWN; a successful proof does not establish broad game compatibility.
