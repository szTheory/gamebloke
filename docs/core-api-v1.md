# Gamebloke Core C API v1

Gamebloke v1 exposes one opaque, deterministic machine through a native C ABI.
The public declarations live in `gamebloke/gamebloke_v1.h`; internal machine
structures are not part of this contract. `gbl_v1_*` names and `struct_size`
fields identify the v1 symbol and structure set. Structure layouts use the
target's native pointer, integer, alignment, and `size_t` ABI. They are not a
wire format and must be built for the same target ABI as the library.

The only accepted profile is `agb` profile version 16, direct start with no
BIOS provider, and a ROM-only cartridge. The API does not provide a host clock,
file access, persistent save writes, controller discovery, windowing, or media
presentation. A guest mailbox verdict is a guest observation and does not
change an API status or establish hardware compatibility.

## CMake consumer

Install the local static API component, then link the exported target:

```sh
cmake --install build/ci --prefix /tmp/gamebloke-install \
  --component GameblokeCoreApi
```

```cmake
find_package(gamebloke CONFIG REQUIRED)
target_link_libraries(my_host PRIVATE gamebloke::api)
```

Include only the stable header:

```c
#include <gamebloke/gamebloke_v1.h>
```

The `GameblokeCoreApi` component contains the static archive, its CMake target
export, package config, and this stable header. It does not install private
core headers or sanitizer-built artifacts.

## Lifetime and memory

`gbl_v1_create` requires a full `gbl_v1_config` prefix with the `agb`, direct,
ROM-only selectors and a caller allocator pair. On success it writes an opaque
handle to the separate output slot. On failure the output slot is unchanged.
The API allocates the handle and its bounded scratch through these callbacks;
ROM loads use the same callback pair. A failed allocation returns
`GBL_V1_ALLOCATION_FAILED`; a retry is allowed.

Allocator callbacks must return distinct live allocations with ordinary
`malloc`-compatible alignment. `release` must accept the same context, pointer,
and exact byte size once for every successful allocation. Keep the callback
context alive through `gbl_v1_destroy`. Allocation callback side effects remain
visible if a later allocation fails. Callbacks must not re-enter the API.
Calls on one handle must be serialized by the caller. Independent handles may
be used independently.

`gbl_v1_destroy(NULL)` is a no-op. `gbl_v1_unload` is idempotent and releases
mounted content while leaving the configured instance reusable. Reset requires
loaded content, preserves the copied ROM, and clears mutable machine, input,
and capture state. Reset while unloaded returns `GBL_V1_INVALID_LIFECYCLE`.

## Sized structures and pointer rules

Every extensible structure starts with `struct_size`. Pass at least the
required v1 prefix. Longer supported structures are accepted; input bytes after
the known structure are ignored. Output functions write only the known prefix
that fits in the declared size and leave caller tail bytes unchanged. The
`gbl_v1_advance_options` prefix ending after `cycle_budget` is supported and
selects no audio capture.

The caller must pass live, correctly aligned C objects of the declared size,
readable input spans, and writable output spans. The API checks arithmetic
wrap, known extents, and overlap with its handle, scratch, owned ROM, and other
known spans. C cannot establish that an arbitrary non-null pointer is mapped,
live, or backed by the declared physical object extent. Those remain caller
preconditions.

## Content and input

`gbl_v1_load_rom` copies the caller's bytes before publishing the replacement.
Content must be an even-sized ROM-only image from 0xC0 through 0x02000000 bytes.
An invalid size or allocation failure leaves the prior mounted content and
machine state unchanged. The caller may reuse or modify its input bytes after
the call returns.

`gbl_v1_set_input` copies up to 256 events. Each event has a guest master-cycle
timestamp and an active-high logical mask using `GBL_V1_KEY_*` bits. Timestamps
must not be in the past and must be nondecreasing. Events with equal timestamps
are applied in supplied order. Installing a new timeline replaces pending
events; a zero-count timeline cancels pending events and preserves the current
logical key state. Reset clears both the current mask and timeline.

Observation reports current logical input, applied and pending event counts,
timeline generation, machine master cycles, and total retired instructions as
separate values. Guest KEYINPUT reads are active-low. They are machine behavior,
not the logical mask's encoding.

## Bounded advance

`gbl_v1_advance` requires a loaded machine, a positive number of complete core
units, and a positive cycle budget no greater than 16,777,216 guest cycles.
The cycle budget bounds total work in this call; the unit goal counts complete
core execution units and may include pipeline refill. A unit goal reached
within budget returns `GBL_V1_OK` with `GBL_V1_ADVANCE_COMPLETED`. A modeled
guest terminal or instruction-support stop also returns `GBL_V1_OK` and reports
`GBL_V1_ADVANCE_GUEST_TERMINAL` with its stop reason. A budget exhausted before
the unit goal refuses the whole call, even if the staged machine made partial
progress.

The adapter runs on a separate bounded machine candidate. It publishes the
candidate, result, and optional output bytes only after the core confirms a
valid result within the requested cycle ceiling. Every non-OK result leaves
machine state, result storage, and caller media bytes unchanged. The result
contains requested units and budget, start and end cycles, elapsed cycles,
retired instructions, outcome, stop reason, audio count, and the same current
observation returned by `gbl_v1_observe`.

## Observation and model limits

`gbl_v1_observe` reports profile ID/version, BIOS and cartridge selectors,
loaded state, mailbox verdict and case ID, input state, machine time, retired
instructions, video validity and epoch, output formats, capabilities, and
limits. The current qualification value is always
`GBL_V1_QUALIFICATION_UNKNOWN`.

The limit flags identify the current model boundaries: disputed timing
choices, unmodeled CPU prefetch, and raw audio captured before a PWM output
filter. These are project model limits, not hardware qualification. Profile
version, API symbol version, and software package version are separate values.

Mailbox states are `ABSENT`, `PENDING`, `PASS`, `FAIL`, and `MALFORMED`. A guest
`FAIL` is still a successful API observation; hosts decide how to interpret the
guest's case-specific result.

## Raw audio

Audio capture is opt-in. Set `capture_audio` to one and provide a caller-owned
byte span. Captured samples lie on the canonical `(start_cycle, end_cycle]`
interval at 512 guest cycles per stereo frame. The format is signed 16-bit
little-endian PCM, two channels, 32,768 frames per second. These are raw
pre-PWM model samples, not device-ready or hardware-qualified audio.

The API stages audio in bounded instance storage and checks the capacity
against the actual staged output. A guest stop before the unit goal can succeed
with capacity for the samples actually produced. If the capacity is short, the
call returns `GBL_V1_INSUFFICIENT_CAPACITY`, with machine, result, and caller
buffer unchanged. A zero-capacity span is valid when the call produces no
samples.

## Raw video

`gbl_v1_read_video` copies the current complete, supported 240 by 160 frame as
76,800 bytes of BGR555 little-endian pixels. It requires all 160 rows to be
valid in the current frame epoch. It does not expose an incomplete frame or
reuse old bytes as a current frame. Invalid or incomplete output returns
`GBL_V1_OUTPUT_UNAVAILABLE`; a span shorter than the complete frame returns
`GBL_V1_INSUFFICIENT_CAPACITY`. Both refusals leave the caller's bytes
unchanged. A successful copy writes exactly one frame and preserves any tail.

## Status values

| Status | Meaning |
|---|---|
| `GBL_V1_OK` | Operation completed; inspect result outcome and guest verdict separately. |
| `GBL_V1_INVALID_ARGUMENT` | Invalid descriptor, selector field, span, ordering, or pointer relationship. |
| `GBL_V1_UNSUPPORTED` | Unsupported profile, request, cycle limit, or timeline count. |
| `GBL_V1_INVALID_LIFECYCLE` | Operation requires mounted ROM content. |
| `GBL_V1_CONTENT_REFUSED` | ROM selection or length was refused. |
| `GBL_V1_ALLOCATION_FAILED` | Caller allocation callback could not provide storage. |
| `GBL_V1_INSUFFICIENT_CAPACITY` | A complete staged output does not fit the caller span. |
| `GBL_V1_WORK_EXHAUSTED` | Cycle budget ended before the requested complete unit goal. |
| `GBL_V1_OUTPUT_UNAVAILABLE` | No valid complete output exists for the requested capture. |
| `GBL_V1_INTERNAL_REFUSAL` | The canonical core rejected an internal staged operation. |

The v1 interface does not provide BIOS bytes, persistent save management,
cartridge peripherals, host input mapping, or a display/audio device backend.
Claims about game compatibility or hardware accuracy require separate evidence
for the exact machine, BIOS mode, cartridge, peripherals, and test scope.
