---
phase: 01-ci-and-verification-foundation
plan: 05
subsystem: infra
tags: [asan, ubsan, sanitizers, clang, cmake, ctest, presets, docker, linux, negative-controls]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "gbl_add_test, inventory compare and junit check, gbl_expect_fail wrapper, core and runner tests, GBL_LANE lane selection (plans 01 to 04)"
provides:
  - "`cmake --workflow --preset asan`: Clang (`clang-21`) Debug build with `-fsanitize=address,undefined -fno-sanitize-recover=all`, core and runner tests under sanitizers, and a post step that checks tests/inventory/asan.txt"
  - "sanitizer.ubsan.shift32 and sanitizer.asan.heap_overflow: opaque shift-by-32 and heap-buffer-overflow controls that must exit non-zero AND print the sanitizer reason text (no exact exit code)"
  - "scripts/linux-lane.sh <workflow-preset> [gcc|clang]: local Linux runner in a digest-pinned ubuntu:26.04 container with a read-only source mount"
affects: [01-06, 01-09, 01-10, 01-11]

actuals:
  tokens: 5500
  tasks: 2
  commits: 2
plan_head_before: 3add0b16d37baadaaa455fd6eedcb91808ee1de2
plan_head_after: c8f87c8f6532b37f090caa5a82516cff2e2a077e

tech-stack:
  added: []
  patterns:
    - "Sanitizer controls live in tests/sanitizer.cmake, included only when GBL_LANE is asan, so no control target exists in the ci lane"
    - "Both asan test presets are verbose (B1) so passing controls' `expect-fail: ok:` lines and the inventory summary reach the log"
    - "Linux proof before CI: `sh scripts/linux-lane.sh <preset> [gcc|clang]` on the host's native architecture, building in a copy of the tree"

key-files:
  created:
    - tests/sanitizer.cmake
    - tests/ub_control.c
    - tests/asan_control.c
    - tests/inventory/asan.txt
    - scripts/linux-lane.sh
  modified:
    - CMakePresets.json
    - CMakeLists.txt

key-decisions:
  - "The asan preset carries -fno-sanitize-recover=all; removing it was observed to make sanitizer.ubsan.shift32 fail with `expect-fail: gate PASSED but must fail` (Pitfall 2 negative check)"
  - "linux-lane.sh installs libc6-dev explicitly: with --no-install-recommends the gcc package alone cannot link (Scrt1.o and crti.o missing); the clang-21 route pulls it in transitively"
  - "Image pinned by the multi-architecture index digest `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c` (pinned 2026-11-10); GBL_LINUX_IMAGE overrides it"

patterns-established:
  - "Pin an image by the digest `docker pull` prints (the index digest), not RepoDigests[0], which printed `ubuntu@sha256:...` without the tag"
  - "Lanes that need a toolchain absent on macOS are proven by the container runner first and by CI second"

requirements-completed: [VERIF-06, VERIF-02]

duration: 20 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 05: ASan and UBSan Lane with Controls and a Local Linux Runner Summary

**`cmake --workflow --preset asan` builds the core and runner with Clang 21 under `-fsanitize=address,undefined -fno-sanitize-recover=all`, passes the core and runner tests, catches both a shift-by-32 and a heap-buffer-overflow control by exit status plus sanitizer text (`inventory: expected 9, ran 9, skipped 0 (allowed 0)`), and `scripts/linux-lane.sh` proves it, and the GCC ci lane, in an ubuntu:26.04 container before any CI run.**

## Performance

- **Duration:** 20 min
- **Tasks:** 2
- **Files:** 5 created, 2 modified

## Accomplishments

- Presets: configure, build, and test `asan`, test `asan-post`, workflow `asan`, schema 6, no `$` keys. Both test presets are verbose (B1).
- Controls: `ub_control.c` shifts `1u` by the opaque `volatile unsigned n = argc + 31` (32 with no arguments); `asan_control.c` writes at `p[8 + argc - 1]` after `malloc(8)`. Both compile clean under the plan 01 warning set with warnings-as-errors on Clang and GCC and are built only in the asan lane.
- `tests/inventory/asan.txt` lists ten sorted lines (nine tests plus the label-`post` `inventory.junit`).
- The ci lane is unchanged: `ctest --test-dir build/ci -N` reports 63 tests and no `sanitizer.` entry.
- `scripts/linux-lane.sh` runs any workflow preset in a pinned container; usage errors exit 2 with `linux-lane: usage` before Docker is touched.

## Task Commits

1. **Task 1: asan workflow with UB and ASan controls and its own inventory** - `51b63d3` (feat)
2. **Task 2: local Linux runner proves the asan lane and the GCC ci lane** - `c8f87c8` (feat)

## Verification Evidence

Docker was available (Docker Desktop, daemon architecture `aarch64`, host Apple silicon), so the container runs were performed. No `--platform` flag was passed; the container ran natively on aarch64 (`uname -m` printed `aarch64`). The x86_64 Linux proof remains the ubuntu-26.04 CI runner (plan 09).

- Image: `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`.
- Toolchain printed in the container: CMake 4.2.3, GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), Ubuntu clang 21.1.8 (6ubuntu1).
- **asan lane** (`sh scripts/linux-lane.sh asan`, about 19 s including apt): exit 0; final lines `100% tests passed, 0 tests failed out of 9` and `inventory: expected 9, ran 9, skipped 0 (allowed 0)`. The log contains `ub_control.c:16:21: runtime error: shift exponent 32 is too large for 32-bit type 'unsigned int'` and `expect-fail: ok: /runtime error: shift exponent 32 is too large for 32-bit type/ (exit 1)`, and `expect-fail: ok: /AddressSanitizer: heap-buffer-overflow/ (exit 1)`. Linux exit code is 1, as predicted. Run twice (before and after the `libc6-dev` fix): same result.
- **GCC ci lane** (`sh scripts/linux-lane.sh ci gcc`): exit 0; `100% tests passed, 0 tests failed out of 62` and `inventory: expected 62, ran 60, skipped 2 (allowed 2)`, with `signature.logo.real_digest` and `signature.owner_local` reported as SKIPPED with reasons. No GCC-only warning appeared (Pitfall 11), so no source change was needed.
- **Pitfall 2 negative check** (manual, not committed): with `-fno-sanitize-recover=all` temporarily removed from the asan preset, `sh scripts/linux-lane.sh asan` exited 8 with `sanitizer.ubsan.shift32 ... Failed` and `expect-fail: gate PASSED but must fail (negative control broken)`, while the log still showed the `runtime error: shift exponent 32` line, which is exactly the recovered-and-exit-0 behavior the flag prevents. Preset restored (`git diff` clean).
- **macOS ci workflow** (`cmake --workflow --preset ci`): exit 0 with `inventory: expected 62, ran 60, skipped 2 (allowed 2)`.
- **Usage check**: `sh scripts/linux-lane.sh bogus-arg extra-arg` exits 2 and prints `linux-lane: usage`.
- **Read-only mount**: `git status --porcelain` showed only the new untracked script after every container run; the source tree is never written.
- Extra, not relied on: a scratch configure of the asan lane on macOS (Apple clang) in a temporary directory, outside the repo, also passed 9 of 9, which agrees with RESEARCH.md assumption A9; the lane stays proven on Linux per D-01.

Task acceptance criteria re-run: `grep -c 'fno-sanitize-recover=all' CMakePresets.json` prints 1; ci `-N` lists `inventory.junit` and no `sanitizer.`; asan inventory has 10 non-comment lines and `LC_ALL=C sort -c` passes; `grep -c 'ubuntu:26.04@sha256:' scripts/linux-lane.sh` prints 1.

## Findings for Later Plans

- **Plan 06 (CI asan job):** on ubuntu:26.04 the package names `clang-21` and `libclang-rt-21-dev` are correct (Assumption A3 confirmed). The container installed `cmake ninja-build git libc6-dev` plus the compiler; the GitHub ubuntu-26.04 runner image needs only `clang-21 libclang-rt-21-dev` if it lacks them. Ubuntu's CMake in the container is 4.2.3, above the 3.25 floor; the floor-version confirmation remains plan 09's job.
- `GBL_EXPECT_C_ID` (`GNU` or `Clang`) and `CC` are exported by the runner for plan 06's configure guard; the asan preset also hardcodes `CMAKE_C_COMPILER` `clang-21`, so the asan lane cannot run on a host without a `clang-21` binary.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] gcc container could not link without libc6-dev**
- **Found during:** Task 2, first `sh scripts/linux-lane.sh ci gcc`
- **Issue:** `apt-get install --no-install-recommends gcc` omits `libc6-dev`, so CMake's compiler check failed (`cannot find Scrt1.o`, `crti.o`). The clang route had masked it because `clang-21` depends on it.
- **Fix:** Added `libc6-dev` to the common apt install line in `scripts/linux-lane.sh`.
- **Files modified:** scripts/linux-lane.sh
- **Verification:** GCC ci lane then passed 62/60/2; asan lane rerun still 9/9/0.
- **Commit:** c8f87c8 (the fix was made before the script's first commit)

### Process notes (no code deviation)

**2. Direct commits on `main`:** committed locally on `main` as the orchestrator directed (`git.branching_strategy: none`); the protected-branch guard condition was expected and nothing was pushed or reconfigured.

**3. Image digest source:** the plan's `docker image inspect --format '{{index .RepoDigests 0}}'` returned `ubuntu@sha256:...` (no tag). The digest equals the `Digest:` line `docker pull ubuntu:26.04` printed (the multi-architecture index), so the script pins `ubuntu:26.04@sha256:<that digest>` explicitly.

**Total deviations:** 1 auto-fixed (Rule 3), 2 process notes. **Impact:** none on deliverables.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. T-01-13 (false green) is mitigated by the flag plus controls and the Pitfall 2 check above; T-01-14 by the digest pin, read-only mount, and local-only use; T-01-SC-05 holds (apt packages from the Ubuntu archive inside a throwaway container, no registry packages).

## Issues Encountered

None beyond the libc6-dev fix above.

## Next Phase Readiness

Ready for 01-06: the asan preset, controls, inventory, and the package names for the CI asan job are proven on Linux (aarch64); the first x86_64 Linux evidence is the ubuntu-26.04 CI run in plan 09.

## Self-Check: PASSED

- Created files exist: tests/sanitizer.cmake, tests/ub_control.c, tests/asan_control.c, tests/inventory/asan.txt, scripts/linux-lane.sh.
- Commits `51b63d3` and `c8f87c8` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 2.
