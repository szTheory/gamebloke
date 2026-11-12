# Plan 09 CI timeout fix

**Status:** Harness fix committed as `f6c7de5`; local verification complete. This does not establish that the hosted GNU job now fits its 180-second limit.

## Change

For the six CPU mutations that only change `src/cpu.c` (`pc_refill`, `bank_alias`, `exception_lr`, `vector_fetch`, `trace_loss`, and `budget_overshoot`), the harness now compiles the six unchanged translation units once per mutant/clean pair and links them into separate mutant and clean executables. It builds a distinct CPU object for each executable. This cuts each pair from 14 translation-unit compiles to 8.

Before reuse, the harness hashes every copied input and requires all files except the mutated `src/cpu.c` to match. It also requires different mutant/clean CPU source and object hashes, and separate executable paths. Compile and link errors stop the CTest case before it can count as semantic evidence. The existing exact mutant failure regex and clean companion assertions remain in place.

`shift_carry`, `peek_effect`, and any mutation not on the explicit allowlist keep the existing full rebuild path. There is no cross-case cache, concurrency, new dependency, timeout change, CI job, emulator change, or inventory change. The eight existing CTest cases provide the semantic and clean controls, so `tests/core.cmake` and `tests/inventory/ci.txt` did not need edits.

## Controlled mutation-matrix timings

Each row covers the same eight `cpu.nc.*` cases, run sequentially with CTest's normal non-verbose output. Baseline source is exact published head `967d7ef`; after source is `f6c7de5`. Native Mac baseline source was extracted from that exact commit, then configured with the same compiler and flags as the current-source run. The pinned Linux runs used `git clone --no-local` from a read-only `/src` mount into `/work` and the same pinned image before and after.

| Platform and configuration | Baseline | After | Observed change |
| --- | ---: | ---: | ---: |
| macOS arm64, AppleClang 21.0.0, Release/CI | 30.24 s | 21.15 s | 9.09 s faster (30%) |
| macOS arm64, AppleClang 21.0.0, ASan/UBSan | 33.05 s | 25.33 s | 7.72 s faster (23%) |
| Ubuntu 26.04 arm64, GCC 15.2.0, Release/CI | 48.72 s | 39.05 s | 9.67 s faster (20%) |
| Ubuntu 26.04 arm64, Clang 21.1.8, ASan/UBSan | 48.12 s | 36.00 s | 12.12 s faster (25%) |

Build flags matched within each before/after pair:

- Release/CI: `CMAKE_C_FLAGS=""`, `CMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG"`, and empty executable linker flags.
- ASan/UBSan: `CMAKE_C_FLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"`, Debug config `-g`, and linker flags `-fsanitize=address,undefined`.
- Linux used the pinned image `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`. Its reported architecture was `aarch64`.

CTest command for the timing runs was `ctest --test-dir <lane-build> --output-on-failure -R '^cpu\.nc\.'`. The after matrix was also run verbosely on all four configurations to retain the hash-guard, full-rebuild-path, named semantic failure, and clean-pass output; those verbose durations are not used in the table.

The first native Mac timing pass had unusually high variance: its Release baseline measured 69.56 s and the verbose after run 19.02 s. Unchanged full-rebuild cases also became much faster, so those numbers are retained as raw observations but excluded from the table. The table uses a repeat baseline extracted from exact `967d7ef` and a sequential, quiet after run. These are observed local timings, not a guarantee about the hosted runner's deadline.

## Canonical local CI

`cmake --workflow --preset ci` passed after the source change. The main CTest step reported 337 tests: 336 passed and the existing owner-local logo test was skipped because `GBL_OWNER_LOGO_FILE` was unset. The `ci-post` inventory check passed and accounted for all 337 entries. Main CTest time was 154.20 s on this Mac.

## Evidence files

Raw logs and timing receipts are ignored under `build/phase05-plan09/mutation-build-fix/`:

- Controlled native repeats: `repeat-before-macos-ci.log`, `repeat-after-macos-ci.log`, `repeat-before-macos-asan.log`, and `repeat-after-macos-asan.log`; matching configure logs and baseline CMake caches record the flags.
- Pinned Linux timings: `before-linux-gcc.log`, `after-linux-gcc-quiet.log`, `before-linux-clang21-asan.log`, and `after-linux-clang21-asan-quiet.log`.
- Verbose harness controls: `after-macos-ci.log`, `after-macos-asan.log`, `after-linux-gcc.log`, and `after-linux-clang21-asan.log`.
- Full native workflow: `native-ci-workflow.log`.
- Initial Mac runs, excluded from the controlled comparison: `before-macos-ci.log`, `after-macos-ci.log`, `before-macos.log`, and `after-macos-asan.log`.
- Two timing-wrapper setup attempts stopped before CTest because the minimal container lacked a `time` executable; their logs are `linux-gcc-wrapper-error.log` and `linux-gcc-time-wrapper-error.log`. Corrected runs used CTest's own elapsed-time output.

The report is intentionally uncommitted for the parent agent to review. The final full pinned Linux GCC run, full Clang 21 sanitizer/inventory run, and the single corrected hosted run remain parent-owned; no hosted run or CI configuration change was made here.


## Root final verification and scoped review

Independent05-09-CI-REVIEW.md is clean at exact sourcef6c7de5. Root sequentially ran the full pinnedUbuntu26.04 nativearm64GCC canonical CI and Clang21ASan/UBSan/LeakSanitizer suites atf6c7de5f894a66476cd0f037e290518b0de7f477: GCC336pass+oneexistingallowedownerlocalskip/post; Clang80/80zero skip/post. Logs/receipts are final-linux-gcc.log, final-linux-clang21-asan.log and matching-receipt.json under build/phase05-plan09/mutation-build-fix/. Hosted correction remains pending; no phase-wide completion is claimed.
