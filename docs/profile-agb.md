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

profile_version: 14

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

The hardware cycle at which the HBlank flag changes remains unrecorded: sources
disagree (1,006, 1,007, and 1,008 cycles appear), and 960 cycles is nominal
visible-dot time, not the flag edge. The profile keeps
`timing.hblank_flag_edge_cycle.value` null with label UNK. The separate
project-selected model event is at 1,006 cycles, remains disputed under
`disp-hblank-flag-edge`, and has hardware qualification UNKNOWN.

## PPU behavior policy

The source-backed geometry and selected Gamebloke model behavior are recorded
separately. These values describe the supported deterministic subset; they do
not qualify the PPU against hardware.

| Setting | Value |
| --- | --- |
| `ppu_policy.policy_classification` | project-selected |
| `ppu_policy.hardware_qualification` | UNKNOWN |
| `ppu_policy.hardware_source_reference` | Nintendo AGB Programming Manual v1.1 §§4.1.1–4.1.2, 5.1.1, 6.1.3–6.1.5, 6.1.7, 6.3.1–6.3.4 and 6.4, printed pp. 58–72 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| `ppu_policy.geometry` | 240x160 visible pixels, 308 dots per line, 228 lines per frame |
| `ppu_policy.line_cycles` | 1232 |
| `ppu_policy.nominal_visible_dot_cycles` | 960 |
| `ppu_policy.recurrence` | one recurring priority-9 event alternates line-start and HBlank boundaries |
| `ppu_policy.event_priority` | 9 |
| `ppu_policy.line_transitions` | VCOUNT advances 0-227 modulo 228, visible lines are 0-159, VBlank sets at 160 and clears at 0 |
| `ppu_policy.same_cycle_order` | due machine events precede timed bus effects, row sample precedes a bus write completing on the same cycle |
| `ppu_policy.dispcnt_writable_mask` | 0xFFFF |
| `ppu_policy.dispstat_writable_mask` | 0xFF38 |
| `ppu_policy.vcount_write_policy` | ignored |
| `ppu_policy.compare_write_policy` | recompute at write completion after due events |
| `ppu_policy.hblank_event_offset_cycles` | 1006 |
| `ppu_policy.hblank_source` | REF |
| `ppu_policy.hblank_source_reference` | GBATEK, LCD I/O Interrupts and Status, states the flag is 0 for 1,006 cycles, source text is not a project hardware measurement |
| `ppu_policy.hblank_provenance` | project-selected |
| `ppu_policy.hblank_dependent_classification` | disputed |
| `ppu_policy.hblank_dispute` | disp-hblank-flag-edge |
| `ppu_policy.hblank_hardware_qualification` | UNKNOWN |
| `ppu_policy.row_sample_point` | visible-line-start |
| `ppu_policy.row_same_cycle_write_policy` | PPU sample precedes a bus write completing on the sample cycle |
| `ppu_policy.pixel_format` | BGR555 little-endian |
| `ppu_policy.frame_validity` | complete coverage of 160 supported rows in one frame epoch, published at line 160 |
| `ppu_policy.supported_controls` | mode 3/4/5 BG2 bitmap sources (mode 3 direct-color 240x160 single page, mode 4 indexed 240x160 two DISPCNT-selected pages, mode 5 direct-color 160x128 two DISPCNT-selected pages), captured BG2 affine transform/reference progression, out-of-extent samples to backdrop, mode-4 index zero transparent and direct-color zero opaque black, non-BG2 background enable bits ignored in bitmap modes, any enabled supported backgrounds per sampled row in modes 0-2 (mode 0 text BG0-BG3, mode 1 text BG0/BG1 or affine BG2, mode 2 affine BG2/BG3), text scroll, four map dimensions, affine transforms and reference progression, 4bpp/8bpp tiles, flips, palette and transparent-index backdrop, regular and affine OBJ rendering, shared BG/OBJ/backdrop priority composition |
| `ppu_policy.unmodeled_controls` | forced blank, rectangular and object windows, semitransparent OBJ, OBJ-window mode, mosaic, color effects, prohibited OBJ encodings, and unsupported background-control encodings |
| `ppu_policy.interrupt_if_bits` | VBlank 0, HBlank 1, VCOUNT-match 2 |
| `ppu_policy.blank_dma_eligibility` | HBlank on visible lines 0-159, VBlank at the line 160 transition |

## PPU object policy

| Setting | Value |
| --- | --- |
| `ppu_policy.object_policy.regular_shapes` | square:8x8,16x16,32x32,64x64;horizontal:16x8,32x8,32x16,64x32;vertical:8x16,8x32,16x32,32x64 |
| `ppu_policy.object_policy.character_windows` | modes0-2:[0x10000,0x18000);modes3-5:[0x14000,0x18000);full-tile-span-before-read |
| `ppu_policy.object_policy.index_zero_transparency` | true |
| `ppu_policy.object_policy.affine_quantization` | signed-8.8;wide-products;floor-by-256 |
| `ppu_policy.object_policy.affine_field` | centered;double-size-destination-only;out-of-source-transparent |
| `ppu_policy.object_policy.coordinate_wrap` | x-modulo-512;y-modulo-256;clip-240x160 |
| `ppu_policy.object_policy.unsupported_modes` | prohibited;semitransparent;obj-window;mosaic |
| `ppu_policy.object_policy.work_budget_units` | 1226 |
| `ppu_policy.object_policy.regular_cost` | source-width |
| `ppu_policy.object_policy.affine_cost` | 10+2*destination-field-width |
| `ppu_policy.object_policy.eligibility` | enabled-supported-object;vertical-field-intersects-row;no-horizontal-or-transparency-refund |
| `ppu_policy.object_policy.admission_order` | ascending-oam;full-cost-fit;stop-at-first-nonfit |
| `ppu_policy.object_policy.overflow_policy` | valid-partial-scene;omit-all-later-eligible |
| `ppu_policy.object_policy.observation_scope` | internal-row-capture;selected-mask;selected-count;omitted-count;used-work;no-stable-c-api |

D-01 supports normal regular shapes and sizes, horizontal and vertical flips,
4bpp and 8bpp palettes, and 1D and 2D character mappings. D-02 confines tile
reads to the mode-dependent physical character window and validates a complete
tile span before any sampled-memory read. D-03 makes source index zero
transparent; palette-resolved black remains opaque. D-04 wraps OAM coordinates
then clips to 240x160 and refuses prohibited or Phase 14 OBJ controls
deterministically. The manual source for OBJ formats and addressing is cited
by section and page only; this profile does not reproduce its wording.
[Nintendo AGB Programming Manual v1.1 §§6.3.1–6.3.3, printed pp. 58–69](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)

D-05 uses signed 8.8 matrix values, wide integer products, and mathematical
floor division by 256 as Gamebloke policy. D-06 centers affine sampling in the
destination field; double-size expands only that field, and source coordinates
outside the object are transparent. D-07 is covered by semantic vectors for
shape and mapping selectors, matrix transforms, clipping, physical tile bounds,
palette transparency, opaque black, and wrong-pixel controls. These are
deterministic software rules, not hardware measurements.
[Nintendo AGB Programming Manual v1.1 §6.3.4, printed pp. 70–71](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)

D-11 sets a fixed budget of 1,226 abstract work units per visible row. It does
not alter master cycles, HBlank duration, or the event schedule. D-12 charges
regular objects by source width and affine objects by 10 plus twice their
destination-field width. Only enabled supported objects whose vertical field
intersects the row are eligible; horizontal clipping and transparent texels do
not refund work. D-13 scans eligible entries in ascending OAM order, admits
only a full-cost fit, stops at the first non-fit, and records selected, omitted,
and used-work values through an internal row-capture seam without defining a
stable public C API. D-14 overflow preserves valid partial-scene output and
omits all later eligible objects. The work budget and admission behavior are
project policy; silicon cost and exhaustion behavior remain unknown.
[Nintendo AGB Programming Manual v1.1 §6.3.1, printed pp. 58–59](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)

## PPU composition policy

| Setting | Value |
| --- | --- |
| `ppu_policy.composition_policy.priority_order` | ascending-numeric-0-3 |
| `ppu_policy.composition_policy.bg_tie` | lower-bg-number |
| `ppu_policy.composition_policy.obj_tie` | lower-oam-index |
| `ppu_policy.composition_policy.obj_bg_tie` | obj-in-front-project-policy |
| `ppu_policy.composition_policy.backdrop` | pram-color-zero;behind-all-enabled-opaque-candidates |

D-08 gives lower numeric priority precedence, then uses the lower BG number or
lower OAM index within the same class. D-09 selects OBJ in front of BG for an
equal-priority cross-class tie as deterministic Gamebloke policy. D-10 keeps
ascending-OAM admission separate from per-pixel priority composition. The
manual's reversed-OAM-order/priority OBJ-BG sandwich remains a semantic case;
its outcome does not establish a silicon comparator or hardware ordering.
[Nintendo AGB Programming Manual v1.1 §6.4, printed p. 72](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)

## PPU display capabilities

The mode table describes available background source layouts and their source
extents. Implementation status records the current Gamebloke renderer; `partial`
is a static capability label and is separate from per-frame validity. The mode
and background layouts are summarized from the cited AGB manual sections, not
reproduced from its tables. The mode families and text tile/map formats are
described in §§5.1.1 and 6.1.3–6.1.5. Bitmap formats and page layouts are
described in §§6.2.1–6.2.4, and palette behavior in §7.1. [Nintendo AGB
Programming Manual v1.1 §§5.1.1, 6.1.3–6.1.5, 6.2.1–6.2.4, and 7.1](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf)

Modes 0, 1, and 2 render any enabled supported background sources per sampled
row. Mode 0 admits text BG0–BG3; mode 1 admits text BG0/BG1 or affine BG2;
mode 2 admits affine BG2/BG3. The text subset uses the four listed map
extents, scrolling, tile flips, 4bpp/8bpp selection, tile and palette selection,
and transparent color-index samples resolved to the declared backdrop in raw
output. The affine subset uses the listed square map extents and sampled VRAM,
with signed 8.8 matrix coefficients and programmed 28-bit reference values.
Programmed references are sign-extended; coordinate quantization uses
mathematical floor division by 256 and horizontal calculations use `int64_t`
intermediates; internal reference updates wrap to signed 28-bit two's-complement
values. These arithmetic, rounding, overflow, and reference-update rules are
selected deterministic software policy, not measured hardware behavior. The
manual documents affine formats and registers (§6.1.7); GBATEK is secondary
evidence for internal-reference reload and accumulation details. Modes 3, 4,
and 5 admit BG2 direct-color 240x160, indexed 240x160, and direct-color 160x128
sources, respectively. Modes 4 and 5 have two pages selected by DISPCNT; mode 3
uses one page. The mode and page selection are captured with the sampled row
state. Bitmap reads are bounded to each declared source extent, and samples
outside it resolve to the backdrop. Mode-4 index zero resolves to the backdrop
under the secondary-source policy; mode-3 and mode-5 direct-color zero is opaque
black. The manual documents bitmap formats, extents, and pages in §§6.2.1–6.2.4.
Mode-4 index-zero transparency follows secondary-source evidence as selected
project policy. The explicit rounding/overflow behavior above is software policy,
not a hardware measurement.
Background, normal regular/affine OBJ, and backdrop samples use the same
captured row state and canonical sampled video memory. The shared compositor
preserves transparency and source identity until priority selection. Forced
blank, windows, effects, mosaic, and unsupported OBJ controls remain outside
the supported subset; enabling unsupported controls retains deterministic
frame invalidation behavior. All six display modes remain `partial`, and PPU
hardware qualification remains `UNKNOWN`.

| Mode | Implementation status | Background sources and dimensions |
| --- | --- | --- |
| `ppu_policy.display_capabilities.modes[0]` | partial | BG0 text: 256x256, 512x256, 256x512, 512x512; BG1 text: 256x256, 512x256, 256x512, 512x512; BG2 text: 256x256, 512x256, 256x512, 512x512; BG3 text: 256x256, 512x256, 256x512, 512x512 |
| `ppu_policy.display_capabilities.modes[1]` | partial | BG0 text: 256x256, 512x256, 256x512, 512x512; BG1 text: 256x256, 512x256, 256x512, 512x512; BG2 affine: 128x128, 256x256, 512x512, 1024x1024 |
| `ppu_policy.display_capabilities.modes[2]` | partial | BG2 affine: 128x128, 256x256, 512x512, 1024x1024; BG3 affine: 128x128, 256x256, 512x512, 1024x1024 |
| `ppu_policy.display_capabilities.modes[3]` | partial | BG2 direct-color-bitmap: 240x160 |
| `ppu_policy.display_capabilities.modes[4]` | partial | BG2 indexed-bitmap: 240x160 |
| `ppu_policy.display_capabilities.modes[5]` | partial | BG2 direct-color-bitmap: 160x128 |

## PPU raw output

The raw output keeps the existing 240x160 display geometry. Public Nintendo
technical information also identifies a 240x160 display; this citation and the
profile do not imply hardware qualification. [Nintendo Game Boy Advance
technical history](https://www.nintendo.com/en-gb/Hardware/Nintendo-History/Game-Boy-Advance/Game-Boy-Advance-627139.html)

| Setting | Value |
| --- | --- |
| `ppu_policy.display_capabilities.raw_output.format` | BGR555_LE |
| `ppu_policy.display_capabilities.raw_output.width` | 240 |
| `ppu_policy.display_capabilities.raw_output.height` | 160 |
| `ppu_policy.display_capabilities.raw_output.stride_bytes` | 480 |
| `ppu_policy.display_capabilities.raw_output.byte_length` | 76800 |

## Video memory access policy

For CPU and DMA reads and writes to palette RAM, VRAM, and OAM, Gamebloke keeps
the existing bus behavior in drawing, HBlank, VBlank, and forced blank. The
policy adds no PPU denial or cycles; bus request costs remain owned by
`bus_policy.request_cycles`. This is selected project behavior, with hardware
qualification `UNKNOWN`. The selected 1006-cycle HBlank point and visible-line-
start row sampling remain in force. The HBlank-edge, DMA-latch, and within-line
visibility disputes remain open.

| Setting | Classification | Hardware qualification | Masters | Regions | States | Availability | PPU extra cycles | Cost reference |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ppu_policy.video_memory_access.matrix` | project-selected | UNKNOWN | cpu, dma | pram, vram, oam | drawing, hblank, vblank, forced-blank | existing-bus-behavior | 0 | `bus_policy.request_cycles` |

## Timing model scope

Model ID: gamebloke-timer-model-1

Qualification: project-model-only

Non-ROM requests use one project-selected master cycle per successful access. Game Pak ROM quotes are contextual: the address selects WS0, WS1, or WS2; WAITCNT chooses the documented N/S wait states; each 16-bit beat adds one base cycle. A word access is two halfword beats, and each beat address is checked for the 128 KiB transition. These values describe the implemented quote path; they do not establish hardware qualification.

[Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) supplies WAITCNT wait choices. [Arm ARM7TDMI TRM DDI 0210C §§3.3 and 6.20](https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) supplies CPU N/S/I composition rules; it does not qualify GBA DMA startup conversion or Game Pak boundary behavior. The 128 KiB sequential transition is supported by GBATEK 3.06 as a secondary/community observation; hardware qualification remains UNKNOWN.

## Timing model costs

ROM rows are parameterized by the physical window selected from each beat address and the current WAITCNT value. Non-ROM project-selected rows retain their fixed one-cycle cost. Hardware cost remains null and hardware qualification remains UNKNOWN.

| Key | Execution class | Execution cycles / formula | Execution citation | Hardware-cost class | Hardware cycles | Hardware citation | Unmodeled limits |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `timing_model.read/bios/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/rom/1/n/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/1/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/1/s/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/1/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/2/n/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/2/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/2/s/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/2/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/4/n/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/4/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/4/s/fetch` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/4/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/unmapped/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/rom/1/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/1/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/2/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/2/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/4/n/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/4/s/data` | source-backed | formula: sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf); WAITCNT window and effective beat context select the runtime value; hardware qualification remains UNKNOWN. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/unmapped/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported non-ROM bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |

## WAITCNT policy

Wait selection follows the Nintendo manual. The register reset value and retained mask are Gamebloke project policy. The 128 KiB transition is secondary/community evidence, and the quoted path remains hardware-unqualified. Prefetch bit state does not model prefetch execution.

| Key | Value | Classification | Evidence |
| --- | --- | --- | --- |
| waitcnt_policy.register | address 0x04000204; retained mask 0x5FFF; reset 0x0000 (project-selected); prefetch bit 14 retained as state | project-selected | Gamebloke WAITCNT register implementation in src/machine.c and src/bus.c; the retained mask and reset initialization are project policy. |
| waitcnt_policy.window.ws0 | 0x08000000–0x09FFFFFF; N bits [2,3] select waits [4,3,2,8]; S bit 4 selects waits [2,1] | source-backed | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| waitcnt_policy.window.ws1 | 0x0A000000–0x0BFFFFFF; N bits [5,6] select waits [4,3,2,8]; S bit 7 selects waits [4,1] | source-backed | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| waitcnt_policy.window.ws2 | 0x0C000000–0x0DFFFFFF; N bits [8,9] select waits [4,3,2,8]; S bit 10 selects waits [8,1] | source-backed | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| waitcnt_policy.beat_policy | 16-bit bus; access widths [1,2,4] bytes; beat width 2 bytes; byte/halfword beats 1; word beats 2; second word beat sequential; sum over 16-bit beats of (selected WAITCNT N/S wait states for the beat address plus one base cycle) | source-backed | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 plus the tested Gamebloke 16-bit quote path. |
| waitcnt_policy.boundary | 131072 bytes; address-aligned beat at a 128 KiB boundary forces effective nonsequential timing; GBATEK 3.06, Game Pak 128 KiB sequential boundary; secondary/community source only.; qualification UNKNOWN | secondary-community | GBATEK 3.06, Game Pak 128 KiB sequential boundary; secondary/community source only. |
| waitcnt_policy.prefetch | bit 14 register state retained; execution unmodeled; hardware qualification UNKNOWN | unmodeled | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2; execution remains intentionally unmodeled. |
| waitcnt_policy.hardware_qualification | UNKNOWN | unmodeled | No physical hardware evidence was produced by this plan; qualification remains UNKNOWN. |

## DMA policy

The register fields and source-backed hardware rules are separated from Gamebloke choices. Startup conversion, unit-boundary arbitration, and per-channel latch selection remain project policy. The latch dispute stays open, and all hardware qualification remains UNKNOWN. VBlank, HBlank, audio, and DRQ trigger producers are deferred.

| Key | Value | Classification | Evidence |
| --- | --- | --- | --- |
| dma_policy.registers | 0x040000B0–0x040000DF; 4 channels; stride 12 bytes; control mask 0xFFE0 | source-backed | Nintendo AGB Programming Manual v1.1 Chapter 12 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| dma_policy.source_facts | source masks ["0x07FFFFFF","0x0FFFFFFF","0x0FFFFFFF","0x0FFFFFFF"]; destination masks ["0x07FFFFFF","0x07FFFFFF","0x07FFFFFF","0x0FFFFFFF"]; count masks ["0x3FFF","0x3FFF","0x3FFF","0xFFFF"]; zero-count expansions [16384,16384,16384,65536]; width bit 10 and widths [2,4]; source modes ["increment","decrement","fixed","unsupported"]; destination modes ["increment","decrement","fixed","increment-and-reload-channel-3-only"]; completion IF bits [8,9,10,11] | source-backed | Nintendo AGB Programming Manual v1.1 Chapter 12 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| dma_policy.trigger_policy | enable bit 15; immediate mode 0 starts on rising enable edge and is one-shot; delayed modes [1,2,3] remain armed; remain armed; VBlank/HBlank/audio/DRQ trigger producers are deferred; channel 0 mode 3 is refused | project-selected | Gamebloke DMA model in src/machine.c; PPU/audio/DRQ trigger producers remain deferred. |
| dma_policy.completion_policy | cleared on completion; remain armed and reload count; channel 3 mode 3 only; set the channel IF bit when control bit 14 is enabled | source-backed | Nintendo AGB Programming Manual v1.1 Chapter 12 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| dma_policy.startup_policy | 2 master cycles; conversion project-selected; hardware qualification UNKNOWN | project-selected | Gamebloke project mapping in include/gamebloke/gamebloke.h; manual activation wording does not qualify a master-cycle conversion. |
| dma_policy.arbitration_policy | priority [0,1,2,3]; active DMA pauses CPU execution; unit is one source access plus one destination access; reconsider after each complete transfer unit; resume sequence nonsequential; event order ["timer0-3","keypad","dma-ready0-3"]; finer boundary unmodeled; hardware qualification UNKNOWN | project-selected | Gamebloke selected DMA policy in src/machine.c; project tests establish implementation behavior only. |
| dma_policy.latch_policy | per-channel selected; latch-dependent results disputed; disp-dma-latch status open; hardware qualification UNKNOWN | disputed | registry/disputes.json entry disp-dma-latch; competing observations remain unresolved. |
| dma_policy.hardware_qualification | UNKNOWN | unmodeled | No physical hardware evidence was produced by this plan; qualification remains UNKNOWN. |

## CPU cycle composition

| Key | Classification | Rule | Citation |
| --- | --- | --- | --- |
| `timing_model.arm7tdmi-nsi-patterns` | source-backed | The ARM7TDMI timing summary expresses bus work as N/S/I/C cycles and accounts for pipeline overlap; examples are LDR 1S+1N+1I, STR 2N, and B/BL 2S+1N. | Arm ARM7TDMI Technical Reference Manual DDI 0210C §§3.3 and 6.20 Table 6-23 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) |
| `timing_model.arm7tdmi-internal-cycle-rules` | source-backed | Instruction-specific internal-cycle contributions follow the ARM7TDMI timing summary and remain separate from memory wait costs. | Arm ARM7TDMI Technical Reference Manual DDI 0210C §§3.3 and 6.20 Table 6-23 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) |
| `timing_model.gba-waitcnt-wait-selection` | source-backed | The WAITCNT register selects the documented nonsequential and sequential wait states for three Game Pak windows; each quoted halfword beat adds one base cycle. Prefetch execution is separate and remains unmodeled. | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |
| `timing_model.gba-prefetch-execution` | unmodeled | Game Pak prefetch fill/hit execution remains unmodeled. The WAITCNT prefetch bit is retained as register state only; no hardware qualification is claimed. | Nintendo AGB Programming Manual v1.1 §§3.3–3.3.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |

## Timer edge policy

These exact edge choices are project-selected conventions; the hardware result for each remains UNKNOWN.

| Key | Classification | Selected rule | Hardware qualification | Citation |
| --- | --- | --- | --- | --- |
| `timing_model.first-tick-after-enable` | project-selected | Disabled-to-enabled loads reload and resets divider phase; the first tick follows one complete selected divisor. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |
| `timing_model.live-reload` | project-selected | A reload write while running affects the next overflow; it does not replace the current counter. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |
| `timing_model.enabled-control-write` | project-selected | An enabled control write synchronizes at transaction completion and resets the selected divider phase. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |
| `timing_model.same-cycle-register-access` | project-selected | Drain scheduled timer events through bus transaction completion before sampling reads or committing writes. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |
| `timing_model.equal-cycle-order` | project-selected | Timer index orders equal-cycle overflows; insertion sequence breaks remaining ties. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |
| `timing_model.timer0-cascade` | project-selected | Timer 0 cascade is ignored by this model. | UNKNOWN | Gamebloke timer-model policy in src/machine.c and include/gamebloke/gamebloke.h; not a hardware source. |

## Timer 0 guest-path coverage

The original BIOS-free guest ROM fetch and literal reads resolve to contextual WAITCNT formula rows; Timer 0/IF I/O and IWRAM markers retain their non-ROM project-selected rows.

| Key | Guest access shape | Resolved cost key |
| --- | --- | --- |
| `timing_model.timer0-initial-rom-fetch` | Initial/refill ARM instruction fetch | `read/rom/4/n/fetch` |
| `timing_model.timer0-sequential-rom-fetch` | Sequential ARM instruction fetch | `read/rom/4/s/fetch` |
| `timing_model.timer0-rom-literal-read` | ARM literal data read from the ROM instruction pool | `read/rom/4/n/data` |
| `timing_model.timer0-reload-control-write` | Word write containing Timer 0 reload and control fields | `write/io/4/n/data` |
| `timing_model.timer0-if-halfword-read` | Halfword read polling Timer 0's IF request bit | `read/io/2/n/data` |
| `timing_model.timer0-iwram-marker-write` | Word write publishing the guest overflow marker/mailbox | `write/iwram/4/n/data` |

## Unmodeled hardware cost gaps

These are residual hardware-qualification gaps, not missing deterministic quote rows.

| Key | Classification | Hardware qualification | Affected shapes | Citation |
| --- | --- | --- | --- | --- |
| `timing_model.bus-costs` | unmodeled | UNKNOWN | Non-ROM rows remain project-selected one-cycle policy; all modeled and contextual bus values remain unqualified on hardware. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. |
| `timing_model.waitcnt` | unmodeled | UNKNOWN | ROM read/write values are computed from source-backed WAITCNT choices and beat formulas; the resulting implementation and boundary mapping have no hardware qualification. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. |
| `timing_model.prefetch` | unmodeled | UNKNOWN | Game Pak prefetch fill/hit execution; bit 14 may be retained as register state but does not enable modeled prefetch. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. |

## Bus policy

Non-ROM raw requests use one project-selected cycle. ROM costs use the WAITCNT window and halfword-beat formula above; no prefetch execution is modeled.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| bus_policy.request_cycles.bios | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.ewram | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.iwram | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.io | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.pram | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.vram | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.oam | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.widths_bytes | [1, 2, 4] | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.access_classes | [nonsequential, sequential] | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |
| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |

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
minimal bus. BIOS protection and unsupported I/O remain incomplete; WAITCNT wait selection is modeled, while prefetch execution remains unmodeled. The actual behavior of reads beyond a short ROM image remains
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

Profile version 6 retains the configured logo-window comparison using the
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
| cpu_policy.timing | unit-preflight-uniform-bus-plus-core-internal-work | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.exception_entry_cycles | 1 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.bios_none_stop | after-entry-before-vector-fetch | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.execution | single-interpreter | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.pc_operand_reads | arm-current+8-register-shift-rm-and-store+12-thumb-current+4 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.internal_work | ddi0029g-6.5-6.6-6.20-cost-table | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.multiply_flag_policy | preserve-short-c-long-cv | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.loaded_pc_boundary | align-mask-wrap-refill | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.unpredictable_operands | named-stop | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.extension_encoding | later-architecture-stop | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_policy.address_arithmetic | data-modulo32-static-fetch-checked | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |

## CPU instruction scope

The interpreter implements the legal ARM7TDMI ARMv4T ARM families and all 19
Thumb formats listed below. This is a source/implementation scope statement.
The complete original diagnostic corpus has executed locally with admitted
fixtures. Hosted qualification requires separate exact-head evidence. This
profile does not establish hardware accuracy or broad GBA compatibility.

| Scope | Opcode / operand forms | Label | Citation |
| ----- | --------------------- | ----- | -------- |
| cpu_scope.supported.1 | ARM data processing: AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN with rotated immediate and immediate/register LSL/LSR/ASR/ROR, defined flags and legal PC/exception-return forms | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.2 | ARM control flow: B, BL, BX | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.3 | ARM multiply: MUL, MLA, UMULL, UMLAL, SMULL, SMLAL with S variants and captured operands | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.4 | ARM status transfers: MRS CPSR/SPSR and MSR register/rotated-immediate c/x/s/f masks, privilege and bank rules | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.5 | ARM single transfer: LDR/STR byte/word, immediate/scaled-register offsets, P/U/B/W/L and post-index T forms, legal PC operands and writeback | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.6 | ARM subword transfer: LDRH, STRH, LDRSB, LDRSH with immediate/register offsets, pre/post indexing and writeback | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.7 | ARM block transfer: LDM/STM IA/IB/DA/DB, writeback, user-bank S forms and SPSR-restoring LDM with PC | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.8 | ARM swap: SWP and SWPB with ordered read/write and captured operands | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.9 | ARM exception decode: full SWI imm24, reserved UND and absent CDP/MRC/MCR/LDC/STC coprocessor entry before BIOS-none stop | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.10 | Thumb formats 1-5: immediate shifts, add/subtract, immediate operations, all 16 register ALU operations and legal high-register ADD/CMP/MOV/BX | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.11 | Thumb formats 6-12: 6 PC-relative LDR, 7 register-offset transfers, 8 signed byte/halfword transfers, 9 immediate-offset byte/word transfers, 10 halfword transfers, 11 SP-relative transfers, 12 PC/SP address formation | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.12 | Thumb formats 13-19: 13 SP adjustment, 14 PUSH/POP with LR/PC, 15 STMIA/LDMIA, 16 conditional B, 17 SWI imm8, 18 unconditional B, 19 BL prefix/suffix | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.supported.13 | Executing project policies: short MUL Rd=Rm and SWP/SWPB Rn=Rd or Rn=Rm snapshot aliases, ARM empty/base-in-list reference rules and isolated BL halves/modulo offset arithmetic | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.1 | Unpredictable operands stop before effects except the explicitly listed executing project policies, including named stops for NV, invalid PC/PSR operands, Thumb same-register MUL, empty Thumb lists and STMIA base-middle/last | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.2 | Allocated later encodings recognized by the decoder stop as later-architecture-instruction, with no ARMv5+ execution semantics | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.3 | Unallocated exact-core encodings enter UND, while malformed allocated decode bits are unpredictable | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.4 | No attached coprocessor or BIOS handler executes, exception entry precedes named BIOS-none stop and vector bytes are never fetched | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.5 | Static B/BX and ALU-PC target alignment/refill extents retain checked unsupported refusal, not a missing legal instruction family | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_scope.excluded.6 | Original full diagnostic corpus executed locally with admitted fixtures, hosted qualification requires separate exact-head evidence, no hardware, game, BIOS or release qualification is claimed | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |

## CPU stop classifications

Every row projects to GUEST_STOP with matching reason and termination. The
exception rows enter architectural state first. Operand/extension/static
refusals stop before instruction effects.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| cpu_stop.unpredictable | unpredictable-instruction | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_stop.later-architecture | later-architecture-instruction | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_stop.undefined | no-bios-undefined | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_stop.absent-coprocessor | no-bios-coprocessor | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_stop.swi | no-bios-swi | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |
| cpu_stop.static-target-refusal | unsupported-instruction | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |

## CPU source and policy boundaries

The source hierarchy is the exact ARM7TDMI TRM DDI0029G, then ARM ARM DDI0100I
for architecture-version/operand rules. GBATEK 3.06 supplies the selected GBA
alignment and ARM empty/base-list reference rules. Reference facts are distinct
from the deterministic project choices below, none of which is a hardware oracle.

No BIOS stack convention is initialized. General and banked registers and valid
SPSRs start at zero, User/System has no SPSR, and the pipeline is empty. Entry is
0x08000000 with CPSR 0x000000DF (ARM/System, flags clear, IRQ/FIQ masked).
User MSR writes flags only, privileged MSR applies ARMv4 masks, unallocated-bit
or T-state requests stop as unpredictable, and S+PC requires a valid banked SPSR.
SPSR restoration changes the active bank/state before the target refill.

Normal ARM PC reads use current+8. Register-shift Rm and ARM store-PC sources
use current+12. Thumb reads use current+4, aligned down to a word for literals
and PC-relative address formation. LDR/LDM PC align down to an ARM word without
interworking, Thumb POP PC masks bit0 and keeps Thumb state. Dynamic loaded-PC
refills wrap modulo 2^32 and their generated top-address pair stops as
unpredictable on the next call. BX alone selects state from bit0. Static B/BX
and ALU-PC alignment/refill-extent checks remain explicit unsupported refusals.
Data effective-address/writeback arithmetic wraps modulo 2^32 independently.

CPU-local work follows DDI0029G sections 6.5, 6.6 and 6.20. Immediate ARM shifts
add zero internal cycles, register shifts add one, and Thumb register shifts add
one. Multiplier m is 1/2/3/4 from the original multiplier's high-byte termination
pattern. Short MUL/MLA and Thumb MUL choose signed termination as project policy,
long forms follow opcode signedness. Internal costs are m for MUL/Thumb MUL,
m+1 for MLA and long multiply, m+2 for long accumulate. Block-load final internal
work and loaded-PC refill work remain preflighted. All time goes through the bus.
Raw bus requests still cost one project cycle each, so no WAITCNT, Game Pak
prefetch, latch, complete cycle-accuracy or physical bus-trace claim follows.

S multiply updates N/Z. Short C and long C/V preservation are deterministic
project policy because those flags are unpredictable under ARMv4, while short V
is architecturally preserved. Non-S multiplies preserve all flags. The following
executing exceptions to unpredictable_operands=named-stop are explicit project
choices: short MUL Rd=Rm snapshots its operands, SWP/SWPB Rn=Rd or Rn=Rm snapshot
the original base/source, ARM empty lists transfer PC with +/-0x40 writeback,
and ARM base-in-list forms use the selected GBATEK ARMv4 reference rules.
Isolated BL halves execute using banked LR and modulo offset arithmetic as
project policy. Thumb same-register MUL, empty PUSH/POP/LDMIA/STMIA, and STMIA
base-middle/last stop for the whole unit before effects. These policies establish
repeatable project behavior, not exact silicon results for unpredictable inputs.

Public unsupported values remain ABI-compatible. No legal family is assigned
an invented unsupported witness. Normal NV encodings stop as unpredictable.
Recognized later allocations (BLX/BXJ/BKPT/CLZ, DSP and doubleword/multiply or
coprocessor extensions, and recognized REV forms) stop as later architecture.
BLX-immediate recognition precedes generic NV as an explicit reporting choice.
Reserved exact-core encodings enter UND. Malformed allocated decode-bit patterns
are unpredictable under DDI0100I A3.16. No attached coprocessor responds to legal
ARMv4T coprocessor instructions, so they enter UND with a distinct absent-
coprocessor cause. All these causes are GUEST_STOP in result/1. Refusals preserve
machine/time/trace apart from the stop latch and retain the queued full opcode.

ARM SWI preserves its complete 24-bit comment and Thumb SWI its complete imm8
in both instruction and exception trace records. Exception bookkeeping saves
the old CPSR and architectural link in the correct bank, selects the vector,
then stops under BIOS provider none before any vector fetch. No handler
continuation or diagnostic PASS is implied. IRQ-between-BL is API-only evidence
with IRQ explicitly unmasked, not an active interrupt-controller or CLI claim.

Successful store traces currently contain the bus result zero rather than the
attempted write payload. Inspect memory for stored data. BIOS/device execution,
physical controllers, hardware/game accuracy, licensing and release qualification
remain outside this profile's evidence.

## IRQ, input, and power policy

The following IRQ, keypad, and power behavior is the selected Gamebloke
deterministic model. The values do not qualify GBA interrupt latency, a BIOS
handler, or physical keypad behavior. The event priority and cycle observations
are checked by the focused Phase 8 CPU and timer selectors.

| Setting | Value | Label | Citation |
| ------- | ----- | ----- | -------- |
| irq_input_power_policy.if_ie_mask | 16383 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.ime_mask | 1 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.cpsr_i_mask | 128 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.timer0_if_bit | 8 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keypad_if_bit | 4096 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keyinput_mask | 1023 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keycnt_mask | 50175 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.sample_boundary | completed-instruction-effects-and-events-before-next-instruction | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.initial_boundary | after-refill-before-first-instruction | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.entry_internal_cycles | 1 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.no_bios_irq_stop | after-entry-before-vector-fetch | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.timer_event_priorities | [0, 1, 2, 3] | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keypad_event_priority | 4 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.input_event_capacity | 256 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.key_polarity | active-low | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keycnt_empty_selection | false | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.keycnt_effective_edge | false-to-true | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.if_ack_while_held | stays-clear-until-condition-falls-then-rises | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.halt_wake_condition | IF-and-IE | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.halt_wake_mask_independent | [IME, CPSR.I] | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.power_transition_cost | internal-only | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.stop_policy | terminal-guest-stop-no-bios-wake | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.stop_cause_code | 3 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.deadlock_policy | distinct-guest-stop | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.deadlock_cause_code | 4 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.timeout_policy | cycle-budget-timeout | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.timeout_reason_code | 2 | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |
| irq_input_power_policy.qualification | project-model-only | IMPL | Gamebloke deterministic Phase 8 IRQ/input/power policy; project-model behavior only, with no GBA IRQ latency qualification. |

## IRQ/input/power policy sources

These cited hardware facts remain separate from the project-selected policy
above. ARM7TDMI exception state and core cycles do not specify GBA bus waits or
total GBA interrupt latency. Nintendo's manual describes device behavior; it
does not qualify this implementation's timing.

| Fact | Evidence | Label | Citation |
| ---- | -------- | ----- | -------- |
| irq_input_power_policy.source_facts.arm_exception_entry | ARM7TDMI exception state and entry behavior | REF | Arm ARM7TDMI Technical Reference Manual DDI 0210C §§2.8.1–2.8.5 and 6.12 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) |
| irq_input_power_policy.source_facts.gba_devices_and_power | GBA keypad, IRQ, HALT, and STOP behavior | REF | Nintendo AGB Programming Manual v1.1 §§14.1–14.2.1, 15, 16.1–16.2 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |

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
| `prefetch` | Game Pak prefetch fill/hit execution is unmodeled; WAITCNT bit 14 is register state only. |
| `waitcnt` | Documented wait choices drive contextual ROM beat quotes, but hardware qualification remains UNKNOWN and the 128 KiB mapping has secondary provenance. |
| `bus-costs` | Non-ROM values remain project-selected and all modeled bus costs remain hardware-unqualified. |
| `hblank-flag-edge` | Hardware timing for the HBlank flag change remains unknown and disputed under disp-hblank-flag-edge; the selected 1,006-cycle model event is in ppu_policy and is not a hardware resolution. |
| `ppu` | The deterministic partial mode, normal OBJ, and composition subset is described in `ppu_policy`; hardware qualification remains UNKNOWN, and windows, effects, mosaic, forced blank, and unsupported object controls remain unmodeled. |
| `audio` | Sound. |
| `dma` | The deterministic project DMA subset and VBlank plus visible-line HBlank triggers are selected policy; hardware qualification remains UNKNOWN, latch interpretation is disputed, and DMA3 display-synchronized mode plus deferred producers remain unmodeled. |
| `timer-edge-qualification` | Exact hardware qualification for first tick, live reload, divider phase, enabled-control writes, same-cycle register access and equal-cycle ordering. The deterministic timer model is implemented; these edges remain UNKNOWN on hardware. |
| `irq-controller` | A complete hardware-qualified GBA interrupt controller. The deterministic IF/IE/IME and CPU-boundary subset is implemented under `irq_input_power_policy`; exact GBA IRQ latency and BIOS-handler behavior remain unqualified. |
| `backup-media` | Cartridge backup storage (save memory). |
| `bios-protection` | CPU fetches participate in BIOS protection checks, but region/state-specific open-bus behavior and Game Pak prefetch are not qualified or emulated. |
| `io-devices` | I/O register semantics remain incomplete and hardware-unqualified. Timer, IF/IE/IME, KEYINPUT/KEYCNT, and the VCOUNT/DISPSTAT/DISPCNT subset is described by the timer, `irq_input_power_policy`, and `ppu_policy` entries; unsupported reads still use the project-policy placeholder. |
