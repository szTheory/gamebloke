# Phase 05 Plan 02: Original ARM/Thumb Toolchain Spike

## Scope and source

This report covers one project-authored ARM/Thumb smoke image and its deliberately broken assertion companion. Both are generated locally from `diagnostics/armv4t/startup.S`, `smoke.S`, and `linker.ld` with direct `as`, `ld`, and `objcopy` calls. No SDK, crt0, Nintendo logo, or linked runtime is used. Output images and build evidence stay under ignored `build/phase05-spike/`.

The startup code branches from the ROM entry at `0x08000000`, uses a stack at `0x03007F00`, and writes the five-word result mailbox at `0x03007FC0`. The ARM routine checks `2 + 3 == 5`, exchanges into Thumb with `bx`, and checks `4 + 2 == 6`. The broken build uses the same instructions but changes the expected constants. Both report case ID `0x5001` (`20481`). The header fixer leaves the Nintendo-logo window zeroed and writes the GBA checksum; the generated 312-byte image stores `0xe7`, matching the checksum recomputed from bytes `0xa0..0xbc`.

## Toolchain and environment observations

Both host archives are xPack GNU Arm Embedded GCC `15.2.1-1.1`; host archive identities differ as expected.

| Witness | Actual host | Archive bytes / SHA-256 | First download / extract | Tool versions |
| --- | --- | --- | --- | --- |
| macOS arm64 | Darwin / arm64 | 293,820,200 / `574082d35e49a2bcbdc355836b2a3ae5e5bb3b9456c9f5e37177db2ab4aad870` | 33s / 4s | as, ld, objcopy, readelf, objdump, nm: 2.45.1.20251203 |
| Linux arm64 | Pinned Ubuntu 26.04 container; Docker and guest report Linux / aarch64 | 302,624,714 / `67980c7990eba7bb7ffdf39699102effd70889f5ac427be19a8c8a6c5fab2972` | 32s / 19s | as, ld, objcopy, readelf, objdump, nm: 2.45.1.20251203; GCC check: 15.2.1 |

The Linux image was already present locally at `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`; its inspected platform was `linux/arm64`. The witness used the dependency recipe already in `scripts/linux-lane.sh` (`ca-certificates`, `cmake`, `curl`, `ninja-build`, `git`, `libc6-dev`, and `gcc`) inside the disposable container. It did not pull an image or add a project dependency.

The first fresh Linux container run took 103.98s by `/usr/bin/time -p`: package provisioning 24s, toolchain fetch/verification 79s, and the smoke-build stage rounded to 0s at the build receipt's 1-second resolution. The toolchain receipt separately measured archive download at 32s and extraction at 19s. A post-commit cached recheck took 25.39s: provisioning 24s, toolchain check 0s, and build 1s at the same 1-second resolution. The cached run reused the verified Linux install and did not download the archive again. These are local arm64 measurements, not hosted-job timings.

The install receipts inventory 53 notice paths for Darwin arm64 and 54 for Linux arm64. Both include notices for binutils, GCC, GDB, newlib, and the packaged support components; the Linux archive also includes a gpm notice. Exact paths remain with each ignored `build/toolchains/arm-none-eabi/<host>-15.2.1-1.1/install.json` receipt. This inventory records what was present; it does not determine rights for every component or grant rights to generated images. No new license file or redistribution claim was added.

## Build and architecture evidence

For both variants and both hosts:

- `readelf -A` reports `Tag_CPU_arch: v4T`, ARM ISA `Yes`, and Thumb-1 for each input object and the final ELF.
- The map contains exactly the project `startup.o` and `smoke.o` inputs plus GNU ld's one `linker stubs` marker. It contains no archive/runtime input, and `nm -u` reports no unresolved symbols.
- The ELF has only `.gba_header` and `.text` executable sections. Captured disassembly includes both sections, the entry branch, and ARM/Thumb `bx` instructions.
- The exact-name ROM scan reports `signature: ok` for `smoke.gba` and `smoke-broken.gba`; logo and BIOS checks remain active.

The offline controls pass a clean map and clean signature scan. Planted runtime and archive inputs, missing and extra objects, wrong or missing v4T attributes, and an extra executable section fail with their expected reasons. Synthetic logo-window and BIOS digests also fail when their filenames are explicitly allowlisted, proving the content checks remain active. The controls use stand-ins only; they contain no copyrighted ROM or BIOS bytes.

## Local byte comparison and runner behavior

The complete output filename sets on macOS arm64 and native Linux arm64 are both exactly `smoke.gba` and `smoke-broken.gba`. Literal `cmp` returned exit 0 for each pair, confirming byte identity for both variants. `proof-only` independently rechecked source fingerprints, maps, section/attribute evidence, signature scans, output sets, and both `cmp` results at committed source head `b3cf9a0`.

The production runner observes the positive mailbox as semantic `pass`, case `20481`; the broken companion is semantic `fail`, the same case. For both unregistered original images, the documented raw result is `GUEST_STOP` / `unregistered-content` with process exit 1. The assertions check mailbox semantics separately from that expected raw runner status; the production runner was not changed to force shell success.

## Decision and limits

The local macOS arm64 and native Linux arm64 outputs are byte-identical, so this comparison requires no local host-specific fallback. No ROM digest was admitted to a manifest or pin, and no image bytes were tracked or uploaded. The ignored receipts under `build/phase05-spike/` retain exact local output hashes and command evidence.

Hosted Linux x64 remains a separate, later witness and was not run here. The `hosted-proof` selector fails closed when no real job receipt exists; this plan creates no hosted success record. Local arm64 parity does not establish hosted x64 parity, broad game compatibility, hardware accuracy, BIOS support, or release qualification.
