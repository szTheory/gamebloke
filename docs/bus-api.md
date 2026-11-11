# Internal machine and bus contract

This guide documents the Phase 3 internal C contract used by the CPU work in
Phase 4. The structs and functions are not a stable embedding ABI. The bus
implements the profile's memory windows and observable project policies; it
does not implement CPU execution or all GBA devices.

## Ownership and lifecycle

The host owns the `gbl_machine` storage and must provide fresh storage to
`gbl_machine_init`. Reinitializing live storage is a host contract violation.
The optional allocator descriptor must be a separate object outside the machine
storage; the core rejects a descriptor whose start address is inside the
machine object. Its callbacks must either both be present or both be absent.
Supplying no descriptor, or a descriptor with both callbacks absent,
supports bus-only use without a default allocator. Invalid initialization
arguments return `GBL_BUS_STATUS_INVALID_ARGUMENT` before changing machine
storage.

Successful initialization explicitly zeroes the master clock, open-bus latch,
`DISPCNT` storage, and every mutable RAM array; it starts initialized with no
ROM and zero ROM length. This zero-fill is a deterministic project policy,
not a claim about GBA power-on RAM contents. No CPU register bank is included
yet.

`gbl_machine_reset` is a no-op for a null or uninitialized instance. On an
initialized instance it returns canonical machine state and every RAM array to
zero, keeps the instance initialized, and preserves the installed ROM
allocation and allocator callbacks.
`gbl_machine_dispose` detaches the ROM and allocator, clears canonical mutable
state, marks the instance uninitialized, then releases the detached ROM if one
was installed. Disposing an initialized empty machine is safe to repeat. The
release callback used during disposal must not reenter or mutate the disposed
instance. On successful ROM replacement, the old-ROM release callback may
inspect the newly committed, cold-reset instance read-only, but must not reenter
or mutate it. Same-instance concurrency and reentrancy remain host contract
violations.

Distinct machine instances have independent backing arrays and clocks. The
host is responsible for synchronizing all access to a single instance.

## Bus requests and results

`gbl_bus_read`, `gbl_bus_write`, and `gbl_bus_internal` are the only ordinary
access and internal-cycle entry points. Read and write widths are byte counts
of 1, 2, or 4. Reads and writes preserve the caller's sequential or
non-sequential class in the result. Each successful request costs one master
cycle regardless of width, region, or class. This is a provisional project
policy; it does not model WAITCNT or prefetch timing and is not a hardware
accuracy claim. Internal-cycle advancement costs exactly the supplied count
and reports non-sequential class.

The result fields are:

| Field | Meaning |
|---|---|
| `status` | `OK`, `INVALID_ARGUMENT`, or `TIME_OVERFLOW` |
| `outcome` | `NONE`, `RAM`, `BIOS_ABSENT`, `IO_UNIMPLEMENTED`, `IO_MODE_SUBSET`, `ROM`, `ROM_TAIL`, `UNMAPPED`, or `VIDEO_INVALID_MODE_POLICY` |
| `value` | Read value; zero for writes and refusals |
| `cycles` | Cost charged to the machine clock; zero for refusals |
| `access_class` | Requested class for a valid read/write, or non-sequential for internal cycles |

Invalid or uninitialized instances, invalid widths/classes, and requested
address spans that exceed `UINT32_MAX` return `INVALID_ARGUMENT`, zero value,
and zero cycles before changing machine state. A clock addition that would
exceed `UINT64_MAX` returns `TIME_OVERFLOW` with zero value/cycles and no
memory, latch, or clock change. A zero-cycle internal request is valid even
when the clock is `UINT64_MAX`; it leaves the full machine state unchanged.

## Memory windows and access policy

The table distinguishes the address map described in the community
[GBATEK 3.06 reference](https://mgba-emu.github.io/gbatek/) from Gamebloke's
selected behavior. GBATEK text is reference evidence, not Nintendo
documentation or physical hardware qualification.

| Window | Backing and translation | Raw read/write behavior | Evidence and limit |
|---|---|---|---|
| `0x00000000–0x00003FFF` BIOS | No BIOS provider is installed. | Reads return the saved latch and report `BIOS_ABSENT`; writes are ignored. | GBATEK's “GBA Memory Map” and “Memory Mirrors” describe the window and say BIOS is not mirrored. Current-latch behavior is an unverified project placeholder, not BIOS protection. |
| `0x02000000–0x02FFFFFF` EWRAM | 256 KiB backing, `address & 0x3FFFF`. | Raw 8/16/32-bit reads and writes. | GBATEK's “GBA Memory Map” and “Memory Mirrors”; one-cycle cost is Gamebloke project policy. |
| `0x03000000–0x03FFFFFF` IWRAM | 32 KiB backing, `address & 0x7FFF`. | Raw 8/16/32-bit reads and writes. | GBATEK's “GBA Memory Map” and “Memory Mirrors”; one-cycle cost is Gamebloke project policy. |
| `0x04000000–0x040003FF` I/O | The byte-address window is decoded. Only DISPCNT bytes `0x04000000–0x04000001` are modeled; all other registers are unsupported. | DISPCNT exposes only bits 0–2 (BG mode) and reports `IO_MODE_SUBSET`. Unsupported reads return the saved latch and report `IO_UNIMPLEMENTED`; unsupported writes are ignored. | GBATEK's “GBA I/O Map” and “LCD I/O Display Control”. This is a mode selector for bus classification, not a PPU or full DISPCNT implementation. |
| `0x04000800–0x04FFFFFF` port aliases | The special port is recognized at each 64 KiB alias whose low 16 bits are `0x0800–0x0803`. | Reads use the current-latch placeholder and report `IO_UNIMPLEMENTED`; writes have no effect. | GBATEK's “Memory Mirrors” calls this port undocumented and says it repeats every 64 KiB. Gamebloke does not model its internal-memory-control effect. |
| `0x05000000–0x05FFFFFF` palette RAM | 1 KiB backing, `address & 0x3FF`. | Reads and 16/32-bit writes are raw. A byte write duplicates its value to both bytes of the containing aligned halfword. | GBATEK's “GBA Memory Map”, “Memory Mirrors”, and “Writing 8bit Data to Video Memory”. |
| `0x06000000–0x06FFFFFF` VRAM | 128 KiB address period: `address & 0x1FFFF`, then offsets `0x18000–0x1FFFF` subtract `0x8000` into the preceding physical 32 KiB. | Reads and 16/32-bit writes use raw little-endian lanes. In modes 0–2 byte writes duplicate below physical `0x10000` and are ignored at/above it. In modes 3–5 the threshold is `0x14000`. Modes 6–7 use the same `0x14000` threshold as explicit project policy; byte writes report `VIDEO_INVALID_MODE_POLICY`. | GBATEK's “Memory Mirrors”, “LCD VRAM Overview”, “LCD I/O Display Control”, and “Writing 8bit Data to Video Memory” support the mirror and valid-mode threshold cases. Modes 6–7 are not claimed as hardware behavior. |
| `0x07000000–0x07FFFFFF` OAM | 1 KiB backing, `address & 0x3FF`. | Reads and 16/32-bit writes are raw. Byte writes are ignored. | GBATEK's “GBA Memory Map”, “Memory Mirrors”, and “Writing 8bit Data to Video Memory”. |
| `0x08000000–0x0DFFFFFF` Game Pak ROM | Each 32 MiB window (`0x08000000`, `0x0A000000`, `0x0C000000`) uses `address & 0x1FFFFFF` into the host-owned copy. | Reads within the loaded image return copied bytes. An offset beyond its actual length returns the corresponding byte of the saved latch, reports `ROM_TAIL`, and does not replace the latch. Writes are ignored and report `ROM`. | GBATEK's “GBA Memory Map” identifies the three wait-state windows. Short-image tail behavior is explicitly unverified; the current-latch result is a project policy, not inferred from that reference. |
| Other addresses, including `0x04000400–0x04FFFFFF` except the port aliases, and `0x0E000000–0x0FFFFFFF` | Unmapped in this profile; backup memory has no device implementation. Full 32-bit address identity is preserved. | Reads return the saved latch and report `UNMAPPED`; writes are ignored. | GBATEK's “GBA Memory Map” documents SRAM in part of `0x0E000000–0x0FFFFFFF`; Gamebloke leaves that area unmapped until a declared save device is implemented. The current-latch result is a project placeholder, not a hardware claim. |

The per-request access contract is independent of that map:

- Width is a byte count of 1, 2, or 4. Each lane decodes its own consecutive
  32-bit address and mirror translation. Values use little-endian lane order;
  the bus does not align or rotate an unaligned CPU load. CPU-specific
  misalignment fixups belong in the CPU adapter.
- The complete address span and one-cycle clock addition are checked before
  any lane changes memory, DISPCNT, the latch, or the clock. Every successful
  read or write costs one cycle regardless of width, region, or class. This is
  a project placeholder, not hardware timing.
- A fallback read lane takes the corresponding byte from the saved latch as it
  existed before the request. If any lane falls back, the read leaves the
  latch unchanged. If every lane has concrete data, the result value updates
  the latch by byte/halfword replication or as a full word. Writes never
  replace the latch.
- When a request spans several regions, its result outcome uses this fallback
  priority: `IO_UNIMPLEMENTED`, `BIOS_ABSENT`, `ROM_TAIL`, then `UNMAPPED`.
  Without a fallback lane, the result reports the starting lane's region or
  policy outcome. Supported memory lanes in a mixed write still take effect;
  unsupported lanes are ignored. Cost remains one cycle for the request.

## Deferred CPU and device behavior

The bus contract does not imply the following behavior:

- CPU instruction execution, pipeline state, PC-relative reads, and ARM
  misaligned load rotation.
- WAITCNT region-specific sequential/non-sequential timing, Game Pak
  prefetch, and CPU pipeline-dependent open-bus values.
- BIOS protection and access rules beyond the absent-provider placeholder.
- I/O behavior other than the DISPCNT mode bits: display-control bits other
  than mode, PPU registers and rendering, audio, timers, DMA, interrupts,
  keypad input, and other peripherals.
- PPU display-period access restrictions, bus contention, and scanline timing.
- The undocumented `0x04000800` port's internal-memory-control behavior.
- Game Pak SRAM, flash/EEPROM, save devices, and other cartridge peripherals.

No BIOS bytes, commercial ROMs, or other copyrighted game content are bundled.
The synthetic test images are generated in memory and are not redistributed
fixtures.

## ROM loading and host allocation

`gbl_load_rom` copies a caller-provided image into storage returned by the
machine's host allocator. It never keeps the caller pointer. The input must be
a readable span for the declared size and remain immutable for the duration of
the call. ROM file selection and file reads stay in a host adapter; the core
does not open files or use a default heap.

Validation returns a `gbl_load_result` by value. The result contains a status
and reason; there is no caller-owned diagnostic buffer for a refusal to mutate.
An uninitialized or null machine is `INVALID_ARGUMENT/ARGUMENT`. For an
initialized machine, size checks run before checking the image pointer or
allocator callbacks, in this order:

| Condition | Result |
|---|---|
| Length is zero | `REFUSED/ROM_ZERO` |
| Length is below `0xC0` | `REFUSED/ROM_SHORT` |
| Length exceeds `0x02000000` (32 MiB) | `REFUSED/ROM_TOO_LARGE` |
| Length is odd | `REFUSED/ROM_ODD` |
| Valid length with null image or missing/incomplete callback pair | `INVALID_ARGUMENT/ARGUMENT` |
| Host allocation returns null | `RESOURCE_FAILURE/ALLOCATION` |

This order gives short length precedence over odd length, and the 32 MiB
maximum precedence over odd length. These are deterministic API rules, not
hardware-derived cartridge behavior.

The host allocator is a trusted boundary with these obligations:

- `allocate(context, length)` receives the exact validated length and returns
  suitably aligned writable storage for that many bytes, or null. A candidate
  must be fresh and disjoint from machine storage, the input span, and the
  currently installed ROM.
- `release(context, pointer, length)` receives the exact pointer and length
  from that allocator context, cannot fail, and must not retain access after
  it returns. It must not reenter or mutate the machine.
- Same-instance calls are ordered by the host. Callbacks must not use the
  machine to start another call while the current load is in progress.

After validation, the core allocates and copies into a candidate before
changing machine state. It installs the candidate pointer and length, cold
resets all canonical mutable fields and arrays, then calls release for the
detached prior ROM. A replacement release callback can observe the new ROM and
zeroed machine state read-only. No failure-prone core operation follows commit.
Allocation failure and every refusal preserve the prior machine state and
caller input. Successful reset preserves the installed ROM; a fresh loaded
machine and a repeatedly reset loaded machine have equal canonical fields and
ROM contents. Disposal clears and detaches the ROM before releasing it, and a
second disposal does not release again.

`gbl_rom_data` exposes a const pointer to the installed owned bytes, and
`gbl_rom_size` returns their byte length. Both are read-only accessors and
return null/zero for a null or uninitialized machine.

## Consumer integration witness

The `loader.integration` selector in the in-tree C test runs the complete
loaded-machine path through `gamebloke_core`. It uses separate host allocator
contexts and separate synthesized input buffers for equal ROM images, then
checks that the core owns a copy and that later caller edits do not affect it.
The test reads each of the three Game Pak ROM aliases, writes and reads mirrored
EWRAM and IWRAM, changes the display mode around a VRAM byte-write boundary,
and advances explicit internal cycles. Every request checks its status,
outcome, value, access class, reported cost, and resulting clock.

One machine runs the bus sequence alone while two others run the same sequence
interleaved; their canonical fields, memory arrays, and ROM contents must agree
exactly. The selector also runs the lifecycle cases for refusal and allocation
failure atomicity, candidate commit before prior-ROM release, source-buffer
edits, repeated loaded reset, and independent disposal. Reset must preserve the
owned ROM and restore the same canonical machine fields as a fresh load.
Header diagnostics remain advisory. With the configured authority, these
synthetic images report checked logo mismatch; pending-authority builds report
`UNCHECKED`/`OWNER_PENDING`.

This is an in-tree integration witness over synthetic bytes. It does not claim
CPU or guest execution, a stable embedding ABI, an installed-consumer route,
physical hardware accuracy, or broad game compatibility.

## Header diagnostics

`gbl_rom_diagnostics` returns a by-value `gbl_header_diagnostics`. Its
`loaded` flag is false in the zero/not-loaded lifecycle state and true only
after a successful ROM load. The remaining fields are bounded enums:

| Field | Values and meaning |
|---|---|
| `canonical_logo` | `CHECKED_MATCH` or `CHECKED_MISMATCH` against the configured owner-authorized digest; `UNCHECKED` when no authority is configured or no image is loaded. |
| `logo_reason` | `CHECKED` when the configured comparison ran; otherwise the zero-state `OWNER_PENDING` value. |
| `logo_window` | `ZERO` when bytes `[0x04, 0xA0)` are all zero, otherwise `NONZERO`. Zero is a project convention for a missing window, not a canonical logo result. |
| `checksum` | `NOT_LOADED`, `OK`, or `MISMATCH`. For a loaded image, subtract bytes `0xA0` through `0xBC` and `0x19` from zero modulo 256, then compare with byte `0xBD`. |
| `game_code` | `NOT_LOADED`, `ORDINARY`, or `UNUSUAL`. The four bytes at `[0xAC, 0xB0)` are ordinary only when each is ASCII `A`–`Z` or `0`–`9`; their contents are not retained. |

These findings use fixed offsets inside the minimum `0xC0`-byte header and are
advisory under `direct-start`; an otherwise size-valid image still loads.
Diagnostics are parsed from the copied candidate, committed only with a
successful load, preserved across reset, and cleared on disposal. Refused and
allocation-failed loads preserve prior diagnostics and caller bytes. The
offsets and checksum convention follow GBATEK 3.06, “GBA Cartridge Header,” a
community reference rather than Nintendo documentation or hardware
qualification.

`gbl_header_compare_bytes` is a generic span-comparison helper for valid,
caller-owned spans: it returns `INVALID_ARGUMENT` for a null span or zero
length, `EQUAL` when all bytes match, and `DIFFERENT` on the first mismatch.
Its tests use synthetic spans to prove comparison mechanics. The actual loader
separately computes SHA-256 of the copied 156-byte window and compares it with
the configured digest from `tests/signatures/logo.sha256`. Profile version 3
activates the owner-authorized row; a zero window reports a checked mismatch
against it. Digest mismatch never refuses an otherwise size-valid load.

A separate synthetic-core target still exercises match/mismatch with owned
patterns. Local owner-authorized header checks establish the configured result
for those files; no game or logo bytes are embedded in public tests. This work
does not establish redistribution permission, hardware accuracy, BIOS boot
behavior, game execution or broad game compatibility.
