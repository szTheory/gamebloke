# Feature Research

**Domain:** Deterministic, accuracy-focused Game Boy Advance emulator (core + headless runner + small host adapters)
**Project:** Gamebloke
**Researched:** 2026-11-09
**Confidence:** MEDIUM-HIGH (emulator facts verified from current repos/source; differentiator ranking is inference)

## How to read this file

Claims carry one of four labels. Nothing in the bootstrap pack was trusted without a re-check.

| Label | Meaning |
|-------|---------|
| **[FACT]** | Primary-source fact: read from a vendor page, current upstream source/changelog/license file, or the hardware reference on 2026-11-09 |
| **[OBS]** | Project observation: read from a sibling project's planning records (Playstead, LibFuturo), read-only |
| **[INF]** | Inference or recommendation by this researcher; could be wrong |
| **[UNK]** | Unknown / could not be verified; do not build on it without a spike |

GBATEK and Tonc are community references, not Nintendo documents; [FACT] on GBATEK means "GBATEK states", not "hardware is proven".

## Headline recommendations (decisions this research can change)

| Question | Recommendation | Confidence |
|----------|----------------|------------|
| 1. Differentiator | **Deterministic, evidence-labelled machine verification**: fresh instance + named profile + timed logical input in, machine-readable semantic verdict + guest-time event trace + raw video/audio out, each result stating what it proves. "Owner-controlled core" is the enabling reason, not the user-facing outcome. | MEDIUM |
| 2. First profile | **`agb` (original GBA machine) with `direct-start` boot**, BIOS provider declared as a separate field. SP = `agb` core + host display profile. Micro = deferred profile. | HIGH (SP/AGB core equivalence is per GBATEK only) |
| 3. BIOS/boot | Two orthogonal axes: boot path (direct-start first) and BIOS provider (`none` first, then project-authored/open replacement built from source, then user-supplied official as optional never-bundled). HLE deferred. | HIGH |
| 4. Cartridge/backup | v1 = ROM up to 32 MiB with header diagnostics + explicitly declared backup device. SRAM first; EEPROM 512 B/8 KiB and Flash 64K/128K complete v1. Save type is declared, never silently inferred. | HIGH |
| 5. Accessories | All deferred (RTC/GPIO first in line) with named promotion evidence below. | HIGH |
| 6. Dev features | v1: headless runner, input timeline, semantic results, raw A/V capture, minimal trace. v1.x: internal snapshot/restore as a test instrument. Deferred: user save states, rewind, GDB stub. | MEDIUM-HIGH |
| 7. Host | **macOS arm64 relocatable CLI/C consumer** from an installed prefix, plus a Linux determinism cross-check in CI. GUI host later. | HIGH |

The project start is not blocked by the differentiator question. Every v1 feature below is required by every ranked candidate, so the roadmap can proceed while the owner confirms the wording.

---

## Q1. Differentiator beside Playstead's managed mGBA route

### What the mGBA route does and does not qualify for GBA

**Playstead [OBS]** (read from `~/projects/playstead/.planning/{PROJECT,REQUIREMENTS,ROADMAP}.md` and the Phase 06 continuation ledger):

- GBA/GB/GBC run through a managed, digest-pinned **mGBA 0.10.5 Qt binary** as an external process, pinned to macOS 26/arm64. This is a GUI app, not a library or headless runner.
- Persistent-save qualification is GB/GBC-focused (MBC1 8 KiB, MBC5 32 KiB, MBC3 32 KiB + 48-byte RTC). GBA coverage is "bounded to restore/Continue-menu" on one private fixture. No GBA save-state, RTC, or broader backup-device claim.
- State support is limited to one GB/MBC1/SRAM configuration. No GBA state claim.
- **Phase 06 continuation spike is `qualification / blocked-capability`**: the pinned 0.10.5 binary has no supported noninteractive way to load a Lua script, so recorded input and a game-visible resumed-state oracle could not be qualified. Keyboard injection was "inconclusive" in the GB/GBC spike; Lua held-key input worked only when pasted into the GUI scripting window.
- Playstead lists "Building emulator cores or a new emulation ABI" as **out of scope**. Playstead does not need Gamebloke.

**Inference [INF]:** There is no evidence that Playstead users need a second GBA core. The honest value of Gamebloke is for the owner's portfolio and for developers/integrators who need a GBA machine they can test, trace, and embed. It should not be pitched as a better player than mGBA.

### Current state of the alternatives (verified 2026-11-09)

| Emulator | Verified facts | Relevance |
|----------|----------------|-----------|
| **mGBA** (MPL-2.0, active: last push 2026-11-06) | [FACT] Latest release 0.10.5 (2025-03-08); mgba.io says it is still current. Qt + SDL frontends, CLI debugger, **GDB stub** (`ENABLE_GDB_STUB`), Lua scripting since 0.10 (`emu:runFrame`, `setKeys`, memory domains, callbacks, screenshot, save-state calls), shared/static **libmgba**, Python bindings option, SQLite game DB, built-in HLE-style BIOS image (`hleBios`) with optional external BIOS, rewind, 9 state slots, local link, e-Reader, solar, tilt, rumble, RTC. README lists re-recording as "planned". **Unreleased 0.11.0 on master adds a "Headless" frontend (`src/platform/headless-main.c`: `--script FILE`, `-S SWI` exit, `-R REG` exit code), startup scripts, scripted debugger breakpoints/watchpoints, range watchpoints, memory access logging, plus GBA DMA latching and video fixes.** Determinism is not a stated goal or guarantee anywhere I found. | The mGBA headless-and-scripted gap Playstead hit is **being closed upstream**. A "headless runner" alone will not stay a differentiator once 0.11 ships. |
| **NanoBoyAdvance** | [FACT] Cycle-accurate "most components" (CPU, DMA, timers, PPU, Game Pak prefetch); claims all AGS Aging cartridge tests, most of mGBA suite, ARMWrestler, gba-suite, FuzzARM. BIOS file required (official recommended; unofficial open BIOS "less accurate"); no HLE BIOS mentioned. RTC and solar emulated. Qt (now Qt Quick) frontend; 10 state slots; palette/tile/BG/OAM viewers. **Moved to Codeberg**: GitHub is archived/frozen; Codeberg `master` is GPL-3.0, the in-development `phoenix` branch ("Update 2") is BSD-2-Clause (relicense commit 2026-07-26); last commit seen 2026-10-30. | The accuracy bar is already high and actively developed. Gamebloke would be a late entrant on raw accuracy. |
| **SkyEmu** (MIT) | [FACT] Claims per-pixel PPU with mid-scanline effects, AGS Aging pass, 100% ArmWrestler/FuzzARM, 2020/2020 GBA Suite timing tests **with official BIOS**, full pipeline + prefetch. Bundles open-source replacement BIOS/stubs (HLE-ness not stated), strongly recommends official dump. RTC and solar. **`SkyEmu http_server <port> <rom>` headless mode** with `/step frames=N`, `/input`, `/read_byte`, `/write_byte`, `/save`, `/load`, `/screen` (PNG). Docs make **no determinism guarantee**. Native builds "experimental". | Headless stepping/control already exists elsewhere; determinism *contract* does not. |
| **ares** (ISC) | [FACT] v144 (2025-04-28) GBA notes: burst-transfer address latching, BG VRAM access timing, game DB update, mid-scanline scroll writes. No BIOS requirement or accuracy claim stated on the page I could read. Claims "accuracy and preservation", clarity over speed. A 2024 lead-developer remark says GBA was "close to" passing AGS Aging [secondary, treat as UNK]. | Clarity-first accuracy codebase; useful as a differential reference, no testing hooks. |
| **BizHawk** (MIT) | [FACT] Offers mGBA as its GBA core; input recording/TAS movies, savestates, frame stepping, Lua/C# scripting for all cores. GBAHawk is a separate project (Windows host). No GBA-specific determinism guarantee stated. | TAS-grade replay is a real prior art for "timed input in, deterministic out", but on top of mGBA. |
| **Mesen2** (license not verified here) | [FACT, partial, search summary only] GBA support added in 2.1.0. Debugger specifics for GBA not verified. | Strong debugger tradition; not verified for GBA, so not weighted. |
| **gdkGBA** (Unlicense) | [FACT] Small C emulator, SDL2, README says "early stages", PPU incomplete. | Educational scale; not a competitor. |
| **Dust / "Optime"** | [UNK] Dust is a DS emulator (GBA-slot only); I found nothing for "Optime" worth citing. Not used. | Dropped. |

### Ranked differentiator candidates

Ranking is [INF]. Value = outcome for a developer/integrator/owner. Evidence = how well sources above support the need.

| Rank | Candidate | Value | Evidence | Cost | Verdict |
|------|-----------|-------|----------|------|---------|
| **1** | **Deterministic headless verification with semantic results + guest-time event trace** (one product: runner contract plus observation channels) | HIGH | MEDIUM-HIGH: Playstead blocked on exactly this [OBS]; mGBA 0.10.5 lacks it, 0.11 adds a headless runner but no determinism contract [FACT]; SkyEmu disclaims determinism [FACT]; the public test ROMs report results only on screen (jsmolka: first failed test number shown in mode 4; mGBA suite renders a text grid) [FACT], so a *semantic* extraction layer has real work to do | MEDIUM, mostly interface design, cheap if designed in from day 1 and expensive to retrofit | **Recommend.** Differentiates on the contract, not on "headless". |
| 2 | Owner-controlled core | MEDIUM (portfolio coherence, license/ABI control, ability to define API postconditions) | HIGH that the owner wants it (settled decision) | It is the whole project | **Enabler, not a differentiator.** Say it as the reason, not the outcome. |
| 3 | Evidence-labelled accuracy claims (per-profile, per-oracle ledger; cycle-aware timing driven by hardware-derived vectors) | HIGH long-term | MEDIUM: NBA/SkyEmu already pass most public timing tests, so score-chasing is not differentiating; the *labelling discipline* is | HIGH (accuracy), LOW (ledger) | Pursue the ledger from day 1. Do not market "cycle accuracy" as the differentiator. |
| 4 | Embeddable portable C library | MEDIUM | LOW-MEDIUM: libmgba already ships shared/static, MPL-2.0 [FACT] | MEDIUM | Required anyway; differentiates only through strict ownership/refusal semantics and no hidden globals. |
| 5 | LibFuturo-native GBA profile | LOW now, MEDIUM later | LOW: LibFuturo has no GBA profile and its contract is experimental; the only real profiles are NES (Nesturbator) and Atari 7800 (Muhtari) [OBS] | MEDIUM (frame-oriented advance vs cycle-level claims needs proof) | Defer until a real core + consumer exist (already a settled gate). |
| 6 | Cycle accuracy as headline | HIGH if achieved | LOW for *Gamebloke winning*: NBA is a dedicated accuracy project with an AGS pass claim | VERY HIGH | Not a differentiator; a long-term goal. |

**Recommended differentiator statement (for owner confirmation):**
> Gamebloke is the GBA machine you can ask a precise question and get a repeatable, labelled answer: given a named profile, content, and timed logical input, a fresh instance returns a machine-readable verdict (guest progress, machine time, selected CPU/device effects, raw video, raw audio, each separate) and a guest-time event trace, and every result says what it proves and what it does not.

**Durability caveats [INF]:** (a) When mGBA 0.11 ships, the "no noninteractive scripting" evidence from Playstead may disappear independently; re-check at each roadmap review. (b) The differentiator survives only if the *determinism contract and result taxonomy* are real: identical machine-visible outputs across macOS arm64 and Linux x86_64, distinct missing/skipped/harness-failed/timed-out/cancelled/guest-failed results, and an explicit "proves / does not prove" field. (c) Because Gamebloke does not copy code from GPL/MPL references until the project license is chosen (owner gate), the owner-controlled core remains clean-room.

---

## Q2. First machine profile: AGB vs SP vs Micro

| Difference | Machine-core or host? | Source |
|------------|-----------------------|--------|
| Display: AGB unlit 2.9" TFT; SP front/backlit (backlight toggle button handled by **separate hardware logic**, "no way to detect or change the backlight state by software"); Micro 2.0" TFT with backlight, same 240x160 | **Host** (color response, brightness, scaling) | [FACT] GBATEK |
| "SP appears 100% compatible with GBA, there seems to be no way to detect SPs by software" | SP = **same machine core** | [FACT] GBATEK (community; hardware equivalence not independently proven) |
| Power: 2xAA vs built-in Li-ion vs Micro; charging, lid, power switch | **Host** | [FACT] GBATEK |
| Audio path: AGB built-in mono speaker/3.5 mm; SP has no headphone jack in stock form | **Host** | [FACT/INF] |
| Micro: 32-bit GBA games only, **no GB/CGB mode** | Machine-profile property (relevant only if GB/CGB mode is ever promoted) | [FACT] GBATEK |
| Micro: 256K EWRAM is on-chip in the CPU and "a bit slower"; the undocumented 0x4000800 "overclocked WRAM" setting that works on AGB/SP **locks up** on Micro | **Machine-core** (only observable to programs that poke undocumented registers) | [FACT] GBATEK |
| Region: no hardware region lock found; cartridge game code carries a destination letter; Nintendo's GBA page describes one hardware spec | **Metadata/host** (language, game-code letter). No machine-core region difference found | [FACT/UNK] absence-of-evidence only |
| BIOS variants: mGBA recognizes three checksums (official GBA, official GBA-in-DS, a known-broken "debug" BIOS) | Per-provider, not per-region | [FACT] mGBA `gba.c` |
| Whether SP/Micro retail BIOS images differ from AGB's | | [UNK] |

**Recommendation:** first profile name **`agb`** (original Game Boy Advance, matching the "AGB Programming Manual" naming), boot mode **`direct-start`**, BIOS provider recorded as its own field in every claim tuple. Treat SP as `agb` plus a host display/audio preset (no separate machine profile). Define `agb-micro` only when a fixture exercises the EWRAM/0x4000800 difference with a hardware-derived expectation, which needs owner hardware (human gate).

**Claim tuple (proposed):** `{machine=agb, boot=direct-start|full-boot, bios_provider=none|project-stub|open-replacement@rev|official@digest, cart={rom_size, backup_device, hardware=none}, input_timeline, evidence}`.

---

## Q3. BIOS and boot policy

**Key hardware fact [FACT, GBATEK]:** the IRQ vector (0x00000018) lives in BIOS ROM, so on a real machine every interrupt first runs BIOS code (`b 128h; stmfd r13!,{r0-r3,r12,r14}; mov r0,#4000000h; add r14,r15,#0; ldr r15,[r0,#-4]` jumping to the user handler at `[03FFFFFC]`). Cartridge header validation (Nintendo logo, fixed byte 0x96, complement checksum) is performed **by the BIOS at boot** and locks the machine on mismatch. **Therefore "no external BIOS" is not "no BIOS code":** skipping the boot sequence does not remove the need for IRQ and SWI code.

| Option | What works with it | What it makes claimable | What it does not | Cost | Verdict |
|--------|--------------------|-------------------------|------------------|------|---------|
| **A. No BIOS image, direct start** | Project-authored diagnostics that poll (VCOUNT, DISPSTAT, KEYINPUT), use no SWI and no IRQ, and use DMA/timers by register polling. Original polling-only homebrew. | "Machine executes ARM/Thumb, bus, PPU, DMA, timers, keypad from documented post-boot state S, no BIOS." Public-CI runnable. | IRQ-driven code, any libgba/Tonc/devkitARM homebrew that calls SWIs (VBlankIntrWait, CpuSet, Div, LZ77…), BIOS-dependent test suites. Needs a defined behavior for BIOS-region reads. | LOW | **First slice.** |
| **B. Project-authored minimal boot stub** (original code: IRQ trampoline + a documented SWI subset) | Interrupt-using diagnostics; a subset of SWIs | "Runs with stub revision R; SWI set X". Public-CI runnable; rights are clean if authored fresh from documentation. | Timing parity with the Nintendo BIOS; any SWI not in the set | LOW-MEDIUM | **Second slice** (enables a real interrupt-driven playable scenario). Needs owner nod on clean-room posture. |
| **C. Open-source replacement BIOS built from source** | Cult-of-GBA/BIOS: ARM assembly "from scratch", MIT, (c) 2020-21 DenSinH and fleroviux, last push 2025-04-06 [FACT]. Authors say "accuracy is a non-goal" and list unfinished sound calls and BIOS-read protection as TODO [FACT]. SkyEmu bundles comparable stubs and warns they are less accurate [FACT]. | "Runs retail-style homebrew with replacement BIOS rev R built from commit C". | Anything depending on exact official timing/state; not verified against any game list by this research [UNK] | MEDIUM (pin, build recipe, toolchain not named on the page [UNK], license notices) | **v1.x** as a fixture with full manifest. |
| **D. HLE BIOS** (SWIs implemented in host code) | mGBA ships an `hleBios` image plus C SWI handlers; its source logs "Stub software interrupt" for SoundBias and others and "Unimplemented unaligned Huffman" [FACT] | "SWI semantic results for the implemented set" | Cycle behavior of SWIs; BIOS-side effects; also still needs an image for vectors | MEDIUM-HIGH, and it is a second implementation of every SWI that diverges from real hardware | **Defer.** Promote only if a named workload needs speed or SWIs that B/C cannot cover. |
| **E. User-supplied official BIOS** (16 KiB, never bundled, never in public CI) | All of the above plus real-BIOS behaviors | "Behavior with official BIOS identified by digest D" (identity, not redistribution right). Enables the BIOS-dependent suites (mGBA suite `bios-math`; jsmolka `bios`; SkyEmu's own timing claims are "with official BIOS") [FACT] | Public reproducibility: such runs must report `skipped: no BIOS` visibly in public CI | LOW (loader + digest check; mGBA accepts a wrong checksum with a warning, Gamebloke should not) | **v1 optional input**, never required. |
| **Boot path: full boot vs direct-start** | Orthogonal to provider. mGBA's skip path sets PC to ROM start, VCOUNT=0x7E, POSTFLG=1 [FACT]; 0.10.5 fixed "booting into BIOS when skip BIOS is enabled" [FACT], so skip semantics have edge cases | Direct-start needs a **named post-boot state definition** (registers, IO, IWRAM bytes, stack pointers). The exact official post-boot state is only knowable by running the official BIOS privately. | | MEDIUM | Define state S explicitly; label it `[INF]` until cross-checked with an owner-private official-BIOS run. |

**Homebrew/diagnostic paths without any external BIOS [INF]:** polling-only bare-metal ROMs (option A) and, with option B/C, interrupt-driven ROMs. Standard toolchain homebrew almost always calls SWIs, so it needs B, C, or E. Header-validation behavior is a *loader policy* under direct-start (the BIOS check is skipped), and should be reported as diagnostics, with refusal only under `full-boot` plus a real BIOS provider.

**Proprietary test ROMs [INF]:** NBA and SkyEmu cite the "AGS Aging" cartridge as an accuracy oracle [FACT]. It is not an open fixture; treat it as an owner-private, non-redistributable oracle (rights gate) and never as CI content.

---

## Q4. Cartridge and backup scope

### Format facts [FACT, GBATEK]

- Header: first 192 bytes of ROM. Entry point (0x00), Nintendo logo (0x04, 156 bytes, "required"), title (0xA0), game code (0xAC), maker (0xB0), **fixed 0x96 at 0xB2**, main unit code 0xB3 (00h), device type 0xB4, software version 0xBC, **complement check at 0xBD**.
- Game Pak ROM maps at 0x08000000-0x0DFFFFFF as three wait-state mirrors of up to **32 MiB**. **EEPROM carts** map the chip at the top of the ROM region (0x0DFFFF00-0x0DFFFFFF, or anywhere in 0x0D000000-0x0DFFFFFF for ROMs ≤ 16 MB), so ROM is limited to "32 MB minus 256 bytes" with EEPROM.
- **The header does not declare the backup type.** Detection is by ID strings in the ROM (word-aligned, multiple of 4 bytes): `EEPROM_Vnnn` (512 B or 8 KiB), `SRAM_Vnnn` (32 KiB), `FLASH_Vnnn` / `FLASH512_Vnnn` (64 KiB), `FLASH1M_Vnnn` (128 KiB). Nintendo's tools insert them; "when using other tools, you may insert ID strings by hand". GBATEK says No$gba auto-detects most types without strings except 128K flash, which defaults to 64K without `FLASH1M_V`.
- **EEPROM**: 512 B (6-bit addressing, units of 64 bits) vs 8 KiB (14-bit, 10 bits used). GBATEK states "there seems to be no autodetection mechanism, so a hardcoded bus width must be used." Access is serial via **DMA3 only**, 68-bit reads, ~6.5 ms writes, 8 wait states.
- **Flash**: 64K (512 Kbit) or 128K (1 Mbit, two 64K banks selected by command `AAh,55h,B0h` then bank write). Chip ID in ID mode (`E000000h`/`E000001h`): SST D4BF, Macronix 1CC2, Panasonic 1B32, Atmel 3D1F (64K); Sanyo 1362, Macronix 09C2 (128K). Sector erase 4 KiB (except Atmel: 128-byte erase-write). Read/ID must be from WRAM-resident code.
- **SRAM/FRAM**: 32 KiB, 8-bit bus, 8 wait states. FRAM is functionally SRAM-class (no battery).
- `SRAM_F_V` (FRAM ID string): [UNK], not in GBATEK's list; verify before treating as a distinct type.

### How current emulators actually detect, and how reliable it is

| Method | Used by | Reliability |
|--------|---------|-------------|
| ID-string scan of ROM | Nintendo tooling convention; GBATEK recommends; Gamebloke could use as an *advisory* | [FACT] Convention, not requirement; hand-built ROMs may lack it; 128K flash ambiguity without `FLASH1M_V`. Misleads if a string is left over from an unused library. |
| **Runtime access-pattern detection** | **mGBA** [FACT, source]: save type starts `AUTODETECT`; an access to the SRAM region resolves to SRAM, or to **Flash if the first byte write hits `SAVEDATA_FLASH_BASE`** (0x5555); a DMA write to the ROM2-EX region resolves to **EEPROM**; EEPROM starts 512 B and **upgrades lazily to 8 KiB** when an access reaches beyond; Flash starts 64K and upgrades on bank switch | Works for games that behave normally; can misfire for oddball access patterns; mGBA's own README says "flash size detection fails in some cases" [FACT] |
| Per-game override table keyed by 4-char game code | mGBA: 104 compiled-in entries (savetype, hardware like RTC/light sensor, idle loop) plus an SQLite game DB (changelog: "Move game database from flatfile to SQLite3") [FACT] | Accurate for listed titles; a maintained database, i.e. unbounded manual curation. Wrong entries produce wrong saves. |
| Flash chip identity | mGBA emulates Panasonic MN63F805MNP (64K, 1B32) and Sanyo LE26FV10N1TS (128K, 1362) [FACT] | Corroborated by GBATEK's ID table. Games that probe for specific IDs may fail on others. |

**Reliability conclusion [INF]:** no detection method is authoritative. This matches the project's rule: *save type is never inferred from a filename or title*. Gamebloke should (1) require the cartridge profile/manifest to **declare** the backup device and chip, (2) offer ID-string scanning only as a *reported advisory* ("ROM contains `FLASH1M_V111`; declared: flash 128K; consistent") and refuse or flag conflicts, and (3) never ship a game-code database in v1.

### v1 vs deferred

| Item | v1? | Complexity | Notes |
|------|-----|------------|-------|
| ROM image validation: size bounds (≥ header, ≤ 32 MiB), alignment/size diagnostics, truncated/overflow refusal with state preservation, header field diagnostics (logo, 0x96, complement, game code) | **Yes** | LOW | Under direct-start, header checks are reported diagnostics, not hardware lockups. |
| ROM mirroring/open-bus above ROM end, wait-state regions, prefetch interaction | **Yes** (part of bus model) | MEDIUM-HIGH | Belongs with CPU/bus work, flagged here only as a cartridge dependency. |
| No backup device | **Yes** | LOW | First slice. |
| SRAM 32 KiB (and FRAM as SRAM-class, labelled) | **Yes**, first | LOW | Export/import byte-for-byte; host owns file policy. |
| EEPROM 512 B and 8 KiB (DMA3 serial protocol, width declared) | **Yes**, v1 completion | MEDIUM | Requires DMA3 bus-width/length interaction to be right; needs a source-built test cart. |
| Flash 64K and 128K with command state machine, chip ID, 4 KiB sector erase, bank switch, busy/status | **Yes**, v1 completion | MEDIUM-HIGH | Pick one declared chip ID per size (Panasonic 1B32 / Sanyo 1362 as mGBA does, corroborated by GBATEK); other chip IDs are a declared parameter, deferred. |
| Atmel 128-byte-sector flash (3D1F) | Deferred | MEDIUM | GBATEK says Nintendo stopped using Atmel in newer games. Promote on a fixture or declared title needing it. |
| ID-string scanner as advisory | Yes (small) | LOW | Reports only. |
| Game-code/title save database | **No** | MEDIUM + permanent curation | Anti-feature in v1. |
| Multicart/bootleg carts, MBC-like GBA mappers (mGBA 0.11 adds "initial support for bootleg GBA multicarts") | Deferred | MEDIUM | No fixture, no rights story. |
| Multiboot images (loaded to EWRAM) | Deferred | LOW-MEDIUM | Useful for fast diagnostic loading; header at 0x02000000; promote if a diagnostic workflow wants it. |

**Fixture note [FACT]:** jsmolka/gba-tests (MIT) includes `save/` with `sram`, `flash64`, `flash128`, `none` sources and ROMs; per-fixture rights review still applies. Source-built original carts remain the plan.

---

## Q5. Accessories and specialty devices: defer, with promotion evidence

| Device | Deferred? | Complexity | What it needs | Promote when… (evidence) |
|--------|-----------|------------|---------------|--------------------------|
| **RTC / cartridge GPIO** (Boktai, Pokémon Gen 3, Rune Factory-class) | Defer, **first candidate** | MEDIUM | GPIO port at ROM 0x080000C4-C8, RTC serial protocol, a deterministic *host-injected* clock (never wall clock inside core) | A named fixture needs it AND the project defines a timeline-driven clock input; or a profile decision to support a game family. mGBA/NBA/SkyEmu all emulate it [FACT], so differential evidence exists. |
| **Solar sensor** (Boktai) | Defer | MEDIUM | GPIO + A/D ramp conversion (GBATEK: 12-bit counter, lower 8 bits used); sensor value as a timeline input | After RTC/GPIO, with a source-built or licensed GPIO fixture. |
| **Tilt / gyro** (WarioWare Twisted, Yoshi's Universal Gravitation) | Defer | MEDIUM | Analog ADC via GPIO, host-supplied axis values; mirrored X/Y depending on cartridge slot (GBATEK) | Same pattern: fixture + input timeline vocabulary extension. |
| **Rumble** | Defer | LOW (output-only event) | A rumble event channel in the output contract | Cheap; promote with RTC/GPIO since it shares the GPIO port. |
| **Link cable / SIO modes** (normal, multiplayer, UART, Joybus) | Defer | HIGH | Multi-instance scheduling, cable topology, transport; mGBA has local link only, networked/wireless "planned" [FACT] | A concrete two-instance deterministic test with a source-built diagnostic; SIO register read/timing behavior can be tested single-instance earlier (mGBA suite has `sio-read`/`sio-timing`). |
| **Wireless Adapter** | Defer, likely never in v1 | VERY HIGH | Proprietary protocol; mGBA lists it as planned [FACT] | An owner use case plus a legal clean-room protocol source. |
| **e-Reader** | Defer | HIGH | Dotcode scanning hardware, flash, GPIO; mGBA implements and fixed it in 0.10.x/0.11 [FACT] | An owner use case plus a rights-cleared card dataset. |
| **GB/CGB backward-compat mode** | Defer (owner decision already) | HIGH | A separate 8-bit CPU/mode, selected at boot via L/R and CGB bootrom (GBATEK); GabbaBoy owns GB/CGB; Micro does not support it | Explicit owner decision and a profile; reuse of GabbaBoy is not automatic. |
| Game Boy Player (GameCube) rumble/detection | Defer | MEDIUM | Special handshake | Only with an owner use case. |

Nothing above is silently in v1. The input/output contract should leave **named extension slots** (timeline events for clock, sensor, link; output events for rumble) so promotion does not require an API break. [INF]

---

## Q6. Developer and integrator features

| Feature | v1? | Complexity | Rationale for this project's verification goals |
|---------|-----|------------|-------------------------------------------------|
| **Headless runner** (fresh instance, bounded advance, exit codes distinguishing missing/skipped/harness-failed/timed-out/cancelled/guest-failed) | **Yes** | MEDIUM | The core of the differentiator. |
| **Input timeline / replay** (text or JSONL of logical key states keyed to explicit machine-time boundaries) | **Yes** | LOW-MEDIUM | Required by the core value. Do not adopt BizHawk `.bk2`, mGBA, or VBA movie formats. |
| **Semantic results** (JSON: guest progress, machine time, selected register/memory assertions, "proves/does-not-prove") | **Yes** | MEDIUM | Public suites report on screen only [FACT]; Gamebloke needs a result protocol plus screen-grid/guest-memory extraction for third-party suites, and its own authored diagnostics should write results to a defined memory location (compare mGBA's `mgba.c` debug-log channel in its suite [FACT]). |
| **Raw A/V capture** (frames as raw/PPM/PNG-from-raw, PCM as raw/WAV; no codec) | **Yes** | LOW | Core says raw video/audio observed separately. No FFmpeg dependency. |
| **Guest-time event trace** (minimal v1: interrupt, DMA start/end, timer overflow, HBlank/VBlank, backup-device ops, with machine-time stamps; opt-in) | **Yes, minimal** | MEDIUM | The second half of the differentiator; cheapest if designed in with the scheduler. Full instruction/bus trace is v1.x. |
| **Internal snapshot/restore** (test instrument: save at boundary, restore, compare continuation) | **v1.x**, after state layout stabilizes | MEDIUM-HIGH | Catches missing state; the project's guidance already says to test in-flight operations. No cross-version format promise. |
| User-facing save-state files and slots | Defer | MEDIUM | Needs format/identity/versioning decisions; promote with an interactive host. |
| Rewind | **Defer** | MEDIUM (needs snapshot) | A player feature, not a verification need. |
| GDB remote stub | **Defer** | MEDIUM | mGBA already provides one for differential debugging [FACT]; promote when a debugging need justifies the surface. Design core so a stub can attach (step, break, read/write regs/memory). |
| Video/GIF capture via FFmpeg | **No (anti-feature)** | MEDIUM + dependency | Violates the small-dependency rule; raw capture + external tools instead. |
| Cheats (Action Replay, GameShark) | **No** | MEDIUM | Not a verification goal; SkyEmu/mGBA have them. |
| Fast-forward, turbo, frame skip, shaders, color correction, LCD ghosting | **No in core** | n/a | Host presentation concerns; NBA offers color correction and LCD ghosting as host post-processing [FACT]. |
| Lua/scripting engine | **No** | HIGH + dependency | The runner's declarative timeline + result protocol replaces it; mGBA 0.11 and BizHawk already cover scripting. |

---

## Q7. First host and installed consumer

| Option | Honest proof it gives | Cost | Verdict |
|--------|-----------------------|------|---------|
| **macOS arm64 CLI runner + a separate relocatable C consumer built against `cmake --install` prefix** | Proves package layout, headers, runtime loading, relocation, refusal postconditions, on the owner's actual machine (Darwin 25.6 / macOS 26 arm64). LibFuturo's local route already follows this pattern (installed real-core sessions, relocated SDK check) [OBS]. | LOW | **Recommend.** |
| Add a **Linux x86_64 container** job that runs the same determinism manifest and requires byte-identical machine outputs | Cheap proof that the "portable, host-free core" claim is not an arm64/macOS accident (endianness, UB, uninitialized memory, libc differences) | LOW-MEDIUM | **Recommend**, once the first deterministic scenario exists. [INF] |
| Interactive window host (AppKit or SDL) | Needed for the owner's *subjective* "is this playable" review and any human demo; a simulated controller or raw capture does not qualify it | MEDIUM | Second host, after the headless proof. Physical controller qualification stays a human gate. |
| Cross-platform GUI/Windows/browser | Marginal early value | HIGH | Defer. |
| Playstead integration | Out of scope unless separately decided; Playstead currently drives a GUI process and has no library seam for Gamebloke | n/a | Not assumed. |

**Honesty note:** an installed-consumer proof covers packaging and API semantics only. It does not qualify hardware accuracy, a physical controller, or Playstead fit. [OBS-aligned]

---

## Feature Landscape (template view)

### Table stakes: what users of GBA emulators expect (and Gamebloke v1 stance)

Missing these makes a *consumer* emulator feel broken. Gamebloke v1 is deliberately **not** a consumer emulator, so only the machine-level rows are v1.

| Feature | Why expected | Complexity | Gamebloke v1 stance |
|---------|--------------|------------|---------------------|
| ARM7TDMI ARM + Thumb execution | Nothing runs without it | HIGH | **Build**: profile-defined timing precision |
| Memory map, wait states, Game Pak prefetch, open bus | Needed for correctness of timing-sensitive games and tests | HIGH | **Build** |
| Interrupts, timers (4), DMA (4) with arbitration | Games depend on them | HIGH | **Build** |
| PPU: six display modes, sprites, affine BG/OBJ, windows, blending, mosaic, raster timing | Visual correctness | HIGH | **Build**, raw output separate from scaling |
| Audio: 4 PSG channels + 2 Direct Sound FIFOs, SOUNDBIAS | Audio correctness | HIGH | **Build**, raw samples |
| Keypad input | Playing | LOW | **Build** as timed logical input |
| Backup media: SRAM, Flash, EEPROM | Saves | MEDIUM | **Build** (see Q4) |
| BIOS handling (boot + SWIs + IRQ vector) | Most software needs it | MEDIUM-HIGH | **Build** per Q3 |
| Game window, audio playback, controller mapping, file picker, fullscreen | Using it as a player | MEDIUM | **Defer to host adapter** |
| Save states, rewind, fast-forward | Player convenience | MEDIUM | Defer (see Q6) |
| Cheats, shaders, filters, color correction, translations | Player convenience | MEDIUM | Not in core |
| Retail compatibility breadth | The real benchmark users apply | VERY HIGH | Not claimed; stated per profile/evidence |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Deterministic fresh-instance runner with timed logical input** | Repeatable answers; separates guest progress, machine time, device effects, raw video, raw audio | MEDIUM | Depends on timeline format, scheduler in guest-time units |
| **Semantic result protocol with distinct failure classes + "proves / does not prove"** | Prevents "runner didn't run it" from passing; ends hash-only evidence | MEDIUM | Needs diagnostic ROM result convention and extraction for third-party suites |
| **Guest-time event trace** | Debug and differential comparison against mGBA/NBA without GUI | MEDIUM | Design with scheduler on day 1 |
| **Evidence-labelled claim tuple per profile** | Every claim names machine, boot, BIOS provider, cart, backup, evidence | LOW | Pure process; high trust value |
| **Explicit cartridge/backup declaration with advisory-only detection** | Avoids the silent wrong-save-type class of bug | LOW-MEDIUM | Contrast with title-database approaches |
| **Strict public API postconditions** (lifecycle, buffer ownership, refusal postconditions) | Embeddability integrators can trust | MEDIUM | LibFuturo lessons apply |
| Cross-OS byte-identical output as a gate | Real portability claim | LOW-MEDIUM | Linux CI cross-check |

### Anti-Features

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Bundling a BIOS dump, commercial ROM, or private saves | "Works out of the box" | Rights boundary; a hash proves identity not permission | User-supplied official BIOS, project-authored stub, or open replacement built from source with manifest |
| Title/game-code save database in v1 | mGBA-style "just works" | Unbounded curation, wrong entries yield wrong saves, contradicts "never infer from title" | Declared device + advisory ID-string scan |
| Single aggregate "compatibility %" or "passes AGS" headline | Marketing | Hides coverage gaps; AGS Aging is proprietary [INF] | Per-test, per-profile evidence ledger |
| Treating HLE BIOS as equivalent to real BIOS | Speed, no BIOS needed | Divergent semantics, stubs (mGBA logs stubbed SWIs) | Name the BIOS provider in every claim |
| Embedded scripting engine (Lua) | mGBA/BizHawk parity | Large dependency, nondeterminism risk, upstream already covers it | Declarative timeline + result protocol |
| FFmpeg/GIF/video recording in core | Demo convenience | Heavy dependency | Raw A/V capture + external tools |
| Wall-clock RTC or host-dependent anything inside core | Easy RTC | Breaks determinism | Host-injected timeline clock |
| "Passes because the runner didn't run it" | Convenience in CI | Hidden missing/skipped tests | Distinct result classes; negative controls |
| GB/CGB mode "for free" | GBA hardware includes it | Separate CPU/mode; GabbaBoy owns it | Explicit owner decision and profile |
| Replace Playstead's mGBA route | Narrative | Not owner-decided; no evidence of need | Measure value beside mGBA; integration is its own decision |

## Feature Dependencies

```
Named profile (agb) + claim tuple
    └──requires──> Boot/BIOS policy (direct-start state S, provider field)
                       └──requires──> BIOS provider option A, then B/C/E

Canonical machine core (CPU, bus, scheduler in guest-time units)
    ├──requires──> Cartridge ROM validation + backup device models
    ├──requires──> Interrupts / timers / DMA / video+audio event scheduling
    └──requires──> PPU raw output, audio raw samples

Headless runner ──requires──> Core + input timeline + result protocol
    ├──requires──> Fixture manifest (source, rev, recipe, digest, license, BIOS mode, expected)
    ├──enhances──> Guest-time event trace (needs scheduler events)
    └──requires──> Original/rights-reviewed diagnostic ROM(s)

Public C API boundary ──requires──> Core + ownership/refusal postconditions
    └──requires──> Installed relocatable consumer (macOS arm64) ──enhances──> Linux determinism cross-check

Internal snapshot/restore ──requires──> Stable core state layout
    └──enhances──> Determinism tests; later enables rewind and user save states

Interrupt-driven playable homebrew ──requires──> BIOS option B or C (IRQ vector in BIOS)
EEPROM / Flash models ──require──> DMA3 + bus-width model; source-built backup test carts
RTC / GPIO / solar / tilt / rumble ──require──> Cartridge GPIO model + timeline extension slots
GDB stub ──requires──> Core with step/break/register access hooks
Wireless Adapter / link ──require──> Multi-instance scheduling (not before single-instance SIO tests)
LibFuturo GBA profile ──requires──> Real core + consumer proven first
```

### Dependency Notes

- **Interrupts require BIOS code:** the IRQ vector sits in BIOS, so the first interrupt-using diagnostic forces a BIOS provider decision (option B at minimum).
- **Result protocol precedes broad suite adoption:** third-party suites report on screen only, so admitting them needs extraction work and a per-fixture rights review.
- **Trace must be designed with the scheduler:** retrofitting guest-time events into a coarse loop is expensive.
- **Snapshot precedes rewind/save states;** neither is v1.

## MVP Definition

### Launch With (v1: first profile `agb` + `direct-start`)

- [ ] Profile and claim-tuple model; one machine profile; support claims tied to evidence
- [ ] ROM validation + header diagnostics; no-backup and SRAM 32 KiB first
- [ ] ARM/Thumb core, bus with wait states and prefetch at profile-defined precision, scheduler in guest-time units, interrupts/timers/DMA
- [ ] PPU raw output (six modes as the profile requires) and PSG + FIFO raw audio
- [ ] Timed logical input timeline
- [ ] BIOS option A (none) then B (project-authored stub) for an interrupt-driven scenario; optional user-supplied official BIOS slot with digest check
- [ ] Headless runner, semantic result protocol, raw A/V capture, minimal guest-time trace
- [ ] Fixture manifest; one original polling-only diagnostic (no BIOS) and one legally cleared playable scenario
- [ ] EEPROM 512 B/8 KiB and Flash 64K/128K with declared chips and source-built test carts
- [ ] Public API boundary + installed relocatable macOS arm64 consumer + Linux determinism cross-check
- [ ] Canonical verification command with negative controls

### Add After Validation (v1.x)

- [ ] Open-replacement BIOS fixture (Cult-of-GBA at a pinned commit) with manifest: trigger = a homebrew/test needs SWIs beyond the stub
- [ ] Internal snapshot/restore as a test instrument: trigger = core state layout stable
- [ ] Full instruction/bus trace: trigger = first differential investigation against mGBA/NBA
- [ ] Interactive window host: trigger = owner subjective playability review
- [ ] RTC/GPIO (+ rumble): trigger = a fixture or profile decision

### Future Consideration (v2+)

- [ ] HLE BIOS: only if a named workload needs it
- [ ] Solar, tilt/gyro, Atmel flash, multiboot, multicart
- [ ] Link/SIO multi-instance, Wireless Adapter, e-Reader
- [ ] GB/CGB mode (explicit owner decision)
- [ ] GDB stub, user save states, rewind
- [ ] LibFuturo GBA profile (after real core + consumer)
- [ ] `agb-micro` profile (needs hardware-derived expectation)

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Deterministic headless runner + semantic results | HIGH | MEDIUM | P1 |
| Core CPU/bus/scheduler/PPU/APU | HIGH | HIGH | P1 |
| Timed input timeline | HIGH | LOW | P1 |
| Raw A/V capture | HIGH | LOW | P1 |
| BIOS option A + B | HIGH | LOW-MEDIUM | P1 |
| SRAM; EEPROM; Flash | HIGH | LOW / MEDIUM / MEDIUM-HIGH | P1 (SRAM first, others v1 completion) |
| Minimal guest-time trace | MEDIUM-HIGH | MEDIUM | P1 |
| Fixture manifest + claim tuple | HIGH | LOW | P1 |
| Installed consumer (macOS) + Linux cross-check | HIGH | LOW-MEDIUM | P1 |
| Optional official-BIOS slot | MEDIUM | LOW | P2 |
| Open-replacement BIOS fixture | MEDIUM | MEDIUM | P2 |
| Snapshot/restore (test instrument) | MEDIUM | MEDIUM-HIGH | P2 |
| RTC/GPIO, rumble | MEDIUM | MEDIUM | P2 |
| Interactive window host | MEDIUM | MEDIUM | P2 |
| HLE BIOS, solar, tilt, Atmel flash | LOW-MEDIUM | MEDIUM | P3 |
| Link, Wireless, e-Reader, GB/CGB, GDB, rewind, save states | LOW (for this project's goals) | HIGH | P3 |

## Competitor Feature Analysis

| Feature | mGBA | NanoBoyAdvance | SkyEmu | Gamebloke approach |
|---------|------|----------------|--------|--------------------|
| License | MPL-2.0 | GPL-3.0 (GitHub/Codeberg master); BSD-2-Clause on `phoenix` branch | MIT | Undecided (owner gate); no code copying meanwhile |
| Headless/automation | 0.10.5: no; 0.11 (unreleased): headless frontend, `--script`, startup scripts | None mentioned | HTTP control server, `/step`, `/input`, `/read_byte`, `/screen` | Runner is v1; **differentiate on the determinism and result contract** |
| Determinism guarantee | Not stated | Not stated | Explicitly none | Stated, tested, cross-OS |
| Debugger | CLI debugger, GDB stub, scripted breakpoints (0.11) | Viewers (palette, tiles, BG, OAM) | CPU/MMIO/memory debuggers | Event trace v1; GDB later |
| BIOS | Built-in HLE-style image + optional external; BIOS checksum recognition | External required; open BIOS optional | Bundled open stubs + official recommended | Provider is an explicit claim field; none/stub/open/official |
| Save type handling | Runtime access-pattern detection + 104 overrides + SQLite DB | Not verified | Not specified | Declared device; advisory scan only |
| Accuracy | Strong, still fixing DMA latching in 0.11 | Cycle-accurate most components; AGS pass claim | Claims AGS pass, 2020/2020 timing tests with official BIOS | Per-profile evidence ledger; not a headline |
| Accessories | RTC, solar, tilt, rumble, local link, e-Reader | RTC, solar | RTC, solar | Deferred with promotion evidence |
| Embeddability | libmgba shared/static, Python | Not described | Not described | C library with strict postconditions |

## Owner-gate questions that remain

1. **Differentiator wording (material product direction).** Confirm "deterministic, evidence-labelled machine verification" as the stated outcome, or choose another. Not blocking.
2. **Project license and clean-room posture.** NBA is GPL-3.0 (frozen/Codeberg `master`) or BSD-2-Clause (`phoenix`); mGBA is MPL-2.0. Until the license is chosen, this research recommends reading references for behavior only and copying no code.
3. **BIOS authoring.** Approve a project-authored minimal boot stub (IRQ trampoline + SWI subset, no Nintendo bytes) and, later, a pinned MIT open replacement (Cult-of-GBA/BIOS) as a fixture with license notices. Both need the fixture manifest and an owner rights review.
4. **Playable homebrew fixture.** Original owner-authored vs a third-party licensed title (rights gate); "playable" judgment is a human gate.
5. **Proprietary oracles.** Whether the owner will privately use the AGS Aging cartridge or an official BIOS dump as non-redistributable oracles (never in CI/source control).
6. **Physical hardware.** Any Micro-vs-AGB claim, or hardware-derived timing vectors, needs owner hardware and authority.
7. **Whether a game-code database is ever acceptable** (this research says no for v1).

Routine and reversible (no gate): first profile name `agb`, direct-start, backup tiering, macOS-first host, Linux cross-check, deferring accessories.

## Stale or wrong bootstrap claims and re-verifications

| Bootstrap claim | Finding |
|-----------------|---------|
| Nintendo sales: 81.51M hardware, 377.42M software | **Verified correct** on the Nintendo IR page as of September 30, 2026; "worldwide consolidated, life-to-date". The page does not break out SP or Micro [FACT]. |
| "NanoBoyAdvance hw-test … BSD-3-Clause", linked at GitHub | License still BSD-3-Clause (Codeberg `LICENSE`, (c) 2021 fleroviux), but **GitHub hw-test is a frozen migration stub; development is on Codeberg** (last commit seen 2026-08-10). Update links. Per-fixture rights still unreviewed. |
| NanoBoyAdvance linked as GitHub project | GitHub is archived/frozen (README: development moved to Codeberg). Codeberg `master` LICENSE is GPL-3.0; `phoenix` branch relicensed BSD-2-Clause (commit 2026-07-26). The README on GitHub still says GPL-3.0. |
| mGBA test suite MIT; jsmolka gba-tests MIT | **Verified** (MIT, (c) 2015 Jeffrey Pfau for mGBA suite). Both report results visually/on screen, not as a machine-readable stream. |
| "mGBA … useful differential/debugging implementation" | Correct and understated: **master has a headless frontend and startup scripts not in the pinned 0.10.5**. Playstead's blocked capability may not persist. |
| "AGB uses an ARM7TDMI at about 16.78 MHz" | Nintendo's public technical-data page only says "32-bit processor"; ARM7TDMI and the clock figure come from the AGB manual/GBATEK, not Nintendo's public page. Not wrong, but source-labelled differently. |
| "BIOS/compatibility: HLE is a viable option to study" | Confirmed; and the sharper fact is that interrupt delivery itself requires BIOS code, so "no BIOS" must mean "no *external* BIOS image" and still needs a vector provider. |

## Sources

- Playstead (read-only): `~/projects/playstead/.planning/PROJECT.md`, `REQUIREMENTS.md`, `ROADMAP.md`, `phases/06-recovery-proof-ci-and-e2e-pipeline/06-CONTINUATION-QUALIFICATION.md`, `06-13-SUMMARY.md`, `spikes/001-bios-free-gameplay/README.md` [OBS]
- LibFuturo (read-only): `~/projects/libfuturo/README.md`, `docs/native-macos-host.md` [OBS]
- [mGBA repository and README](https://github.com/mgba-emu/mgba) (HIGH: official)
- [mGBA CHANGES, master](https://raw.githubusercontent.com/mgba-emu/mgba/master/CHANGES) (HIGH)
- [mGBA headless-main.c, master](https://raw.githubusercontent.com/mgba-emu/mgba/master/src/platform/headless-main.c) (HIGH)
- mGBA source: `src/gba/{savedata,memory,dma,overrides,gba,bios,hle-bios}.c`, `CMakeLists.txt` on master (HIGH)
- [mGBA releases](https://github.com/mgba-emu/mgba/releases) and [downloads](https://mgba.io/downloads.html) (HIGH)
- [mGBA scripting API](https://mgba.io/docs/scripting.html) (HIGH; generated from 0.10.2)
- [mGBA test suite](https://github.com/mgba-emu/suite) and [jsmolka gba-tests](https://github.com/jsmolka/gba-tests) (HIGH for license/layout)
- [NanoBoyAdvance on Codeberg](https://codeberg.org/nba-emu/NanoBoyAdvance), [GitHub (archived)](https://github.com/nba-emu/NanoBoyAdvance), [hw-test on Codeberg](https://codeberg.org/nba-emu/hw-test) (HIGH)
- [SkyEmu README](https://github.com/skylersaleh/SkyEmu) and [HTTP control server doc](https://raw.githubusercontent.com/skylersaleh/SkyEmu/dev/docs/HTTP_CONTROL_SERVER.md) (HIGH)
- [ares repository](https://github.com/ares-emulator/ares) and [v144 release notes](https://ares-emu.net/news/ares-v144-released) (MEDIUM: GBA specifics thin)
- [BizHawk](https://github.com/TASEmulators/BizHawk), [gdkGBA](https://github.com/gdkchan/gdkGBA), [Mesen2 web search summary](https://github.com/SourMesen/Mesen2) (MEDIUM / LOW for Mesen2 GBA)
- [Cult-of-GBA/BIOS](https://github.com/Cult-of-GBA/BIOS) (README, LICENSE, repo metadata) (HIGH)
- [GBATEK, mGBA-hosted markdown fork](https://mgba-emu.github.io/gbatek/) (MEDIUM-HIGH: community reference): cartridge header, backup IDs, EEPROM, Flash, SRAM, GPIO, BIOS interrupt handling, model notes
- [Nintendo IR lifetime sales](https://www.nintendo.co.jp/ir/en/finance/hard_soft/index.html?country=66) and [Nintendo GBA technical data](https://www.nintendo.com/en-gb/Support/Legacy-system/Technical-data-619479.html) (HIGH)

---
*Feature research for: deterministic accuracy-focused GBA emulator (Gamebloke)*
*Researched: 2026-11-09*
