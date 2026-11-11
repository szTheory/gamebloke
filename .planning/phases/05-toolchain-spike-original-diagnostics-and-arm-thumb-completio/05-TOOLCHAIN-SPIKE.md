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

At the Plan 02 evidence cutoff, hosted Linux x64 remained a separate, unrun witness. The Plan 03 result is recorded below; it does not change the limits of the local arm64 parity evidence or establish broad game compatibility, hardware accuracy, BIOS support, or release qualification.

## Plan 03 hosted CI witness

The corrected required run on the existing draft PR is [CI run 43089440697](https://github.com/szTheory/gamebloke/actions/runs/43089440697), checked out at full source head `2a18d8e1b2f7d24b11b40627e697cfd42b60c48b`. Both required diagnostic jobs and every other underlying job passed; `CI required` passed. The exact raw API snapshot and job logs are retained in ignored `build/phase05-spike/05-03-corrected-hosted-run.json`, `05-03-corrected-hosted-jobs.json`, `05-03-corrected-linux-x64.log`, and `05-03-corrected-macos-arm64.log`.

| Hosted job | Result | Whole job | Toolchain fetch step | Diagnostics workflow | Margin to unchanged 180s cap |
| --- | --- | ---: | ---: | ---: | ---: |
| Ubuntu 26.04, Linux x64, GCC/GNU | success | 92s | 81s | 5s | 88s |
| macOS 26, Darwin arm64, AppleClang | success | 145s | 130s | 7s | 35s |

Other hosted results were: Ubuntu GCC build 113s, macOS build 140s, GNU regression 113s, ASan/UBSan 100s, hygiene 61s, and `CI required` 6s; all passed. The corrected macOS diagnostics job is 19 seconds faster than the historical 164s result. The installer receipts report only transfer/extraction sub-times (Linux 4s/3s, macOS 3s/3s); those fields do not represent the total fetch step, which also includes verification and staging. Toolchain setup is the dominant portion of each diagnostics job. The existing jobs do not repeat the same host installation within a lane, and no measured evidence justified adding a cache, changing dependencies, or raising the job limit.

Both diagnostic jobs built the original positive and deliberately broken 312-byte images. All six ARM/Thumb object and final-ELF records report ARMv4T (`Tag_CPU_arch: v4T`), ARM ISA support, and Thumb-1. The production runner returned the expected semantic `pass` and `fail`, both with case `20481`; its raw unregistered-content control remains `GUEST_STOP` / `unregistered-content`, process exit 1. The diagnostics workflow ran its four required tests and post-test inventory on both hosts with zero skips. This includes the script-mode host-detection controls, all 21 installer `offline-all` cases, the controlled installer-refusal propagation check, and exact hosted-output row checks; missing, duplicate, and extra output rows are rejected by their controls.

The hosted Linux x64 job reports `smoke.gba` SHA-256 `424a460469dbdbbf98b1eb1991e26a45ae9324a2127936f81572308ba38510d9` and `smoke-broken.gba` SHA-256 `76d20735f76c2bb8c67117c98b6488f0ab324039b8b19d768f14aecb64caa9e6`. These values were compared with independently built local counterparts under ignored `build/phase05-spike/local-counterparts/roms`, whose provenance records that they came from the existing local macOS arm64 output. Both values also match the existing native Linux arm64 outputs; literal `cmp` exit 0/0 applies only to the local macOS arm64 versus native Linux arm64 pair. The hosted proof is SHA-256 identity evidence, not a literal hosted/local byte comparison. No hosted binaries were downloaded, no ROM artifacts were uploaded, and no diagnostic ROM digest was admitted.

Fresh local Plan 03 controls at the same source head also passed: `sh scripts/check-workflow.sh`, the 17-case `CASE=needs` suite, and the real `CASE=hosted-proof` verifier against the captured Linux x64 receipt and local counterpart provenance. Native macOS and the existing pinned Linux arm64 container both passed `cmake --workflow --preset diagnostics` with 4/4 required tests, 4/4 post-inventory, and zero skips. Their ignored logs are `05-03-host-fix-darwin-diagnostics.log` and `05-03-host-fix-linux-diagnostics.log`.

The initial red run remains recorded in `.planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-03-CI-HANDOFF.md` as historical evidence. The owner explicitly authorized the narrow offline-helper correction and bounded retest; the corrected all-green run resolves that stop for Plan 03. Any future red hosted run still requires the same stop before a fix or retry. These results satisfy the bounded Plan 03 hosted witness; they do not complete the phase's full diagnostic matrix or authorize digest admission, CPU expansion, BIOS/game compatibility claims, or release qualification.
