# Internal machine and bus contract

This guide documents the Phase 3 internal C contract used by the CPU work in
Phase 4. The structs and functions are not a stable embedding ABI. The current
implementation provides deterministic machine lifecycle and EWRAM access; it
does not yet implement the complete GBA memory map or CPU execution.

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
| `outcome` | `NONE`, `RAM`, `BIOS_ABSENT`, `IO_UNIMPLEMENTED`, `ROM`, `ROM_TAIL`, or `UNMAPPED` |
| `value` | Read value; zero for writes and refusals |
| `cycles` | Cost charged to the machine clock; zero for refusals |
| `access_class` | Requested class for a valid read/write, or non-sequential for internal cycles |

Invalid or uninitialized instances, invalid widths/classes, and requested
address spans that exceed `UINT32_MAX` return `INVALID_ARGUMENT`, zero value,
and zero cycles before changing machine state. A clock addition that would
exceed `UINT64_MAX` returns `TIME_OVERFLOW` with zero value/cycles and no
memory, latch, or clock change. A zero-cycle internal request is valid even
when the clock is `UINT64_MAX`; it leaves the full machine state unchanged.

## Implemented memory behavior

The current bus maps the EWRAM window `0x02000000–0x02FFFFFF` to its 256 KiB
backing array, mirrored every `0x40000` bytes. Raw transfers use consecutive
addresses and little-endian byte assembly/disassembly. The bus does not align
or rotate misaligned CPU words. A request that extends beyond this implemented
window currently takes the unmapped path as a whole; later full-map decoding
will dispatch individual lanes and charge once per request.

An EWRAM read updates the explicit open-bus latch by replicating an 8-bit read
four times, replicating a 16-bit read twice, or storing the full 32-bit read.
Writes leave the latch unchanged. An unmapped read returns the current latch
masked to the requested width and preserves the latch; unmapped writes are
ignored and leave the latch unchanged. These latch rules are project policy
for this stage. They do not claim to reproduce opcode-prefetch-dependent
hardware behavior.

Other outcome values reserve room for the planned BIOS, I/O, and cartridge
decode paths. Their presence does not mean those regions are implemented yet.
The full map, video access rules, and ROM-tail policy are handled by later
Phase 3 plans. No BIOS bytes or game ROMs are bundled.

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
return null/zero for a null or uninitialized machine. Header diagnostics are
not implemented in this loader slice; later header findings remain advisory
under `direct-start`, and the owner-pending canonical logo digest stays a
separate gate. This work does not establish hardware accuracy or broad game
compatibility.
