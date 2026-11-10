# Phase 03 Plan 06: Execution Evidence

**Result:** Local automated evidence passed for the integrated bus/loader path. The canonical-logo acceptance remains partial and owner-gated; this file does not seal Phase 03 or any hardware claim.

## Environment

- Host: macOS (`Darwin arm64`).
- CMake: 4.4.3 (`/opt/homebrew/bin/cmake`).
- CI and hygiene build compiler: `/usr/bin/cc`, AppleClang 21.0.0.21000101.
- ASan/UBSan compiler: `/usr/bin/clang`, Apple clang 21.0.0.21000101. The preset's `clang-21` executable is absent, so the local command supplied the requested `CMAKE_C_COMPILER` override. `CMakePresets.json` and CI compiler mappings were not changed.
- Fixtures were synthesized in memory. No game ROM, BIOS bytes, owner logo bytes, or owner digest were retrieved or manufactured. `tests/signatures/logo.sha256` was not opened.

## Commands and results

| Exact command | Result and measured time |
|---|---|
| `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^loader\.integration$' --no-tests=error --output-on-failure` | Passed 1/1 twice: first run CTest 0.51 s / command 0.9 s; tracer feedback rerun CTest 0.29 s / command 0.5 s. The test carried `core`, `integration`, `loader`, and `unit` labels. |
| `cmake --workflow --preset ci` | Final post-validator-change run passed; JUnit records 124.68 s CTest time, 0.2 s configure, build `ninja: no work to do`, 0.13 s post-check. Inventory: 292 expected, 290 ran, 2 skipped and allowed. |
| `cmake --workflow --preset hygiene` | Final post-validator-change run passed; JUnit records 74.53 s CTest time, 0.2 s configure, build `ninja: no work to do`, 0.08 s post-check. Inventory: 200 expected, 198 ran, 2 skipped and allowed. |
| `sh scripts/hygiene.sh --tree` | Passed after evidence file creation: `hygiene: ok (177 files)`, 4.23 s. |
| `CMAKE=$(command -v cmake) sh scripts/signature-scan.sh --tree` | Passed after evidence file creation: `signature: ok (177 files)`, 3.38 s. |
| `cmake --preset asan -DCMAKE_C_COMPILER="$(command -v clang)" && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post` | Passed with AppleClang 21.0.0.21000101. Configure reported 0.6 s; build compiled and linked all 17 target steps. The CTest lane passed 36/36 with 0 skips (CTest reported 41.58 s); the executor-observed end-to-end command wall was about 35.2 s across its two tool waits. `asan-post` passed in 0.02 s with inventory 36 expected / 36 ran / 0 skipped. |

The CI and hygiene inventories both reported the same allowed owner skips:

- `signature.logo.real_digest`: `logo digest owner-pending (tests/signatures/logo.sha256; owner action in plan 11)`.
- `signature.owner_local`: `GBL_OWNER_LOGO_FILE not set`.

The sanitizer lane instrumented `src/bus.c`, `src/loader.c`, and `src/machine.c`. Ordinary bus, loader, and integration behavior tests were clean. The `bus.pos.clean` and `loader.pos.clean` companions passed. The ASan heap-overflow and UBSan shift controls emitted their expected diagnostics, which their wrappers matched; the ASan control also printed external-symbolizer startup warnings but still emitted the required heap-buffer-overflow report.

## Integration coverage

`loader.integration` loads four equal synthetic images through four separate host allocator contexts and input buffers. It checks owned-copy behavior after caller edits, all three ROM aliases, mirrored EWRAM/IWRAM, a VRAM byte-write boundary on both sides of the modeled display-mode threshold, and explicit internal cycles. Each bus request checks status, outcome, value, class, cost, and resulting clock. A machine run alone is compared field-by-field and by digest with two machines receiving the same sequence interleaved. Repeated loaded reset is compared with a fresh loaded machine. The selector also invokes the existing transaction and diagnostics assertions for refusal/allocation failure atomicity, candidate commit-before-release, advisory diagnostics, and independent disposal. It prints `loader: integration ok` only after those assertions pass.

These results show the selected deterministic project contract. They do not establish CPU or guest execution, an installed-consumer route, a stable ABI, physical hardware accuracy, or broad game compatibility.

## Critical path review

The final CI run spent 124.68 s in CTest; the largest measured label was `control` at 82.38 s. Hygiene spent 74.53 s in CTest, with `control` at 46.84 s. Configure took 0.2 s per workflow, and the product build was already current. Each workflow performed one configure/build pair; there was no duplicate product compilation or repeated setup to remove. The five slowest CI tests were `hygiene.tree` (9.81 s), `signature.tree` (9.58 s), `bus.nc.video_bytes` (3.76 s), `signature.signal` (3.39 s), and `hygiene.signal` (3.20 s). Hygiene's slowest were `hygiene.tree` (9.30 s), `signature.tree` (6.33 s), `manifest.nc.missing_field` (3.70 s), `signature.signal` (3.05 s), and `hygiene.signal` (3.04 s). The latest CI run began at 17:03:57 and ended at 17:06:02; hygiene began at 17:06:11 and ended at 17:07:25, so they did not overlap. The increase over earlier CI/hygiene samples is not explained by retained logs. No safe evidence-preserving reduction was supported by these measurements, so no cache, dependency, or concurrency change was made.

The final `profile.nc.bus_policy_doc` control took 3.15 s in CI and 2.88 s in hygiene, below the five slowest tests in both lanes. Its validator calls exercise distinct adversarial document mutations, so combining them would remove independent refusal evidence. A prior 0.77 s sample shows this control's runtime varies as well; the cause of that variation and the broader increase from earlier 49–97 second CI samples is not established by retained logs. No evidence-supported reduction to the profile controls was identified.

The canonical profile document has a deliberately narrow supported format: plain Markdown with supported fenced/indented examples and HTML comments, and no unsupported raw HTML blocks. The validator removes examples and comments once, rejects unsupported raw HTML across that normalized document, then uses only that same visible content for version, heading, section, and table discovery. Keeping discovery on one representation prevents hidden or cross-section content from shadowing the synchronized profile. Its controls cover commented headings/tables, adjacent duplicate versions, and an HTML block opened before and closed after a synchronized section.

## Remaining gates and evidence limits

- **LOAD-02 remains PARTIAL/Pending.** Checksum, game-code, synthetic byte-span comparison, and unchecked advisory behavior are verified. The real canonical-logo mismatch result remains `UNCHECKED`/`OWNER_PENDING`; synthetic comparisons do not satisfy it. The owner is the next actor: run the existing digest workflow on a file already held and supply the approved digest through the established review path.
- **BUS-02 ROM-tail hardware behavior remains unverified.** Tests verify only the selected current-latch project policy. The core maintainer owns any future hardware evidence. No hardware claim is made.
- The three probe classifications that remain unresolved are BUS-02 (full map/tail scope), LOAD-02 (canonical logo), and LOAD-03 (refusal/ownership atomicity). Automated tests verify the concrete current contracts, but do not retroactively mark these probe items classified.
- The two retained descriptor-less prohibitions remain flagged-unverified for evidence review: do not present project timing/fill/latch/ROM-tail policies as measured hardware or broad compatibility, and do not treat synthetic logo comparison or matching hashes as canonical validation or rights evidence.
- No CI run was performed against a remote service, no PR or push was created, and no phase transition or release publication occurred. Orchestrator-owned source review and phase bookkeeping remain outstanding.

## Daemon recovery verification

The saved branch contained all six completed plans. The restart left two table
review findings open and no phase verification report. Recovery added exact
Unmodeled occurrence/count checks and rejected unsupported row continuations,
including non-heading `#text`. Both control groups failed before correction;
all 29 profile checks passed afterwards. The recovered independent delta review
is clean.

On the final recovery tree, canonical CI ran 290 of 292 expected tests (82.52 s
CTest), with only the two permitted owner-logo skips. Hygiene ran 198 of 200
(45.92 s), with those same skips. Both inventory post-checks passed. Prior lane
inventory entries are all retained. These are local measurements, not hosted CI
or emulator-throughput qualification. No duplicate build occurred; runtime
variation is not attributed to a cause. A surviving CTest process was confirmed
to belong to another project and left untouched.
