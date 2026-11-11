# The `agb` profile

`profiles/agb.json` is the authority for this profile. This file is the narrative
that explains it; a check compares the two, so a change to the unmodeled list or
the version must be made in both files together.

This canonical profile document uses plain Markdown only. The synchronization
check ignores fenced and indented examples, inline code spans (including spans
that cross line breaks), and HTML comments, and requires its version line,
section headings, and pipe tables to be visible.
Raw HTML elements, declarations, and processing instructions are unsupported
anywhere outside those ignored examples and comments; use Markdown code spans or
fenced code examples when showing HTML text.
Synchronized tables require both outer pipes on every row, the documented
headers and separators, and a blank line or heading after the table (or end of
document). Every Unmodeled ID must occur exactly once.

profile_version: 4

Every timing constant carries exactly one source label: REF (a reference
document), SRC (a secondary or community source), IMPL (another implementation),
INF (inferred by arithmetic), or UNK (unknown or disputed). A constant whose value
is not settled carries UNK and a dispute id from `registry/disputes.json`, never a
number.

## Machine

The profile id and machine are both `agb`: the original Game Boy Advance. There is
no other profile id in v1.0.

## Boot

`direct-start`: execution begins at the cartridge entry point with no boot ROM
running. No result may claim boot-ROM behavior.

## BIOS provider

The v1.0 default is `none`. The accepted provider forms are:

- `none`
- `project-stub`
- `hle:<name>` followed by more comma-separated names
- `replacement:<name>` joined by an at-sign to a 40-character lowercase hex commit
- `user-official` joined by an at-sign to a 64-character lowercase hex SHA-256

Names inside an `hle:` list are not checked against a table of BIOS calls.
Older milestone labels for BIOS options are not accepted.

## Cartridge

The cart kind is `rom-only`. Backup storage is listed as unmodeled below.

## Timing

All values are cycles of the 16,777,216 Hz master clock unless the name says dots
or lines. Section names are cited from GBATEK, never quoted.

| Constant | Value | Label | Cited section |
| -------- | ----- | ----- | ------------- |
| master_clock_hz | 16777216 | REF | LCD Dimensions and Timings: System Clock |
| cycles_per_dot | 4 | REF | LCD Dimensions and Timings: Horizontal Dimensions |
| dots_visible | 240 | REF | LCD Dimensions and Timings: Horizontal Dimensions |
| dots_hblank | 68 | REF | LCD Dimensions and Timings: Horizontal Dimensions |
| dots_per_line | 308 | REF | LCD Dimensions and Timings: Horizontal Dimensions |
| cycles_per_line | 1232 | REF | LCD Dimensions and Timings: Horizontal Dimensions |
| lines_visible | 160 | REF | LCD Dimensions and Timings: Vertical Dimensions |
| lines_vblank | 68 | REF | LCD Dimensions and Timings: Vertical Dimensions |
| lines_per_frame | 228 | REF | LCD Dimensions and Timings: Vertical Dimensions |
| cycles_per_frame | 280896 | REF | LCD Dimensions and Timings: Vertical Dimensions |
| hblank_flag_edge_cycle | none (null) | UNK | disputed, see disp-hblank-flag-edge |

The check recomputes the relations between these constants: visible plus HBlank
dots give the dots per line, dots per line times cycles per dot give the cycles
per line, visible plus VBlank lines give the lines per frame, cycles per line times
lines per frame give the cycles per frame, and the clock is 2 to the power 24.

The printed refresh rate is a derived figure: 16,777,216 divided by 280,896 is
about 59.7275 Hz (INF). Rates printed in other sources are not used here.

The cycle at which the HBlank flag changes is not recorded. Sources disagree
(1,006, 1,007, and 1,008 cycles appear), and 960 cycles is the visible drawing
time, not the flag edge. The constant stays null with label UNK until the dispute
is settled by evidence.

## Bus policy

The following one-cycle request costs are Gamebloke project policy. They are
not measured hardware timing, do not model WAITCNT or prefetch, and do not
establish hardware accuracy or broad compatibility.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| bus_policy.request_cycles.bios | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.ewram | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.iwram | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.io | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.pram | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.vram | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.oam | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.widths_bytes | [1, 2, 4] | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.access_classes | [nonsequential, sequential] | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |
| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |

## Initialization policy

Mutable machine memory, non-CPU registers, the master clock and the open-bus latch
are initialized to zero for deterministic project behavior. Hardware power-on RAM
contents remain unknown. CPU direct-start state is specified separately below.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| initialization_policy.memory_fill | 0 | IMPL | Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown. |
| initialization_policy.register_fill | 0 | IMPL | Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown. |
| initialization_policy.master_cycles | 0 | IMPL | Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown. |
| initialization_policy.open_bus | 0 | IMPL | Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown. |

## Read policy

The choices below are unverified Gamebloke project behavior for the current
minimal bus. BIOS protection, active I/O devices, WAITCNT, and prefetch remain
unmodeled. The actual behavior of reads beyond a short ROM image remains
unverified; the current-latch response is only a placeholder and says nothing
about the physical cartridge bus.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| read_policy.absent_bios | current-open-bus | IMPL | Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails. |
| read_policy.unsupported_io | current-open-bus | IMPL | Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails. |
| read_policy.rom_tail | current-open-bus | IMPL | Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails. |
| read_policy.latch_update | mapped-read-width-replicated | IMPL | Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails. |
| read_policy.write_latch | preserve | IMPL | Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails. |

## Header diagnostics

Profile version 4 retains the configured logo-window comparison using the
owner-authorized SHA-256 row in `tests/signatures/logo.sha256`. The loader hashes
only the copied candidate window `[0x04, 0xA0)` and reports checked match or checked
mismatch. A mismatch, missing-window convention, checksum finding or unusual
game code remains advisory under `direct-start`; it never refuses a size-valid
image. A pending-authority build reports unchecked with an owner-pending reason.

Two owner-provided local files yielded the same configured digest. This proves
identity of those windows and supports the configured comparison result. It
does not establish original-cartridge provenance, redistribution rights, BIOS
boot behavior, game execution, or broad compatibility. The ROM files are local
ignored inputs and are not public fixtures or CI inputs. Timing, fill, latch and
ROM-tail policies remain unchanged and unverified as hardware behavior.

## CPU direct-start policy

The following startup and cycle values are deterministic Gamebloke policy. They
do not describe a BIOS handoff or measured hardware power-on state. ARM PC offsets,
mode banking, and the absence of an architectural User/System SPSR are sourced
from the ARM7TDMI technical reference manual. CPU fetches use the existing bus;
region/state-specific BIOS open-bus behavior and Game Pak prefetch are not qualified.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| cpu_policy.entry_address | 134217728 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.cpsr | 223 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.register_bank_fill | 0 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.spsr_fill | 0 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.user_system_spsr | absent | REF | ARM7TDMI Technical Reference Manual (DDI 0029G, Issue G), ARM state, Thumb state, and processor modes sections. |
| cpu_policy.register_banks | User/System share r8-r14, FIQ has private r8-r14, IRQ/SVC/ABT/UND each have private r13-r14, and FIQ/IRQ/SVC/ABT/UND have SPSRs. | REF | ARM7TDMI Technical Reference Manual (DDI 0029G, Issue G), ARM state, Thumb state, and processor modes sections. |
| cpu_policy.pipeline | empty | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.arm_pc_offset | 8 | REF | ARM7TDMI Technical Reference Manual (DDI 0029G, Issue G), ARM state, Thumb state, and processor modes sections. |
| cpu_policy.thumb_pc_offset | 4 | REF | ARM7TDMI Technical Reference Manual (DDI 0029G, Issue G), ARM state, Thumb state, and processor modes sections. |
| cpu_policy.timing | unit-preflight-one-cycle-bus | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.exception_entry_cycles | 1 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.bios_none_stop | after-entry-before-vector-fetch | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.execution | single-interpreter | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |

## CPU instruction scope

This is the implemented Phase 4 slice, not full ARM7TDMI or broad GBA
compatibility. Unsupported valid instructions stop explicitly; architecturally
undefined encodings retain their separate classification.

| Scope | Opcode / operand forms | Label | Citation |
| ----- | --------------------- | ----- | -------- |
| cpu_scope.supported.1 | ARM data processing: MOV, ADD, SUB, CMP, AND, ORR, rotated immediate or register operand, immediate or register-specified LSL/LSR/ASR/ROR, S flag supported, destinations r0-r14 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.2 | ARM control flow: B, BL, BX | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.3 | ARM single transfer: LDR/STR byte or word, immediate 12-bit offset, pre-indexed, no writeback, U plus or minus, r0-r14 base/data, PC base only for positive LDR literal | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.4 | Thumb data processing: MOV immediate8, ADD/SUB immediate8, CMP immediate8 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.5 | Thumb high-register operations: MOV to r0-r14 and BX | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.6 | Thumb control flow: conditional B except SWI/reserved conditions, and unconditional B | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.7 | Thumb transfer: PC-relative word LDR at aligned(current+4), plus immediate word/byte LDR/STR using low registers | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.1 | ARM register-specified shifts involving r15, r15 destinations other than branch/BX, STR with PC source, and PC reads | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.2 | ARM post-indexed or writeback transfers, halfword/signed transfers, block transfers, multiply, swap, full PSR transfers, and coprocessor execution | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.3 | Thumb data-processing writes to r15 outside BX/branch, Thumb BL pair, and non-listed transfer forms | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.4 | All other valid-but-unimplemented opcodes and operands stop as unsupported, reserved undefined encodings remain distinct | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.5 | Later ARM architecture extensions and every instruction family absent from the supported array | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |

No BIOS stack convention is initialized. All general and banked registers plus
valid SPSR storage start at zero by project policy, User/System has no SPSR, and
the pipeline starts empty. The ARM entry is `0x08000000` with CPSR `0x000000DF`
(ARM state, System mode, clear condition flags, IRQ/FIQ masked). Architectural
ARM and Thumb operand-visible PC offsets are +8 and +4 respectively. The CPU
uses one interpreter path with whole-unit preflight and the declared one-cycle
bus model; exception entry charges one project internal cycle. With no BIOS,
exception bookkeeping completes and the named stop occurs before any vector fetch.

## SP and AGS naming

The only profile id is `agb`. The SP is `agb` plus a host presentation preset, and
that preset is claimed only with evidence; no `agb-micro` or SP profile is offered
until owner hardware exists.

- The SP's backlight statement is REF (GBATEK, section GBA SP Notes).
- The model labels AGS-001 (front-lit) and AGS-101 (backlit), and which label maps
  to which display, come from secondary sources and are labeled SRC at MEDIUM
  confidence, not REF. The front-lit versus backlit difference is a display
  difference, not a machine difference.
- GBATEK hedges that the SP seems to behave the same as the original and that software cannot tell them apart; that is a community claim and is labeled UNK, so hardware compatibility is undetermined here.
- The labels AGB-001 (original) and OXY-001 (Micro) are community labels (SRC at
  MEDIUM confidence).

This profile does not state that the SP is identical to the AGB.

## Unmodeled

Each entry below is listed in `profiles/agb.json` with status `unmodeled`. A result
must not claim the behavior of anything in this table.

| Id | What is not modeled |
| -- | ------------------- |
| `sp-micro-differences` | SP and Micro differences. Includes the EWRAM wait register at 0x04000800 (its fastest setting is reported to lock up the Micro, REF) and the Micro's missing cartridge switch (REF). |
| `prefetch` | The cartridge prefetch buffer and cartridge wait-state timing. |
| `hblank-flag-edge` | The cycle at which the HBlank flag changes; disputed, see disp-hblank-flag-edge. |
| `ppu` | Picture processing. |
| `audio` | Sound. |
| `dma` | DMA transfers and their timing. |
| `timers` | Hardware timers. |
| `irq-controller` | The interrupt controller. |
| `backup-media` | Cartridge backup storage (save memory). |
| `bios-protection` | CPU fetches participate in BIOS protection checks, but region/state-specific open-bus behavior and Game Pak prefetch are not qualified or emulated. |
| `io-devices` | I/O device register semantics. |
