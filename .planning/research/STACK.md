# Technology Stack

**Project:** Gamebloke (deterministic, accuracy-focused GBA emulator; greenfield, no repo yet)
**Researched:** 2026-11-09
**Dimension:** Stack (language/build, ROM toolchain, fixtures, BIOS, homebrew, license, deps/CI)
**Overall confidence:** MEDIUM-HIGH (sibling conventions and upstream licenses verified against current files; legal conclusions are inference and owner gates)

## Evidence labels used below

- **[PF]** primary-source fact: read this session from a current repo file, API, or package index.
- **[OBS]** observation of a local file or a local probe run this session (sibling repos, Homebrew toolchain). True of that file or machine, not of CI.
- **[CR]** community reference: a third-party claim not checked against a primary source.
- **[INF]** inference from facts above it. May be wrong.
- **[UNK]** not determined. Listed so nobody builds on it.

A repo-level license statement is never read as "every embedded file is cleared". Where the two differ, the row says so.

---

## 1. Recommended stack (summary)

### Core technologies

| Technology | Version | Purpose | Why |
|---|---|---|---|
| C, ISO C17, extensions OFF | `CMAKE_C_STANDARD 17`, `C_EXTENSIONS OFF` | Canonical machine core, headless runner, public API | All four inspected siblings use exactly this [OBS]. LibFuturo's public header is a plain C API with `extern "C"` guards [OBS]. C17 builds on GCC, Clang, Apple clang and MSVC without flags that diverge. |
| CMake | `cmake_minimum_required(3.25)`; presets schema **6**; dev box has 4.4.3 [OBS] | Build, CTest, install/package, workflow presets | 3.25 is the first release with workflow presets [PF: cmake.org presets table: schema 6 = 3.25, 11 = 4.3, latest 13 = 4.5 rc]. Cogmaster requires 4.3 and schema 11 [OBS]. Gamebloke's library has no need for that, and a 3.25 floor lets Debian/Ubuntu LTS consumers build it. |
| Ninja | 1.13.x (dev box) [OBS] | Generator | Every sibling preset uses `"generator": "Ninja"` [OBS]. |
| GCC 14/15, Clang 21 (+Apple clang), MSVC `cl` | Linux CI: GCC (cogmaster's nofp lane records "GCC 15 on the hosted image"), Clang 21 for ASan/UBSan/tidy/format [OBS] | Compilers | Mirror the sibling matrix. Add MSVC and arm64 legs in the scheduled lane, not the PR gate (section 7). |
| GNU binutils + GCC for `arm-none-eabi` | **xPack 15.2.1-1.1** (2026-03-03), pinned by SHA-256 per host [PF] | Build ORIGINAL GBA diagnostic ROMs from assembly (and freestanding C if ever needed) | One identical toolchain version on macOS arm64 and Linux, fetched through a `pins.txt` + `fetch-pin.sh` pattern the owner already uses [OBS]. Fallback: Ubuntu apt `binutils-arm-none-eabi` (see section 2). |
| In-tree SHA-256 | ~150 lines, project-owned | Fixture/frame/audio digests | Siblings carry their own `sha256.c` rather than a crypto dependency [OBS: gabbaboy `src/accept/sha256.c`, cogmaster `runner/sha256.c`]. |
| CTest + a 40-line `check.h` | none | Test execution | No framework in cogmaster: "each test is a program whose main ends with CHECK_DONE(), and CTest reads its exit status" [OBS]. |

### Supporting tools and libraries

| Tool | Version | Purpose | When to use |
|---|---|---|---|
| clang-format / clang-tidy | 21 | `hygiene` preset lane | Cogmaster's runner image provides both [OBS]. Add with the first source file. |
| libFuzzer (Clang 21) | 21 | Fuzz ROM loader, header validator, save importer, state codec, input-script parser | Scheduled lane only. Cogmaster's nightly does this [OBS]. Add once those parsers exist. |
| qemu-user (ppc32 BE) + `gcc-powerpc-linux-gnu` | apt | Big-endian "hash-equality" proof | Scheduled lane. Cogmaster compares byte-identical hashes across six builds plus qemu BE [OBS]. Strong determinism evidence for a machine core. |
| wasmtime | pinned LTS (cogmaster pins 48.0.7) [OBS] | wasm32/WASI portability lane | Later. Only after the core links nothing but memcpy-class libc. |
| Docker + `devkitpro/devkitarm@sha256:...` | see section 2 | Build THIRD-PARTY suites that need devkitARM/libgba | Scheduled/deep lane and local on-demand only. |
| Zig | 0.17 (emibios requires `minimum_zig_version 0.17.0`) [PF] | Build emibios from source | Optional, differential BIOS lane only. Do not add to the core toolchain. |
| release-please | `googleapis/release-please-action` v5.0.0, pinned by SHA in cogmaster [OBS] | Versioning/changelog | Use `release-type: simple` with a `version.txt` single source (cogmaster, nesturbator). |

### Install (local dev, macOS arm64, matches the owner's host)

```bash
brew install cmake ninja llvm@21          # cmake 4.x, ninja, clang-21 tools
# ROM toolchain: do NOT use the Homebrew arm-none-eabi-gcc for committed digests.
# Fetch the pinned xPack tarball:   scripts/fetch-pin.sh xpack-arm-none-eabi-gcc-darwin-arm64 tools/arm-gcc
cmake --workflow --preset ci              # one canonical command, same as CI
```

```bash
# CI (ubuntu-26.04), fast gate
sudo apt-get update && sudo apt-get install -y ninja-build   # clang-21/gcc already on image [OBS: sibling comments]
cmake --workflow --preset ci
```

---

## 2. Decision 1: Implementation language, standard, build system

### What the siblings actually do (read-only inspection, [OBS])

| Convention | Where | Reuse for Gamebloke |
|---|---|---|
| `LANGUAGES C`, `CMAKE_C_STANDARD 17`, `C_EXTENSIONS OFF` | gabbaboy, nesturbator, cogmaster, libfuturo CMakeLists | Adopt unchanged. |
| `CMakePresets.json` with hidden `base` (Ninja, `build/${presetName}`), `dev`, `ci` (Release + `CMAKE_COMPILE_WARNING_AS_ERROR=ON`), `ci-msvc`, `asan`, `nofp`, `be32`, `wasm32`, `hygiene`, plus `workflowPresets`. CI runs `cmake --workflow --preset X`, identical to the local command | nesturbator, cogmaster | Adopt. This is the "one canonical local command aligned with the fast CI gate" in PROJECT.md. |
| ASan/UBSan preset: `clang-21 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`, timeout scale 3 | cogmaster `asan` preset; gabbaboy uses an option `GABBABOY_ENABLE_SANITIZERS` limited to Linux | Use the cogmaster form (preset, not an option). |
| Warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes -Wvla -Wundef -Wcast-qual -Wimplicit-fallthrough -Wmissing-variable-declarations` (+GNU `-Wduplicated-cond -Wlogical-op`; MSVC `/W4 /utf-8`), applied only when top-level | cogmaster `cogmaster_warnings()` | Copy the function. `-Wconversion` matters in a bit-twiddling CPU/PPU core. |
| Core links only memcpy/memmove/memset/memcmp; `-fno-stack-protector -U_FORTIFY_SOURCE -ffile-prefix-map`; `C_VISIBILITY_PRESET hidden`; PIC | cogmaster core | Adopt as a target. Pairs with a build-time symbol check (gabbaboy `CheckCoreNoHostClock.cmake`, `CheckHostApiSymbols.cmake`) enforcing "no wall clock, no host files" in the core. |
| Host-supplied allocator callbacks; no hidden global state | cogmaster test `alloc(void *u, size_t n)` | Fits "caller-owned buffers" and independent instances. Specify it in the API phase. |
| `nofp` lane: core compiled with `-mgeneral-regs-only`, abi tests only | nesturbator, cogmaster | Adopt. Machine core must be integer-only; resampling/conversion lives in the host adapter. |
| `be32` lane (qemu-user PPC BE) and "hash-equality" job requiring byte-identical `hashes.txt` across every leg | cogmaster | Adopt in the scheduled lane. Direct proof of the Determinism constraint. |
| Package: static lib, `Config.cmake` + `ConfigVersion.cmake` (`SameMinorVersion` pre-1.0) + `.pc`, relocatable (prefix located from the file's own dir); `tests/consumer/` builds C and C++ consumers; gabbaboy has a `cmake-3.25.3-relocated` preset with `GBB_TEST_INSTALL_PREFIX` | gabbaboy, cogmaster | Adopt: `tests/consumer` (C + C++ main), `tests/install`. |
| Versioning: `version.txt` single source read by CMake; release-please `simple`, `bump-minor-pre-major`, `draft: true`, `force-tag-creation`, `include-v-in-tag` | cogmaster, nesturbator | Adopt. Gabbaboy's `# x-release-please-version` marker inside `CMakeLists.txt` also works, but `version.txt` is cleaner. |
| Tests have no framework; CI proves "every expected test ran" by checking `ctest --output-junit` against an inventory file | cogmaster `tests/inventory/*.txt`, gabbaboy `tests/expected-tests.txt` | Adopt. Directly implements "missing and skipped checks must be visible". |
| Pinned downloads: `pins.txt` rows `name version url sha256` and `scripts/fetch-pin.sh` verifying SHA-256 before unpack | cogmaster | Adopt for every external tool and fixture. |
| `ASSET_POLICY.md`, `PROVENANCE.md`, `hygiene.sh` rejecting ROM/save signatures and unlisted binaries | nesturbator, cogmaster | Adopt before the first fixture lands. |

Note for PROJECT.md accuracy: LibFuturo's LICENSE is **Apache-2.0** [OBS], while the four emulators are MIT. PROJECT.md says "all C/CMake with MIT licenses where present". Fine for the emulators, but do not assume the LibFuturo header is MIT when planning the adapter.

### C vs C++ vs Rust, honestly

| Criterion | C17 | C++20 | Rust (stable, `no_std` core + `extern "C"` facade) |
|---|---|---|---|
| Determinism | Needs discipline: no UB on shifts/overflow, explicit unsigned arithmetic. Enforced by UBSan lane and `-Wconversion`. | Same hazards plus static-init order and implicit conversions. | Strongest: overflow is explicit (`wrapping_*`), no UB in safe code. |
| Untrusted ROM/save/state parsing | Highest risk. Mitigated only by validation plus ASan/UBSan plus libFuzzer. | Same as C, slightly better containers. | Best: bounds-checked by default. This is Rust's real advantage here. |
| Embedding / ABI | Trivial, stable C ABI, hidden visibility, matches LibFuturo directly. | Needs an `extern "C"` facade, avoid exceptions/RTTI across the boundary; STL types never cross. | Needs `#[no_mangle]`, `panic=abort`, a header (cbindgen or hand-written), and CMake integration for an installed, relocatable package. |
| Portability lanes (nofp, be32, wasm32, MSVC arm64) | Already built and proven in cogmaster, copy-paste. | Mostly works, more surface. | wasm32 is excellent; nofp/be32/MSVC lanes would be rebuilt from scratch. |
| Portfolio reuse | Highest: CI shape, `check.h`, runner, sha256, png writer, fetch-pin, hygiene scripts are C/CMake/sh. | Low-medium. | Low: almost none of the harness transfers. |
| Dependencies | None needed. | None needed. | Zero crates is achievable but `cargo` + toolchain pinning becomes a new supply-chain surface. |
| Performance | Equal. Performance is not a deciding factor here and is out of scope until correctness baselines exist. | Equal. | Equal. |

**Recommendation: C17 + CMake (3.25 floor, preset schema 6) + Ninja.** Confidence: **HIGH** for C over C++; **MEDIUM** against Rust.

Why: the decisive factors are portfolio reuse (an enormous, already-debugged CI/QA harness), a C public API that LibFuturo consumes natively, and the owner's stated constraint of a small flat dependency tree. Rust's memory-safety advantage over untrusted ROM bytes is real and is the reason this is MEDIUM. The C-specific mitigation is part of the stack, not an afterthought: ASan/UBSan preset in the PR gate, libFuzzer in the scheduled lane against every parser, and a loader that validates lengths before allocation (PROJECT.md Security).

Revisit trigger [INF]: if the first fuzz campaigns keep finding memory-safety defects in the loader/state codec after the validators are written, reconsider a Rust loader behind the same C ABI. Do not start with a mixed-language core.

**What NOT to use:** C++ for the core (ABI and static-init surface with no gain over C); C23 (`-std=c2x`) because MSVC and older Apple toolchains lag and siblings stay on C17; GNU extensions (`gnu17`); autotools/Meson/Bazel (nothing reusable); Python as a hard build dependency (gabbaboy's fixture-admission uses `python3 -I`, cogmaster uses `cmake -P` and POSIX `sh`. Prefer the latter so Windows lanes stay simple).

---

## 3. Decision 2: Toolchain for building ORIGINAL diagnostic/homebrew ROMs in CI

### Options evaluated

| Option | License (as verified) | CI-installable reproducibly? | Verdict |
|---|---|---|---|
| **GNU binutils/GCC `arm-none-eabi`, xPack 15.2.1-1.1** | Packaging repo MIT [PF: GitHub API]; contents are GPL/LGPL toolchain components (build-time tools only; output ROM is not a derivative of the compiler; libgcc has the GCC Runtime Library Exception [CR: standard GCC terms]) | **Yes.** GitHub release with per-host tarballs and SHA-256 digests: `darwin-arm64 574082d3...`, `darwin-x64 5be906a5...`, `linux-x64 da6a49ad...`, `linux-arm64 67980c79...` [PF: release assets `digest` field]. Same shape as cogmaster's wasmtime rows. | **Recommended.** |
| Ubuntu apt `binutils-arm-none-eabi` / `gcc-arm-none-eabi` | GPL tool packages | Reasonably. Ubuntu 24.04 = 13.2.rel1, **26.04 = 14.2.rel1**, 26.10 = 15.3.rel1 [PF: packages.ubuntu.com]. Version differs by runner image and from macOS. | **Fallback.** Cogmaster's policy tolerates unpinned apt tools: "signed, and drift fails loudly" [OBS]. Acceptable for assembly-only ROMs. |
| Arm GNU Toolchain tarballs | GPL tool components | Tarballs exist (15.2.rel1 and 14.3.rel1 return 302 to the blob host; **15.3.rel1 x86_64 path returned 404**) [OBS]. `developer.arm.com/downloads/-/arm-gnu-toolchain-downloads` now 301-redirects to a new `gitlab.arm.com/tooling/gnu-toolchains-for-arm` project (created 2026-04-28, 0 tags) [PF]. Hosting is in flux. | Do not pin to it. xPack tracks Arm releases from GitHub. |
| **devkitARM / devkitPro** r67 (gcc 15.2.0, binutils 2.45.1, 2026-01-12) [PF: buildscripts release notes]; a "release 68" commit exists 2026-06-10 but its contents are not in GitHub Releases [PF/UNK] | Mixed: `libgba` = LGPL-2 **with exceptions**, static linking allowed, but "programs must still identify their use of libgba" ("[program/work] is based in part on the work of devkitPro") [PF: `libgba_license.txt`]; `gba_crt0.s` = MPL-2.0 [PF]; `gba-tools` (gbafix) = GPL-3.0 [PF]; `docker` repo = GPL-3.0 [PF]; `libtonc` repo has no LICENSE but a `license.txt` in the gbadev-org mirror is MIT, (c) J Vijn [PF]. devkitPro's own redistribution/mirroring terms for its pacman repo and Docker Hub images: **[UNK]**. The site returned HTTP 403 to every fetch. | Only via Docker image by digest. `devkitpro/devkitarm:20260610` = `sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5` (amd64 ~537 MB, arm64 ~519 MB) [PF: Docker Hub API]. The image was pushed 13:53Z, two hours before the r68 buildscripts commit (15:55Z), so whether it holds r67.1 or r68 is **[UNK]**. Record `dkp-pacman -Q` output in each fixture manifest. The apt/pacman repo is mutable and not version-pinned. | **Required only for third-party suites** (mGBA suite, hw-test, gba-pong). Not for Gamebloke-owned fixtures. |
| LLVM/clang `--target=armv4t-none-eabi` + lld | Apache-2.0 with LLVM exception [CR] | Compiles to v4T [OBS probe: llvm@21 emitted no `blx/movw/movt`]. Link not verified: Homebrew llvm@21 ships no `ld.lld` [OBS]. Division needs `__aeabi_uidiv` and `libclang_rt.builtins` for armv4t is not provided [OBS: `llvm-nm` showed `U __aeabi_uidiv`]. | Viable later for a freestanding-C diagnostic, not now. |
| `gba-toolchain` (felixjones) | zlib [PF: LICENSE.md] | CMake toolchain file wrapping Arm GNU/devkitPro, pulls `librom`, tonclib, maxmod, grit via `find_package` [PF: README]. Last push 2026-09-19. | Not needed. A project-owned 100-line crt0 and linker script are smaller than the dependency. |
| Rust `gba` crate (Apache-2.0) / `agb` (MPL-2.0) | per GitHub [PF] | `gba` needs `build-std = ["core"]` on target `thumbv4t-none-eabi` with `arm-none-eabi-ld` [PF: its `.cargo/config.toml`], i.e. a nightly-gated build. `agb` 0.25.0's README lists an mGBA-based `mgba-test-runner` for its tests. | Reject for diagnostics. `agb` is the best-maintained route for *playable* Rust homebrew, which is not a Gamebloke need. |
| Butano (zlib; v21.10.0, 2026-11-07) [PF] | zlib code, but its example games mix asset licenses (section 5) | devkitARM + C++ templates + maxmod | Reject for diagnostics. Heavy. |
| gvasm (0BSD) / armips (MIT) | permissive | gvasm requires Deno [PF]; armips has no release since v0.11.0 (2020) though master is active [PF] | Reject: new runtimes/tools for no gain over GNU `as`. |

### Recommendation

**Assembly-first fixtures built with the pinned xPack `arm-none-eabi` binutils, with a project-owned crt0/header, linker script, and header fixer.** Confidence: **MEDIUM-HIGH**.

- Model the cogmaster fixture pipeline [OBS]: sources in `tests/roms/src/*.s` + `.ld`; the ROM is **built, not committed**; the digest lives in `tests/built/expected.txt`; the fixture "measures and never judges" by writing a RAM mailbox (magic, protocol version, running/done status, record count, CRC) and `tests/diag/expected.csv` holds the verdicts. Reuse that mailbox idea for a GBA diagnostic (EWRAM/IWRAM fixed address, host reads it at a defined machine-time boundary).
- Why assembly: ROM bytes then depend on the assembler, not on compiler code generation. This is a live problem, not a hypothetical one: the mGBA suite's own history has commits "Update hblankBit test with modern gcc values" and "Update DMA latch test to be a bit more resistant to gcc updates" [PF: mgba-emu/suite log, 2026-06-01]. Identity of a C-built fixture is source commit + exact toolchain digest.
- Write a ~50-line `gba-header-fix` (C, built by CMake) that computes the header complement check and optional checksum. Avoid `gbafix` (GPL-3.0 dependency) and `agb-gbafix` (MPL, Rust).
- **Nintendo logo bytes.** The 156-byte logo is zero-filled in devkitPro's crt0 and written in by `gbafix -p` [PF: crt0 source; mGBA suite Makefile calls `gbafix -p`]. jsmolka's header carries the logo bytes [PF]. emibios ships `assets/gba_logo.bin` (156 bytes) [PF]. Do not put those bytes in Gamebloke-owned ROMs by default. Cult-of-GBA's entrypoint contains no logo/checksum validation (grep of `entrypoint.s` found none) [INF: medium], and Gamebloke's own direct boot needs no logo. Whether the logo is cleared for reuse is a rights question for the owner. Treat as an **owner rights gate** if any fixture must boot on a logo-checking BIOS (emibios default build, or real hardware).
- First CI spike (cheap, prevents a bad assumption): assemble+link a trivial ROM, then `readelf -A` and assert `Tag_CPU_arch: v4T` and `Tag_THUMB_ISA_use: Thumb-1`. I observed exactly that on Homebrew GCC 16.2.0 with `-mcpu=arm7tdmi -mthumb`, where the link selected the `thumb/nofp` multilib and resolved `__aeabi_*` division from libgcc [OBS probe]. xPack 15.2.1 and Ubuntu 14.2.rel1 are **not yet verified** to behave the same.
- Local macOS: do not use the Homebrew `arm-none-eabi-gcc` (16.2.0 here [OBS]) to refresh committed digests. A digest mismatch from a different binutils is a toolchain-identity problem, not a regression. The canonical command should report "SKIPPED: pinned toolchain absent" loudly rather than silently falling back.
- Third-party suites needing devkitARM go in a **separate** lane (Docker digest), never gate the PR on a 500 MB image pull.

**What NOT to use for owned fixtures:** devkitPro/libgba/maxmod (attribution and LGPL obligations travel into every ROM binary), Rust nightly toolchains, gbafix, any `-O3 -ffast-math` C build (the mGBA suite itself builds that way [PF: Makefile]) as a recipe for a fixture whose bytes you want stable.

---

## 4. Decision 3: Candidate test fixtures

Revisions below are the HEAD SHAs I read on 2026-11-09 [PF]. Pin by SHA, never by branch. "Stated license" is what the repo's LICENSE says. "Other rights" is everything else I found in the closure.

| Fixture | Stated license (PF) | Other rights / caveats | Build recipe | Result channel | BIOS need | Admission recommendation |
|---|---|---|---|---|---|---|
| **mgba-emu/suite** @ `e6942030d25f` (2026-07-09; 94 commits; no releases) | MIT, (c) 2015 Jeffrey Pfau | `include/mgba.h` and `src/mgba.c` carry a **BSD-2-clause-style header** (2016), not MIT. `gfx/raw/font.bmp` (and other `.bmp`) have **no stated provenance** (single "Initial commit" history). Built ROM links devkitPro `libgba` (LGPL-2+exceptions, attribution) and MPL-2.0 `crt0` and gets the Nintendo logo from `gbafix`. | `make` with `DEVKITARM`, `-lgba`, grit, `gbafix -p`; `-march=armv4t -mthumb -O3 -ffast-math` [PF: Makefile]. Upstream's own recipe is `docker run devkitpro/devkitarm make` (`docker-build.sh`). Output depends on GCC version (see above). | **Not a pass/fail exit.** It is a menu: press A on each of 14 suites (memory, io-read, timing, timers, timer-irq, shifter, carry, multiply-long, bios-math, dma, sio-read, sio-timing, misc-edge, video). Machine channels: (1) `savprintf` appends ASCII result lines to **SRAM** (`SRAM_V123` save-type string embedded), (2) mGBA debug-log registers `0x4FFF600/700/780`, (3) no$gba debug port `0x4FFFA00`, (4) an IWRAM `ActiveInfo` struct with magic "Info" (suite/test/subtest ids). Per-test PASS/FAIL colors are only on screen. | Uses `irqInit()` and `VBlankIntrWait()` throughout, so IRQ dispatch and SWI 5 must exist (BIOS or HLE). `bios-math` and `swi` suites call real BIOS math SWIs; results are BIOS-mode-specific [INF from source]. | **Pinned build in the deep lane, not committed.** Drive it with the timed-input script (A presses) and read the SRAM log. Do not republish screenshots or the built ROM until the font provenance is reviewed. Label every result with BIOS mode. |
| **jsmolka/gba-tests** @ `a7113b67e63f` (2025-04-19; 136 stars) | MIT, (c) 2019 Julian Smolka | Prebuilt `.gba` files are committed. Source font "extracted from tonclib" (tonclib is MIT, (c) 2005-2009 J Vijn [PF: gbadev-org/libtonc `license.txt`]) so notice is needed. Header embeds the **Nintendo logo bytes** [PF: `lib/header.asm`]. `unsafe/` tests are explicitly flagged by the author as unreliable on hardware ("probably caused by the flash card") and some disagree between mGBA, No$GBA and hardware [PF: `unsafe/README.md`]. | FASMARM (freeware, source available, needs FASM) [PF: arm.flatassembler.net]. No Makefile. Rebuild reproducibility unverified [UNK]. Use the committed ROMs by digest. | Pass = idle loop with **r12 == 0**; fail = r12 holds the first failing test number (`m_test_eval`). Also draws "All tests pass" or the number on screen (mode 4). No memory mailbox, no exit code. Read r12 when PC reaches the `idle: b idle` loop. | `arm`, `thumb`, `memory`, `ppu`, `save` ROMs: no SWI in their test sources (grepped); `nes` and `unsafe` not checked [UNK]. Their `m_test_eval` failure path uses `swi 0x60000` (Div) to print a number, so HLE Div is needed to *display* a failure but r12 still carries the result [PF]. **`bios/bios.gba` requires the Nintendo BIOS**: it asserts exact BIOS open-bus values (e.g. 0x0DC+8 after startup) [PF]; a replacement BIOS will fail it by design. | **Best early fixture.** Pinned fetch or committed with LICENSE + tonc notice (repo-level MIT is clear; logo bytes are the residual rights item, see section 3). Per-ROM SHA-256 in the manifest. Use `arm`, `thumb`, `memory`, `ppu`, `save/*` first; treat `unsafe/*` as informational only; `bios` is a Nintendo-BIOS-only local check. |
| **nba-emu/hw-test** @ `fbc99140e06f` on **Codeberg** (2026-08-10). The GitHub repo is **archived** ("Close GitHub repository", 2026-06-21; README says development moved to Codeberg) | BSD-3-Clause, (c) 2021 fleroviux. Codeberg API reports no license field but the LICENSE file is the same BSD-3 text [PF]. | BSD-3 clause 2 requires reproducing the notice with binary redistribution. Each ROM statically links libgba and (Makefile `-lmm -lgba`) maxmod. PPU tests carry `expected.png`; whether those were captured from hardware or from NBA is **[UNK]**. | Per-test `Makefile` with `DEVKITARM`, `-mcpu=arm7tdmi -O2`; prebuilt `.gba` committed upstream. | **Visual only.** `printf` via libgba `consoleDemoInit()` prints `PASS`/`FAIL` lines and "congratulations!"; no machine channel. PPU tests are compared against `expected.png`. | `haltcnt` uses BIOS `CpuSet` and POSTFLG [PF]. Others [UNK], at minimum libgba init. | **Deep lane, pinned fetch from Codeberg.** Requires Gamebloke's raw-frame capture plus a text-grid reader, or per-test frame digests recorded once. Treat `expected.png` provenance as unknown until the author states it. Good targets for DMA/IRQ/timer/halt/PPU-latch edge timing. |
| **SingleStepTests/arm7tdmi** @ `e3097d88d428` (2025-06-13) | MIT (repo) | **Oracle is NanoBoyAdvance, not hardware.** README: "generated from fleroviux's excellent NanoBoyAdvance" [PF]. NBA is GPL-3.0 and its GitHub repo is also archived (moved to Codeberg). Whether machine-generated data derived from a GPL emulator carries any obligation is a legal question: **[UNK], not resolved here**. 46 `.json.bin` files at ~20 MB each (~900 MB) [PF: contents listing]. | Python `transcode_json.py` converts `.json.bin` to JSON. | Machine-readable (initial/final CPU state per instruction), but timing/bus-cycle fidelity is whatever NBA does. | None (pure CPU state). | **Deep lane, subset only.** Commit a small sample the way cogmaster commits `sst-sample.bin` (rows in `tests/roms/manifest.txt` with source, pin, license, sha256) [OBS]. Label as "differential against NBA", never "hardware-accurate". |
| **emibios `test_rom.gba`** (BIOS accuracy test ROM) | emibios tests/tooling MIT; BIOS LGPL-3.0-or-later (section 5, decision 4) | Built with Zig 0.17; `build.zig.zon` pulls `zigimg` and a **GBAHawk** archive for its runner [PF] | `zig build rom` | BIOS-accuracy unit tests (`zig build test`) | n/a (tests BIOS behavior) | Defer until a replacement BIOS is in scope for a claim. |
| **FuzzARM** (DenSinH) | GPL-3.0 [PF]; last push 2022 | Python generator + FASMARM, `numpy` | `py main.py N` | Unknown channel [UNK] | [UNK] | Skip. Copyleft generator, unmaintained. Gamebloke's own deterministic generator (seeded, owned) covers the same ground. |
| **ARMwrestler GBA** (destoer/armwrestler-gba-fixed) | **No license file** [PF: GitHub license null, 2020] | A fork of an older program with unstated authorship | Makefile + sources | Visual | none expected | **Do not admit.** Violates the "authors state no licence" rule in the siblings' ASSET_POLICY. |
| **AGS Aging Cartridge** | **Proprietary Nintendo** test program; versions v1.0, v7.0, v7.1, v9.0, v10.0 documented; v10 leaked via Nintendo's Switch emulator [CR: tcrf.net via search result] | Not redistributable | n/a | Nintendo's own on-screen pass/fail | needs retail-like environment | **Never in source control, CI, artifacts, or screenshots.** If the owner lawfully holds a copy, local-only, off the manifest, results unattributable in public docs. Rights gate. |
| 240p-test-mini (pinobatch) | GPL-2.0 [PF] | video-signal suite | devkitARM | visual | n/a | Skip. Not a machine-behavior fixture; copyleft. |
| ladystarbreeze/GBA-Test-Collection | MIT [PF] | README says "TODO"; last commit 2021 | assembly | unknown | unknown | Skip. |

"Endrift's timing tests" [from the request]: I found no separate repo. The timing, timers, timer-IRQ, DMA, and SIO-timing suites inside mgba-emu/suite are that work [INF].

### Fixture policy to carry into requirements

1. Admit by manifest row (source URL, revision SHA, license, notice file, build recipe, toolchain identity, digest, BIOS mode, expected result, "permitted use": `in-tree` | `pinned-fetch` | `local-only`). Mirror cogmaster's `tests/roms/manifest.txt` and `pins.txt`.
2. `hygiene.sh` rejects any ROM/BIOS/save signature not listed in the manifest.
3. Hash == identity only. A suite passing under HLE says nothing about Nintendo BIOS behavior, and NBA-generated vectors say nothing about hardware.
4. Result labels must name machine profile + BIOS mode + suite revision + toolchain digest.

---

## 5. Decision 4: BIOS replacement options

| Option | License (PF) | Redistribute / build in CI | Notes |
|---|---|---|---|
| Nintendo BIOS dump | Proprietary | **Never** bundled, fetched, or in CI. User-supplied local only. | A matching hash proves identity, not rights. |
| **Normmatt/gba_bios** | **No license file**. README: "a disassembly of the Game Boy Advance BIOS ROM ... an original BIOS is required in order to build. Rename this file to baserom.bin" [PF] | **Do not use.** It is not a clean-room replacement and cannot be built without the Nintendo image. | The request called this an "open-source BIOS". The current repo does not support that reading. Community sources say other projects embed a "Normmatt open-source BIOS" [CR: GBARunner2 wiki, gdkGBA readme via search], which is not a rights clearance and I could not reconcile it with the repo. Flag for the owner: treat as derivative until proven otherwise. |
| **Cult-of-GBA/BIOS** @ `a30e9a96df08` (last commit 2025-04-06; 80 commits) | LICENSE is MIT text, (c) 2020-2021 DenSinH and fleroviux (GitHub detects MIT) [PF]. README: "Written completely in ARM assembly from scratch" (author claim). | **Yes, build from source in CI**: `make.sh` is `arm-none-eabi-as entrypoint.s -mcpu=arm7tdmi` then `objcopy -O binary` [PF]. Output 16,384 bytes. A prebuilt `bios.bin` is also committed upstream. | Accuracy is an explicit non-goal ("high accuracy is a non-goal") and several sound SWIs are unimplemented [PF: README TODO]. Boot-screen glyph/glow data (`glyphs.dat`, `glow.dat`, `names.*`) has no stated provenance [UNK]. Entry code does not appear to validate the cartridge logo [INF]. |
| **coolbho3k/emibios** 0.2.1 (2026-10-26; 3 releases in October; active) @ `6b134b3a24d2` | **BIOS: LGPL-3.0-or-later; tests and some tooling MIT** [PF: README + LICENSE]. A boot-screen font is credited to "Fonts for GB Studio" (Jeremy Oduber, itch.io), license **[UNK]**. | Release assets with digests: `gba_bios.bin sha256:95e6c8a6...`, `gba_bios-no_header_check.bin sha256:a30ab81e...`, `test_rom.gba sha256:16605f8d...` [PF]. Build from source needs Zig 0.17. | Accuracy-focused, many SWIs timing-tuned, multiboot, header-check boot screen. Provenance statement: "not intended to contain, or use as reference, any proprietary code... rely on observing and matching the behavior of the retail BIOS", with LLMs used "to propose and run the thousands of experiments" and firewalled from proprietary code [PF: its README]. That is the author's assertion, not an audit. The header-check build requires the Nintendo logo (it ships a 156-byte `gba_logo.bin`); the `no_header_check` build does not. LGPL conveying obligations apply if Gamebloke ever ships it; a pinned-fetch used as runtime input does not link it. |
| ez-me/gba-bios | GPL-2.0 [PF]; last push 2023-05 | copyleft; not evaluated further | Skip unless a need appears. |
| **mGBA HLE BIOS** | MPL-2.0 [PF: `hle-bios.s`, `bios.c`] (C SWI implementations + a small asm stub) | Not a BIOS file, an HLE in the emulator | Reference for behavior only. Do not copy source into an MIT core. The sibling PROVENANCE rule (no code, comments, tables, or layout from another emulator) applies. |
| **Gamebloke-owned HLE / direct boot** | Gamebloke's license | Yes, shipped with the core | Written from GBATEK-class hardware facts, tested by the owned diagnostics. |

**Recommendation (a ladder, and every claim names its rung): Confidence MEDIUM-HIGH.**

- **M0, default:** direct boot with Gamebloke-owned SWI HLE. No BIOS bytes, no rights surface, ships with the core. This is the mode the PR gate and the first profile claim use.
- **M1, optional, test-only:** Cult-of-GBA BIOS built from source in CI with the pinned binutils (MIT, no extra download). Good enough to run suites that only need IRQ dispatch and basic SWIs.
- **M1b, differential only:** emibios, **pinned-fetched by SHA-256 and never committed**, for comparing BIOS-dependent behavior. Not in the PR gate (young, moving fast, LGPL, Zig build, font and provenance caveats).
- **M2:** user-supplied Nintendo BIOS, local only, never CI. Covers jsmolka's `bios/bios.gba` and the BIOS-sensitive parts of the mGBA suite.

Owner gate: any statement that a replacement BIOS is "clean" or "cleared" is the owner's to make. This research cannot establish that from repo claims.

---

## 6. Decision 5: Legally cleared playable homebrew (early "playable scenario")

I found **no third-party game I can call fully cleared** without a per-asset review. The repo license almost never covers the art and music.

| Candidate | Code license (PF) | Asset/other rights (PF) | Verdict |
|---|---|---|---|
| **Gamebloke-owned "playable 0"** (original Pong/breakout-class game, assembly + tiny freestanding C, MIT) | Gamebloke's license | Cleared by construction | **Recommended default.** Cheapest route to a rights-clean playable scenario and the only one that can include save-device and timed-input coverage you choose. Fixture manifest row `permitted use: in-tree`. |
| AnthonyCalandra/gba-pong @ `1efdad3558af` | MIT (2015, pushed 2026-08) | Font is "credit to TONC" (`toncfont.h`); tonclib text is MIT, (c) J Vijn [PF]. Links `libgba` (LGPL+exceptions; attribution rule) [PF]. Uses BIOS `CpuFastSet` (SWI 0xC) [PF]. The 2015 Makefile against r67 devkitARM has **not** been built [UNK]. | **Conditional candidate** as corroboration. Pinned fetch, build in the devkitARM lane, record attribution. Not the first playable. |
| Butano Fighter (in GValiente/butano `games/`) | zlib (butano) | `credits/music.txt` lists `cyberrid.mod` as **CC BY-NC-SA** and other tracks as CC0 / CC-BY 4.0; graphics CC-BY-SA 3.0 plus CC0 [PF] | **Reject.** The NC-SA music alone fails "permits redistribution in tests". Repo zlib does not clear it. |
| fritzem/Google-Dino-GBA | MIT | README: "All game art is property of Google" [PF] | **Reject.** |
| jtsiomb/gbatris | GPL-3.0-or-later | Music by Greig Stewart is separately copyrighted with no stated grant [PF: README] | **Reject** for in-tree use. |
| JeffRuLz/Celeste-Classic-GBA (405 stars) | **No license** [PF] | Port of a third party game | **Reject.** |
| afska/beat-beast, jeremyelkayam/trogba, exelotl/xniq (MIT), Solar-Guard, Inheritors of the Oubliette, voxelburg (GPL-3.0) | Per GitHub/catalog | Asset licenses not reviewed this session | **[UNK].** Candidates for a later per-asset review. GPL games used purely as input data to the emulator do not change the emulator's license [INF, not legal advice], but shipping the ROM in an MIT repo carries GPL source obligations. Prefer pinned fetch. |
| gbadev-org "Homebrew Hub" catalogue (251 entries) | Catalogue code GPL-3.0; "every other asset ... is copyrighted by the author of the entry" [PF: its LICENSE] | `game.json` has a `gameLicense` field on only **7 of 251** entries and `assetsLicense` on **0 of 251** [PF: I fetched all 251 `game.json` files, no ROMs] | A discovery index, not a clearance source. The catalogue also stores `.gba` binaries, so do not mirror it. |

**Recommendation:** ship an owner-authored playable ROM first (MIT, built from source, same pipeline as the diagnostics). Add gba-pong later as an independent third-party corroboration only if its build and attribution check out. Confidence **HIGH** that no verified fully-cleared third-party game exists in what I checked; **MEDIUM** on gba-pong as a conditional.

---

## 7. Decision 6: Project license (recommendation only; owner gate)

| Option | For | Against |
|---|---|---|
| **MIT** | All four sibling emulators are MIT [OBS]. Maximum embeddability (Playstead, LibFuturo consumers, closed hosts). Fixtures sit under their own `LICENSE.txt` and a `THIRD-PARTY-NOTICES.md` so the core license does not decide fixture rights (gabbaboy pattern) [OBS]. | No patent grant, no reciprocity. |
| Apache-2.0 | Explicit patent grant. It is LibFuturo's license [OBS]. | Notice/NOTICE-file overhead; GPLv2-only incompatibility; the GBA hardware patent concern is low (2001-era) [INF]. |
| MPL-2.0 | File-level reciprocity; mGBA uses it [PF]. Keeps improvements to core files open. | Breaks portfolio consistency. Combining MPL files with an MIT tree complicates the sibling PROVENANCE rule. Little benefit if no outside contributors are expected [INF]. |
| GPL/LGPL | Strong reciprocity. | Defeats the embeddable-core goal; incompatible with the planned relocatable consumer story. |

**Recommendation: MIT** (confidence MEDIUM; this is a preference call, not a technical one). Pairs with `PROVENANCE.md`: facts may enter, another emulator's expression may not; copyleft emulator source (GPL: NBA, VBA-M, Hades) not opened, permissive source read only for behavior; MPL (mGBA) read for facts, not copied. The license of the core does not "embed" or "infect" separately-licensed test ROMs, because fixtures are inputs under their own manifest rows.

Owner gates: project license; repository visibility; whether fixtures that carry GPL/LGPL/BSD notices may sit in the tree versus pinned-fetch; the logo-bytes question; any "cleared" claim for a replacement BIOS.

---

## 8. Decision 7: Minimal dependencies and CI shape

### Dependency set

| Concern | Choice | Rationale |
|---|---|---|
| Test framework | **None.** CTest + `check.h` + junit-inventory check | Siblings' proven pattern [OBS]. Zero dependencies, direct visibility of skipped/missing tests. |
| Hashing | In-tree SHA-256 (also CRC32 only if a fixture needs it) | No OpenSSL. Digests of raw output are the canonical determinism evidence. |
| Video capture, canonical | **Raw native frame** (240x160, one little-endian u16 per pixel as the machine produces it; confirm bit layout in the PPU phase [INF: BGR555]) plus SHA-256 of those bytes | Keeps "raw video" separate from host conversion (PROJECT.md). |
| Video capture, human-facing | **PPM (P6)** for diffing and CI artifacts; **PNG via a stored-deflate writer** (cogmaster `runner/png.c`: IHDR/IDAT/IEND, zlib header with uncompressed blocks up to 65,535 bytes, no zlib dependency) [OBS] | No libpng/zlib/stb. RGB expansion is labeled a host conversion, not machine output. |
| Audio capture, canonical | **Raw s16le interleaved stereo at the machine sample rate**, plus SHA-256 | Keep resampling in the host. The machine rate depends on SOUNDBIAS [UNK: define in the audio phase]. |
| Audio capture, human-facing | **WAV** (44-byte RIFF header) | Trivial, no dependency. |
| Input scripts | One absolute record per frame or per timed boundary, parsed in fixed memory and validated before run (cogmaster `--input`: unknown/repeated/surplus fields exit 2) [OBS] | Ready-made refusal semantics. |
| Fuzzing | libFuzzer (Clang) | No new library. Scheduled lane. |
| Fixture/tool fetch | `pins.txt` + `fetch-pin.sh` (SHA-256 verified before unpack) | Already written and tested in the portfolio. |

LibFuturo fit flag: its video formats are `GRAY8`, `RGB24`, `INDEX8` only [OBS: `libfuturo.h`]. There is no 15-bit format, so a GBA adapter would force a host conversion to RGB24 or a LibFuturo extension. This belongs in the later LibFuturo-fit phase; it does not affect the core's raw output decision.

### CI platform shape

GitHub Actions, copying the sibling workflow shape [OBS]:

- Single required check named `CI required` that passes only if every job did. No trigger-level path filters (a path-skipped workflow leaves the required check pending); a `changes` job classifies so `.planning/`-only PRs skip heavy jobs.
- `permissions: contents: read`; actions pinned by full SHA with the version in a comment; `persist-credentials: false`; concurrency cancels superseded PR runs but never push runs.
- Conventional-Commit PR title check (squash message comes from the title).
- Each job runs `cmake --workflow --preset <lane>`, then a "every test ran" check against the inventory.
- Timeouts derived from measured runs ("2 x slowest of N"). Cogmaster's measured small-core lanes: hygiene 33 s, ASan 22 s, nofp 8 s, be32 34 s, title 5 s [OBS: workflow comments, "provisional"]. Plan on a few minutes for a young core.
- Runners seen in siblings: `ubuntu-26.04`, `ubuntu-26.04-arm`, `macos-15`, `macos-15-intel`, `windows-2025`, `windows-11-arm` [OBS: not verified against GitHub's current image list].

| Lane | PR gate (fast, required) | Scheduled / on-demand (deep) |
|---|---|---|
| hygiene (clang-format-21, tidy, asset/ROM-signature scan) | Yes | |
| title (Conventional Commits) | Yes | |
| build + CTest, Release, warnings-as-errors | `ubuntu-26.04` GCC; `macos-15` arm64 (owner's host) | + `ubuntu-26.04-arm`, `windows-2025` MSVC, `windows-11-arm`; add `macos-15-intel` only if an Intel user exists [INF] |
| ASan + UBSan (`asan` preset, clang-21) | Yes (Linux) | |
| Owned fixtures: build with pinned xPack, compare digest, judge mailbox, negative controls | Yes (seconds; no devkitPro) | |
| Installed, relocatable consumer (C and C++) | Yes once packaging exists | |
| nofp, be32 + hash-equality, wasm32 | | Yes, once the core has substance |
| libFuzzer over loader/state/input parsers | | Nightly |
| Third-party suites (jsmolka, mGBA suite, hw-test) | jsmolka only (committed or pinned ROMs, no devkitARM) | mGBA suite and hw-test built in the devkitARM Docker lane; SingleStepTests full sets |
| Perf / benchmark | | Trend data, never a gate; separate from CI wait time |
| Nightly failure handling | | A failed night opens or updates one issue; a good night closes it [OBS: cogmaster nightly] |

Add a lane only when it catches a distinct recurring failure class. After every CI run, check the critical path for duplicate setup (PROJECT.md/AGENTS.md rule).

---

## 9. Alternatives considered (compact)

| Category | Recommended | Alternative | Why not now |
|---|---|---|---|
| Core language | C17 | Rust | See section 2: safety benefit real, but loses portfolio harness and LibFuturo-native ABI. Revisit on fuzz evidence. |
| Core language | C17 | C++20 | ABI/exception/static-init surface, nothing gained. |
| ROM toolchain | xPack GNU `arm-none-eabi` 15.2.1-1.1 | devkitARM Docker | Attribution/LGPL surface in every ROM; 500 MB image; mutable repo. Keep for third-party suites only. |
| ROM toolchain | GNU as/ld | clang+lld | No v4T builtins; link unverified. |
| ROM toolchain | project crt0/ld | gba-toolchain | Extra layer. |
| BIOS | HLE (M0) + Cult BIOS built from source (M1) | emibios as default | LGPL, young, Zig, font/provenance unknowns; use as pinned differential. |
| Test oracle | owned diagnostics + jsmolka | SingleStepTests as gate | Oracle is NBA, not hardware, ~900 MB. |
| Image output | raw + PPM + stored-deflate PNG | libpng/stb | Dependency for a human convenience. |
| License | MIT | MPL-2.0 / Apache-2.0 | Consistency, embeddability. |

## What NOT to use (consolidated)

| Avoid | Why | Instead |
|---|---|---|
| Normmatt/gba_bios as a replacement BIOS | Disassembly that needs a Nintendo `baserom.bin`; no license | Own HLE; Cult-of-GBA BIOS (MIT) |
| AGS Aging Cartridge anywhere in repo/CI | Proprietary Nintendo | Owned diagnostics, hw-test, mGBA suite |
| ARMwrestler (destoer fork) | No license file | jsmolka `arm`/`thumb` |
| Butano Fighter, Google-Dino-GBA, Celeste GBA, GBAtris as fixtures | NC-SA music / Google art / no license / separate music copyright | Owned playable ROM |
| GitHub `nba-emu/hw-test` and `NanoBoyAdvance` URLs | Archived; moved to Codeberg | `codeberg.org/nba-emu/hw-test` |
| Homebrew `arm-none-eabi-gcc` (16.2.0) for committed digests | Version differs from CI | Pinned xPack |
| Mutable `devkitpro/devkitarm:latest` or the dkp apt repo | Not reproducible | Image by digest + recorded `dkp-pacman -Q` |
| Unpinned fixture fetch | A hash proves identity only if pinned and checked first | `pins.txt` + `fetch-pin.sh` |
| `-ffast-math` / `-O3` C-built ROMs as stable fixtures | Codegen drifts with compiler version | Assembly-first |

---

## 10. Stale or wrong items in `EMULATOR-DNA.md` and the request

1. **hw-test link is stale.** `github.com/nba-emu/hw-test` is archived; active development is at `codeberg.org/nba-emu/hw-test` (HEAD `fbc99140e06f`, 2026-08-10) [PF]. The NanoBoyAdvance GitHub repo is also archived (moved to Codeberg) [PF].
2. **"mGBA suite - repository identifies an MIT license"** is true but incomplete: two source files carry a BSD-2-style header, the font bitmaps have no stated provenance, results are menu-driven rather than exit-coded, and the build needs devkitARM + libgba with GCC-version-sensitive expected values [PF].
3. **"jsmolka - MIT"** is true; omitted that ROMs are prebuilt with freeware FASMARM, embed the Nintendo logo bytes, and report only via r12/on-screen; `bios.gba` needs the real Nintendo BIOS [PF].
4. **"nba-emu hw-test - BSD-3"** is true; omitted that output is visual `printf` text and `expected.png` images of unknown provenance, and that builds need libgba + maxmod [PF].
5. **Missing:** SingleStepTests/arm7tdmi is an NBA-generated oracle, not hardware [PF]; emibios (accuracy-focused open BIOS, LGPL-3.0+, active) and Cult-of-GBA BIOS (MIT, buildable with GNU `as`) are the viable replacement-BIOS leads; neither is mentioned [PF].
6. **The request's "Normatt's open-source BIOS"** does not match the current Normmatt/gba_bios repo (a disassembly requiring a Nintendo image) [PF].
7. **Not re-verified this session:** the AGB Programming Manual mirror, GBATEK, Tonc/gbadoc and ARM TRM links, and Nintendo sales figures. gametechwiki returned a Cloudflare challenge and devkitpro.org returned 403 (so devkitPro's own terms stay [UNK]).
8. PROJECT.md says siblings are "MIT where present"; LibFuturo is Apache-2.0 [OBS].

---

## 11. Version compatibility / traps

| Item | Constraint |
|---|---|
| CMake preset schema vs minimum | schema 6 needs 3.25; 11 needs 4.3; 12 needs 4.4; 13 needs 4.5 (rc) [PF]. Cogmaster's schema 11 forces CMake 4.3 on consumers who run presets. |
| `-mcpu=arm7tdmi -mthumb` multilib | Observed on Homebrew GCC 16.2.0: links `thumb/nofp`, ELF attribute `v4T` [OBS]. Unverified on xPack/Ubuntu until the spike runs. |
| ARM7TDMI has no hardware divide | Freestanding C using `/` needs libgcc's `__aeabi_uidiv`. Clang has no armv4t builtins [OBS]. Assembly-first avoids the problem. |
| mGBA suite expected values | Depend on GCC version [PF]. Fixture identity = suite SHA + toolchain digest. |
| `-fno-stack-protector -U_FORTIFY_SOURCE` for the core | Keeps the core free of libc helpers so hashes match across platforms (cogmaster) [OBS]. |
| macOS 26 / arm64 dev host | ASan lane is Linux-only in gabbaboy and runs in Linux CI for cogmaster; do not rely on local macOS sanitizer parity. |

## Gaps and phase-research flags

- **Toolchain spike (first fixture phase):** confirm xPack 15.2.1-1.1 produces `v4T`, links libgcc division, and yields identical ROM bytes on macOS arm64 and ubuntu-26.04 for a trivial ROM; if not, switch to apt-only on CI and move digest ownership to Linux.
- **mGBA-suite headless harness:** SRAM log extraction, A-press timing script, and which suites are meaningful under M0 HLE. Needs phase-specific research.
- **hw-test expected-image provenance** and exactly which tests require BIOS; ask the author or inspect per-test READMEs.
- **Replacement-BIOS accuracy deltas** (Cult vs emibios vs Nintendo) on specific suites; only matters once a claim depends on it.
- **Logo bytes and font provenance** (mGBA suite font, Cult glyphs, emibios font) are rights-review items before anything is committed or published.
- **devkitPro redistribution terms** for mirroring or caching their image/packages: unresolved.
- **LibFuturo 15-bit video gap** for the adapter phase.

## Sources

- Sibling repos read directly (OBS): `~/projects/{gabbaboy,nesturbator,cogmaster,libfuturo}` CMakeLists, CMakePresets, `.github/workflows`, `release-please-config.json`, `pins.txt`, `scripts/fetch-pin.sh`, `ASSET_POLICY.md`, `PROVENANCE.md`, `THIRD_PARTY_NOTICES.md`, `tests/check.h`, `runner/png.c`, `docs/runner.md`, `libfuturo.h`.
- GitHub API / raw files (PF, 2026-11-09): mgba-emu/suite (LICENSE, Makefile, `docker-build.sh`, `src/main.c`, `src/mgba.c`, `include/mgba.h`, commit log); jsmolka/gba-tests (LICENSE, README, `lib/header.asm`, `lib/macros.inc`, `bios/bios.asm`, `unsafe/README.md`); nba-emu/hw-test (GitHub archive notice, tree, LICENSE) and its Codeberg mirror API (`codeberg.org/api/v1/repos/nba-emu/hw-test`); SingleStepTests/arm7tdmi README; Cult-of-GBA/BIOS (LICENSE, README, `make.sh`, `entrypoint.s`); coolbho3k/emibios (README, LICENSE, `build.zig.zon`, release assets); Normmatt/gba_bios README; mgba-emu/mgba (`hle-bios.*`, LICENSE); devkitPro/{libgba, devkitarm-crtls, buildscripts, docker, gba-tools}; xpack-dev-tools/arm-none-eabi-gcc-xpack releases; felixjones/gba-toolchain; rust-console/gba; agbrs/agb; GValiente/butano; AnthonyCalandra/gba-pong; fritzem/Google-Dino-GBA; jtsiomb/gbatris; gbadev-org/games and libtonc; DenSinH/FuzzARM; destoer/armwrestler-gba-fixed; pinobatch/240p-test-mini; velipso/gvasm; Kingcom/armips.
- Docker Hub API: `devkitpro/devkitarm` tags and digests.
- Ubuntu package index (packages.ubuntu.com): `gcc-arm-none-eabi` per release.
- CMake documentation, cmake-presets(7): schema-to-version table.
- tcrf.net AGS Aging Cartridge page (via search result; CR).
- Local probes (OBS): `arm-none-eabi-gcc 16.2.0` multilib and `readelf -A`; `clang 21 --target=armv4t-none-eabi` compile; `llvm-nm` division symbol.
- Method note: the `gsd research-plan` cache seam was not used. All facts above were fetched directly from repos, APIs, and package indexes; no ROM or BIOS binary was downloaded (only `game.json` text metadata and source/license files).
