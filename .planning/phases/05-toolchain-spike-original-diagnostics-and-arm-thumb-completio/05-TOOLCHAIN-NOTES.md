# Phase 5 Supporting Notes: xPack Toolchain and Reproducible Diagnostics

**Prepared:** 2026-11-11
**Scope:** Independent toolchain, fetch/extract, reproducibility, and supply-chain research for Phase 5. This is supporting input for discussion/planning, not canonical `05-RESEARCH.md` or `05-CONTEXT.md`. No toolchain archive was downloaded or executed.

## Phase 5 boundary to preserve

The roadmap requires a spike before any diagnostic ROM digest is recorded, then project-authored assembly diagnostics and full ARM/Thumb testing. Its exact success criteria include: “records `Tag_CPU_arch: v4T` from `readelf -A` and byte-identical diagnostic ROMs built on macOS arm64 and on Linux” and, for CI, “The signature scan and a link-map check confirm no logo bytes and no linked runtime library.” [VERIFIED: `.planning/ROADMAP.md:243-247`; quoted verbatim from lines 244-245]

The requirements are `DIAG-01`, `DIAG-02`, `DIAG-03`, `VERIF-04`, `CPU-01`, and `CPU-02`. [VERIFIED: `.planning/ROADMAP.md:241-242`; quote: `**Requirements**: DIAG-01, DIAG-02, DIAG-03, VERIF-04, CPU-01, CPU-02`]

Use the accepted project constraints: assembly first; project-owned `crt0`, linker script, and header fixer; no devkitARM/libgba/gbafix; no newly added runtime Python dependency; and no linked runtime in these original diagnostics. Keep fixtures and tool downloads out of source control. These are task/context constraints from the owner and orchestrator, not new research decisions.

## Verified source observations

### Current xPack release and host assets

The official GitHub Releases API currently identifies `v15.2.1-1.1` as the latest release, published `2026-03-03T07:30:26Z`. [CITED: [xPack release metadata](https://api.github.com/repos/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/latest)] The tag-specific asset metadata supplies the following exact archive sizes and GitHub `digest` values. [CITED: [xPack v15.2.1-1.1 release](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v15.2.1-1.1), [tag release API](https://api.github.com/repos/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tags/v15.2.1-1.1)]

| Host asset | Bytes | SHA-256 | Planned role |
|---|---:|---|---|
| `darwin-arm64` | 293,820,200 | `574082d35e49a2bcbdc355836b2a3ae5e5bb3b9456c9f5e37177db2ab4aad870` | Required macOS arm64 parity witness |
| `linux-x64` | 307,090,703 | `da6a49ad4003944b823c6c93702a8787c922ab34bd7e918ec0eaf6933a9b1ff6` | Linux x86_64 hosted-CI witness and recommended canonical digest owner |
| `linux-arm64` | 302,624,714 | `67980c7990eba7bb7ffdf39699102effd70889f5ac427be19a8c8a6c5fab2972` | Separate Linux aarch64 witness; the orchestrator reports a pinned Ubuntu 26.04 native container is available, not independently probed here |

**Inference:** These are distinct host binaries, so their archive hashes differ even if the diagnostic ROM bytes match. The xPack installation guide shows a separate `15.2.1-1.1.1` npm wrapper/package version; the binary asset names and GitHub release are `15.2.1-1.1`. Pin the binary release identity, not the wrapper spelling. [CITED: [xPack installation guide](https://xpack-dev-tools.github.io/arm-none-eabi-gcc-xpack/docs/install/), [release assets](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v15.2.1-1.1)]

### Existing pin contract and why it cannot fetch these assets as-is

The repository script documents: “Redirects are NOT followed” and “the integrity guarantee is the SHA-256 compare, not the transport.” [VERIFIED: `scripts/fetch-pin.sh:23-27`; verbatim phrases quoted] Its request also sets `--max-filesize 268435456`. [VERIFIED: `scripts/fetch-pin.sh:169-174`; exact literal appears at line 172] That is 256 MiB, below every xPack archive listed above. [INFERENCE from the verified byte counts and script limit]

A no-body `HEAD` request to the stable GitHub Linux x64 release URL returned HTTP 302 to a time-limited `release-assets.githubusercontent.com` URL in this session. The signed target included a short expiry and query token, so it is unsuitable as a committed pin and should never be copied to logs. [CITED: [stable release-asset URL](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz); direct HTTP observation, 2026-11-11]

The current pin file requires direct URLs and says “a 3xx answer is refused as bad-status.” [VERIFIED: `pins.txt:11-14`; quoted verbatim] The fetcher’s existing redirect negative control synthesizes a 302 and asserts that no payload is placed, without making a network request. [VERIFIED: `tests/gates/fetch_case.cmake:286-294`]

The fixture manifest linter in `MODE=all` requires pin rows to cross-reference fixture rows and rejects orphan pins; it separately permits `build.toolchain.sha256` to be null. [VERIFIED: `cmake/CheckManifest.cmake:31-32,68-74`; exact source wording: `build.toolchain.sha256 key required; null allowed; otherwise 64 lowercase hex` and `a pins row that no manifest row names (an orphan pin)`] Keep toolchain archive identities in a dedicated toolchain lock, separate from fixture pins and fixture output digests. The ROM digest identifies output bytes; each platform archive digest identifies a different tool binary. Avoid weakening the generic fixture downloader or encoding three host hashes as one fixture hash.

### ARM ELF attributes, disassembly, and no-runtime proof

ARM’s ABI defines `Tag_CPU_arch` value 2 as Arm v4T and associates Arm7TDMI with v4T. It defines the Thumb-use attribute separately; legacy value 1 referred to 16-bit Thumb, and the current addenda marks legacy Thumb values as deprecated. [CITED: [Arm ABI Addenda32](https://github.com/ARM-software/abi-aa/blob/main/addenda32/addenda32.rst)] GNU `readelf -A` reports architecture-specific ELF information, while GNU ld’s map documents input objects and archive members actually included in the link. [CITED: [GNU readelf manual](https://sourceware.org/binutils/docs/binutils/readelf.html), [GNU ld map option](https://sourceware.org/binutils/docs/ld/Options.html)]

**Interpretation:** `readelf -A` is a required metadata check, not proof that every opcode in hand-authored assembly is v4T. Build attributes describe declared requirements and can be missing or wrong in hand-authored objects. Inspect the ELF and every input object, then disassemble executable sections and run the ROM through behavioral diagnostics. Gate the final linked ELF on `Tag_CPU_arch: v4T`; separately inspect the Thumb attribute if the chosen assembler emits it. Do not accept compiler flags such as `-mcpu=arm7tdmi` as output evidence.

For the no-runtime requirement, use `arm-none-eabi-as`, direct `arm-none-eabi-ld` with the explicit project objects and project linker script, then `objcopy` and the project header fixer. Do not invoke the GCC driver for linking, pass `-l` runtime libraries, or put `INPUT`/`GROUP` library directives in the linker script. Preserve the linker map and make its object/archive inputs match an allow-list consisting only of project-owned objects. Check unresolved symbols with `nm` as a secondary check.

**A flag or symbol check alone is insufficient.** `-nostdlib` (if a future GCC-driver link uses it) is configuration intent, not a record of actual link inputs. An empty `nm -u` proves there are no unresolved references, but it cannot rule out statically included library members. The map plus explicit input list is the primary linkage evidence; a symbol-name scan is supporting evidence, not a substitute. A negative control should inject a fake `libgcc.a` map entry and prove the map gate fails.

GCC documents `libgcc` as a low-level runtime that GCC may call for operations such as integer division, and its runtime exception applies only to files carrying the relevant notice. The FSF FAQ also explains that use of GPL-covered tools does not by itself license or relicense the user’s code. [CITED: [GCC 15.2 libgcc documentation](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gccint/Libgcc.html), [GCC Runtime Library Exception FAQ](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html), [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html#CanIUseGPLToolsForNF)] Neither fact is a blanket rights grant for an output ROM or for every file in the xPack archive. Keep the archive’s component notices with the downloaded toolchain, inspect actual archive contents after pin verification, and base the diagnostic’s rights row on its own source/license/notice review.

The older stack research says its earlier spike wanted to “links libgcc division,” while the current Phase 5 acceptance requires “no linked runtime library.” [VERIFIED: `.planning/research/STACK.md:327` and `.planning/ROADMAP.md:245`; quoted phrases verbatim] For the accepted assembly-only diagnostic, do not add a division probe that forces libgcc into the ROM. Treat libgcc division resolution as an unresolved future freestanding-C concern unless Phase 5 scope is explicitly changed.

## Recommendation and alternatives

### Recommended path

1. Keep `scripts/fetch-pin.sh` and fixture `pins.txt` behavior unchanged. Add a small, reviewed toolchain lock with one fixed official release URL, host/architecture, exact expected byte count, SHA-256, and the narrowly allowed redirect host for each xPack archive. Keep the lock distinct from fixture pins; keep downloads and extraction under ignored `build/`.
2. Use a dedicated toolchain fetch/extract path. Require HTTPS to the pinned GitHub release URL, permit at most one redirect only after checking its scheme and exact host (`release-assets.githubusercontent.com` was observed for this release), and never persist or print the signed redirect URL. Fail closed if the redirect shape/host changes. Do not use an unrestricted `curl -L` or raise the generic fixture cap.
3. Before extraction, verify exact archive byte count and SHA-256 in a temporary file. Extract only after both match. First inspect archive member paths and types; reject absolute paths, `..`, escaping links, special nodes, or unexpected top-level layout. Extract with ownership/privileged mode preservation disabled into a fresh private temporary directory, verify the resulting paths remain under it, and atomically promote only after checks pass. Any failure removes both temporary download and extraction trees and leaves the previous toolchain destination untouched.
4. Start with one tiny project-owned ARM/Thumb assembly diagnostic, project `crt0`, linker script, and header fixer. Save `readelf -A`, disassembly, linker map, allowed input list, final ROM SHA-256, and exact tool archive SHA/size for each host. Do this before adding a ROM digest to the fixture manifest.
5. Compare the macOS arm64 and Linux x64 final ROM files exactly (`cmp`), not by tool archive hash. Treat Linux aarch64 in the provided native Ubuntu 26.04 container as a third, separately labeled witness; it does not replace Linux x64 hosted evidence or macOS arm64 evidence. If macOS and Linux x64 output differs, record both outputs and diagnostics, designate the Linux x64 ROM digest as owner/canonical as the roadmap directs, and report the parity failure honestly.
6. Only after the spike passes, promote the Linux x64 xPack archive identity as the fixture manifest’s canonical build toolchain digest; retain all host archive identities in the toolchain lock/evidence. Add source-built diagnostics to the manifest only with their source, recipe, notices/license, BIOS mode, permitted use, ROM digest, and case IDs. Never put the compiled `.gba` files in source control.

### Alternatives and tradeoffs

| Option | Benefit | Cost / reason not preferred |
|---|---|---|
| Broaden generic `fetch-pin.sh` to follow redirects and raise the size ceiling | Reuses one helper | Expands every fixture download’s redirect and size authority; conflicts with the tested redirect refusal and makes large downloads valid everywhere. Keep the generic contract. |
| Dedicated per-tool lock and downloader | Exact asset size, hash, host, and redirect policy stay scoped to this large developer tool | Adds a small validator/fetch path and focused negative controls. This is the least disruptive fit for the current fixture contract. |
| Install via xpm/npm | Official xPack docs describe versioned local installation | Adds another package manager and package resolution surface; package wrapper version differs from the GitHub binary asset identity. No concrete need for this spike. |
| Ubuntu apt binutils fallback | Avoids xPack’s 300 MB archives and can produce a Linux-owned canonical digest | Different packaging/version path; no longer proves the pinned xPack cross-host output requirement. Keep as the explicitly documented fallback if the xPack spike fails or cannot fit the fixed hosted budget. |

## Runtime and supply-chain budget

The official archives are about 294–307 MB compressed; the current generic fetcher is capped at 256 MiB. The Phase 4 hosted evidence recorded “Ubuntu 26.04 GNU | success | 149 seconds” and “macOS 26 AppleClang | success | 140 seconds”; the same report says “within the unchanged 180-second job limit.” [VERIFIED: `.planning/phases/04-cpu-execution-core-and-headless-runner/04-HOSTED-CHECKS.md:14-28`; exact snippets quoted] Those are historical hosted observations, not a measurement of xPack download/extraction or the later main run.

**Planning implication:** Put download, verification, extraction, assembly/link, ELF checks, and ROM comparison in a focused job/target rather than appending roughly 300 MB of work to the existing near-limit C/runner jobs. Keep the hosted job limit at 180 seconds and do not add speculative caches. The first spike must measure fresh download + extraction + smoke-build wall time on the required runners, with a clear margin under 180 seconds. If it does not fit, stop the xPack CI integration and make the explicit Linux-owned fallback decision; do not silently skip the required build, loosen generic fetch limits, or increase timeouts.

During this session, only release metadata and a no-body redirect probe were requested; no toolchain binary was downloaded, extracted, or executed. GitHub API digests are a content-integrity pin source, not an independently verified publisher signature. No release attestation/signature chain was validated in this pass. [CITED: [GitHub release API](https://api.github.com/repos/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tags/v15.2.1-1.1)]

## First-plan evidence sequence and failure controls

1. Add only the toolchain lock and fetch/extract behavior first; preserve the existing network-free fixture fetch controls. Prove a valid synthetic redirect to the allow-listed host succeeds without network; controls for wrong host, HTTP downgrade, multiple redirect, bad status, wrong size, wrong SHA, empty file, archive traversal/link escape, unsupported member type, and corrupted gzip all fail.
2. For every rejected request/archive, assert nonzero status with a stable reason, no extracted destination, no leftover temporary archive/tree, no writes outside the toolchain root, and no overwrite of a previously valid install. Confirm a failed hash never calls the extractor. Never log the time-limited redirect query string.
3. Download the real exact-pinned archive only in the approved spike plan; record transfer and extraction times separately. Check version output and archive/license inventory after integrity verification, then assemble/link the project-owned minimal image.
4. Inspect attributes on each input object and final ELF; require the exact v4T final ELF tag, audit executable-section disassembly, and preserve the link map. Verify map inputs contain only project objects and no runtime/archive member. Verify the final ROM has no Nintendo logo bytes through the existing signature scan and compare macOS arm64 and Linux x64 outputs before any ROM digest is admitted.
5. Run the same source in native Linux aarch64 as a third witness if available within the spike scope. Label it local container evidence, not hosted Linux x64 proof.
6. Run the produced diagnostics through the already implemented runner/mailbox, with each intentionally broken variant yielding `GUEST_FAIL` and the failing case ID. Keep the absent-toolchain case an explicit `SKIPPED` with reason; do not allow that skip to satisfy the required hosted diagnostic-build gate.

## Unknowns to keep explicit

- xPack 15.2.1-1.1’s actual assembler/linker attributes for the project’s objects, and whether all three host assets produce identical ROM bytes: unmeasured.
- Fresh download, extraction, and minimal-build time on each required host under the fixed 180-second limit: unmeasured. Expanded archive size and member/link layout were not inspected because the archive was not downloaded.
- Exact license/notice inventory of the packaged binaries and libraries: not established from the packaging repository’s MIT license page. Inspect after archive digest verification; do not infer that the package-level MIT text grants rights to every component or fixture output.
- Whether the release is backed by a separate signed checksum/provenance attestation: not verified. The observed GitHub API `digest` and locally committed SHA pins establish exact bytes, not publisher identity or build provenance.
- Whether the official GitHub release redirect host will remain unchanged: not promised by release metadata. The dedicated fetch path should fail closed on any unreviewed host/redirect change.

## Sources

### Primary external sources

- [xPack `arm-none-eabi-gcc` latest release API](https://api.github.com/repos/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/latest) and [v15.2.1-1.1 asset page](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v15.2.1-1.1) — current release tag, host archives, sizes, asset hashes.
- [xPack install guide](https://xpack-dev-tools.github.io/arm-none-eabi-gcc-xpack/docs/install/) — distinguishes xpm package install from manual binary archive use.
- [xPack package repository LICENSE](https://raw.githubusercontent.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/xpack/LICENSE) and [README](https://raw.githubusercontent.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/xpack/README.md) — scope of the package source’s MIT notice.
- [Arm ABI Addenda32](https://github.com/ARM-software/abi-aa/blob/main/addenda32/addenda32.rst) and [AAELF32](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst) — ARM ELF architecture and instruction-set attributes.
- [GNU readelf documentation](https://sourceware.org/binutils/docs/binutils/readelf.html) and [GNU ld map documentation](https://sourceware.org/binutils/docs/ld/Options.html) — what attribute and link-map outputs report.
- [GCC 15.2 `libgcc` documentation](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gccint/Libgcc.html), [GCC Runtime Library Exception FAQ](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html), and [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html#CanIUseGPLToolsForNF) — limits of runtime-library and compiler-tool license claims.

### Project source files read

- `.planning/ROADMAP.md:238-251`; `.planning/REQUIREMENTS.md:31-45,66-71`
- `.planning/config.json:1-8,104-107`
- `.planning/phases/04-cpu-execution-core-and-headless-runner/04-CONTEXT.md:6-19,84-94`
- `.planning/phases/04-cpu-execution-core-and-headless-runner/04-HOSTED-CHECKS.md:14-28`
- `.planning/phases/04-cpu-execution-core-and-headless-runner/04-FINAL-CHECKS.md`
- `.planning/research/STACK.md:123-135,320-328`
- `scripts/fetch-pin.sh:19-46,159-188`; `pins.txt:1-16`; `cmake/CheckManifest.cmake:20-38,68-93,494-581`; `tests/gates/fetch_case.cmake:286-294`
