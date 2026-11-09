# Phase 1: CI and Verification Foundation - Context

**Gathered:** 2026-11-09
**Status:** Ready for planning

<domain>
## Phase Boundary

From the first code commit, one command (`cmake --workflow --preset ci`) gives
the same verdict locally and as the required GitHub check `CI required`. The
phase delivers: a skeleton C17 core library and headless runner (no emulation
behavior), CTest with a junit-backed test-inventory check, ASan+UBSan lanes,
a repository hygiene check (credentials, non-noreply email, home-directory
paths), a rights signature scan (Nintendo logo, known BIOS, out-of-allowlist
ROM/save files), and a negative control for every gate.

Requirements: VERIF-01, VERIF-02, VERIF-03, VERIF-05, VERIF-06, VERIF-07,
EVID-04. Not in scope: fixture manifest schema and pinned fetch (Phase 2),
any bus/CPU/ROM-loading behavior (Phases 3-4), toolchain SKIPPED reporting
(VERIF-04, Phase 5), LICENSE file (owner gate G1, do not add).

</domain>

<decisions>
## Implementation Decisions

### Verify command and gate mechanics
- `cmake --workflow --preset ci` runs configure, Release build with warnings
  as errors, CTest, the hygiene and signature scans, the test-inventory check,
  and all negative controls. ASan+UBSan is a separate `asan` workflow preset
  run in Linux CI (Clang). Local macOS sanitizer parity is not relied on.
- Gate scripts are POSIX `sh` plus CMake `-P` scripts. No Python or other new
  runtime dependency (sibling cogmaster/gabbaboy pattern).
- Each negative control is a CTest test that runs a gate against planted
  input and passes only when the gate fails with its stated reason (matched on
  the reason text). A negative control that passes makes the verify fail.
- The test inventory is a sorted, checked-in `tests/inventory/<lane>.txt` per
  lane. It is compared against registered tests at configure time, and after
  the run against `ctest --output-junit`. Skipped tests are listed with their
  reason; a missing or silently skipped test fails.

### Skeleton core and rights scan
- Skeleton: a `gamebloke_core` static library with no host includes, exposing
  only `gbl_version()` under the `gbl_` prefix in
  `include/gamebloke/gamebloke.h`. A `gamebloke-run` runner prints version and
  usage with documented exit codes. No lifecycle API yet; Phases 3/4 define it.
- Tests: CTest plus an in-tree ~40-line `check.h`. No test framework.
- Signature scan commits only SHA-256 digests, never logo or BIOS bytes. It
  hashes the 156-byte header window at offset 0x04 of every binary file,
  hashes every file of exactly 16384 bytes against known BIOS digests, and
  fails on ROM/save extensions or binaries not on the allowlist.
- Negative controls synthesize random stand-in "logo" and "BIOS" blobs at test
  time, register their digests through a test-only signature list, and plant
  them, so every matching path is exercised. A separate check confirms the
  committed real digests are well-formed and in the right place. An optional
  local check against an owner-supplied file reports SKIPPED when it is absent.
- Hygiene negative controls plant credentials, a non-noreply email, and an
  absolute home-directory path at test time (synthesized, not committed in a
  form that trips the scan on the tree).

### GitHub CI and repository actions (owner-authorized 2026-11-09)
- Runner labels are verified against GitHub's current runner-images list
  during research. The PR gate uses one Linux x64 image (GCC build; Clang for
  ASan/UBSan) and one macOS arm64 image, pinned by explicit version, never
  `-latest`. Every action is pinned by full commit SHA with a version comment.
- Single required check `CI required` that passes only if every job did. No
  trigger-level path filters; a `changes` job classifies PRs so
  `.planning/`-only PRs skip heavy lanes and still report passed.
- The owner authorized pushing a phase branch to `origin`, opening two real
  PRs (one `.planning/`-only, one touching source) to prove routing, recording
  their run IDs as evidence, and squash-merging to `main` once green.
- The owner authorized applying a `main` branch ruleset requiring
  `CI required`, via a checked-in, re-runnable `scripts/apply-repo-settings.sh`.
- Add a Conventional-Commits PR title check and an opt-in local
  `hygiene.sh --staged` pre-commit hook.

### Claude's Discretion
- Exact preset names beyond `ci` and `asan`, file layout under `scripts/`,
  `cmake/`, `tests/`, warning flag set, job timeouts (derive from measured
  runs, "2 x slowest", marked provisional).
- `permissions: contents: read`, `persist-credentials: false`, concurrency
  that cancels superseded PR runs but never push runs (sibling pattern).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- No source code in this repo yet; this phase creates the first code commit.
- Sibling patterns to study, not copy blindly (engineering lessons only; their
  hardware, gates, and stops do not transfer):
  - `~/projects/cogmaster/scripts/hygiene.sh` (POSIX scanner: home paths,
    emails with noreply allowlist, ROM by name/content, unlisted binaries,
    `--staged`/`--tree`/`--history` modes).
  - `~/projects/cogmaster/scripts/classify-changes.sh` and
    `.github/workflows/ci.yml` (`changes` job, `CI required` aggregator,
    SHA-pinned actions, title check, junit "every test ran" step).
  - `~/projects/cogmaster/cmake/CogmasterTests.cmake` (configure-time
    inventory check via `cmake_language(DEFER ...)`).
  - `~/projects/gabbaboy/cmake/ExpectedTests.cmake`,
    `ScanForbiddenFixtures.cmake`, `ScaleSanitizerTimeouts.cmake`.

### Established Patterns
- C17, extensions off; CMake 3.25 floor, presets schema 6, Ninja (STACK.md).
- Core built free of host and libc helpers where possible
  (`-fno-stack-protector -U_FORTIFY_SOURCE` precedent) so hashes match across
  platforms.

### Integration Points
- `.planning/` is the only existing tree; the hygiene scan must pass on it
  (it is tracked and public).
- Git identity must be a noreply address; the existing history must pass
  `--tree`. A `--history` mode is useful but scanning history is not required
  by the success criteria.

</code_context>

<specifics>
## Specific Ideas

- UBSan negative control: a deliberate shift by 32 (or equivalent) built only
  in the sanitizer lane, expected to be caught with the UBSan reason text.
- The verify command itself must fail if any negative control passes.
- Research flag: GitHub runner image names in research are [UNK]; verify.

</specifics>

<deferred>
## Deferred Ideas

- MSVC, Windows, Linux arm64, macOS Intel legs: scheduled lane later, not the
  PR gate.
- libFuzzer, nofp, be32, wasm32 lanes: once parsers and core substance exist.
- Hygiene `--history` scan in CI: optional; not a success criterion.

</deferred>
