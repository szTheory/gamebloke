# The `agb` profile

`profiles/agb.json` is the authority for this profile. This file is the narrative
that explains it; a check compares the two, so a change to the unmodeled list or
the version must be made in both files together.

This canonical profile document uses plain Markdown only. The synchronization
check ignores fenced and indented examples and HTML comments, and requires its
version line, section headings, and pipe tables to be visible. Raw HTML blocks
are unsupported anywhere in this file; use fenced code examples when showing
HTML text.

profile_version: 2

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

Mutable machine memory, registers, the master clock and the open-bus latch are
initialized to zero for deterministic project behavior. Hardware power-on RAM
contents remain unknown, and this policy does not set Phase 4 CPU direct-start
registers.

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
| `bios-protection` | BIOS read protection and CPU pipeline-dependent behavior. |
| `io-devices` | I/O device register semantics. |
