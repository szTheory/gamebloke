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

profile_version: 7

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

## Timing model scope

Model ID: gamebloke-timer-model-1

Qualification: project-model-only

Every supported runtime bus key has an explicit row below. The runtime selects one cycle for each successful access. That value is a project placeholder, not a hardware cost. The hardware-cost column remains null and unmodeled until the complete region, width, sequence and access-role mapping is sourced. Source-backed ARM cycle composition is separate from bus waits.

Timer start, first tick, divider phase, live reload and same-cycle register ordering are selected deterministic model policies. Exact hardware edge qualification remains UNKNOWN.

[Nintendo AGB Programming Manual v1.1 §§3.2–3.3](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) documents configurable Game Pak waits. [Arm ARM7TDMI TRM DDI 0210C §§3.3 and 6.20](https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) supplies CPU N/S/I composition rules; it does not qualify GBA bus waits.

## Timing model costs

The first key component is the model identifier; row keys enumerate direction, region, width, sequence class and fetch/data role exactly.

| Key | Execution class | Execution cycles | Execution citation | Hardware-cost class | Hardware cycles | Hardware citation | Unmodeled limits |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `timing_model.read/bios/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/bios/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/ewram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/iwram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/io/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/pram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/vram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/oam/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/rom/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/rom/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt, prefetch |
| `timing_model.read/rom/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.read/unmapped/1/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/n/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/s/fetch` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.read/unmapped/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/bios/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/ewram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/iwram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/io/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/pram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/vram/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/oam/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/rom/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/rom/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. | bus-costs, waitcnt |
| `timing_model.write/unmapped/1/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/1/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/2/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/2/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/4/n/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |
| `timing_model.write/unmapped/4/s/data` | project-selected | 1 | Gamebloke policy: one model cycle per successful supported bus request in gamebloke-timer-model-1; project-selected, not hardware-qualified. | unmodeled | null | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. | bus-costs |

## CPU cycle composition

| Key | Classification | Rule | Citation |
| --- | --- | --- | --- |
| `timing_model.arm7tdmi-nsi-patterns` | source-backed | The ARM7TDMI timing summary expresses bus work as N/S/I/C cycles and accounts for pipeline overlap; examples are LDR 1S+1N+1I, STR 2N, and B/BL 2S+1N. | Arm ARM7TDMI Technical Reference Manual DDI 0210C §§3.3 and 6.20 Table 6-23 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) |
| `timing_model.arm7tdmi-internal-cycle-rules` | source-backed | Instruction-specific internal-cycle contributions follow the ARM7TDMI timing summary and remain separate from memory wait costs. | Arm ARM7TDMI Technical Reference Manual DDI 0210C §§3.3 and 6.20 Table 6-23 (https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819) |
| `timing_model.gba-memory-wait-mapping` | unmodeled | The mapping from CPU cycle classes to each GBA region, width, sequence class and fetch/data role is incomplete; configurable Game Pak waits and prefetch remain outside this profile. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. |

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

The original BIOS-free guest's instruction fetch, literal read, Timer 0/IF I/O and IWRAM result marker each resolve to one explicit bus-cost row.

| Key | Guest access shape | Resolved cost key |
| --- | --- | --- |
| `timing_model.timer0-initial-rom-fetch` | Initial/refill ARM instruction fetch | `read/rom/4/n/fetch` |
| `timing_model.timer0-sequential-rom-fetch` | Sequential ARM instruction fetch | `read/rom/4/s/fetch` |
| `timing_model.timer0-rom-literal-read` | ARM literal data read from the ROM instruction pool | `read/rom/4/n/data` |
| `timing_model.timer0-reload-control-write` | Word write containing Timer 0 reload and control fields | `write/io/4/n/data` |
| `timing_model.timer0-if-halfword-read` | Halfword read polling Timer 0's IF request bit | `read/io/2/n/data` |
| `timing_model.timer0-iwram-marker-write` | Word write publishing the guest overflow marker/mailbox | `write/iwram/4/n/data` |

## Unmodeled hardware cost gaps

These are hardware qualification gaps, not missing deterministic execution rows.

| Key | Classification | Hardware qualification | Affected shapes | Citation |
| --- | --- | --- | --- | --- |
| `timing_model.bus-costs` | unmodeled | UNKNOWN | All direction, region, width, sequence-class and fetch/data rows. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): no complete hardware-cost mapping is modeled for this region, width, sequence and role. |
| `timing_model.waitcnt` | unmodeled | UNKNOWN | All ROM reads and writes, widths 1/2/4, both sequence classes, and supported fetch/data roles. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. |
| `timing_model.prefetch` | unmodeled | UNKNOWN | ROM instruction-fetch rows, widths 1/2/4 and both sequence classes. | Nintendo AGB Programming Manual v1.1 §§3.2–3.3 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf): Game Pak accesses use configurable waits; this row's exact hardware cost is unmodeled. |

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
| `prefetch` | Game Pak prefetch timing; all ROM fetch hardware costs remain UNKNOWN. |
| `waitcnt` | WAITCNT-dependent Game Pak waits; all ROM hardware costs remain UNKNOWN. |
| `bus-costs` | Complete hardware costs by direction, region, width, sequence class and fetch/data role. |
| `hblank-flag-edge` | The cycle at which the HBlank flag changes; disputed, see disp-hblank-flag-edge. |
| `ppu` | Picture processing. |
| `audio` | Sound. |
| `dma` | DMA transfers and their timing. |
| `timer-edge-qualification` | Exact hardware qualification for first tick, live reload, divider phase, enabled-control writes, same-cycle register access and equal-cycle ordering. The deterministic timer model is implemented; these edges remain UNKNOWN on hardware. |
| `irq-controller` | A complete hardware-qualified GBA interrupt controller. The deterministic IF/IE/IME and CPU-boundary subset is implemented under `irq_input_power_policy`; exact GBA IRQ latency and BIOS-handler behavior remain unqualified. |
| `backup-media` | Cartridge backup storage (save memory). |
| `bios-protection` | CPU fetches participate in BIOS protection checks, but region/state-specific open-bus behavior and Game Pak prefetch are not qualified or emulated. |
| `io-devices` | Complete I/O device register semantics remain incomplete and hardware-unqualified. The timer, IF/IE/IME, KEYINPUT, and KEYCNT subset is described by the timer and `irq_input_power_policy` entries; unsupported reads still use the project-policy placeholder. |
