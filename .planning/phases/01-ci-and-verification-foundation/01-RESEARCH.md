# Phase 1: CI and Verification Foundation - Research

**Researched:** 2026-11-09
**Domain:** CMake/CTest verification harness, POSIX-sh repository gates, GitHub Actions required-check design, sanitizer lanes, rights/hygiene scanning (C17 skeleton, no emulation behavior)
**Confidence:** HIGH on CI/CTest/sanitizer mechanics (probed this session on CMake 3.25.2 and 4.4.3, Apple clang 21, Ubuntu 26.04 GCC 15.2 and Clang 21.1.8); MEDIUM on GitHub-side rollout order; LOW on one item: the Nintendo-logo SHA-256 is **not** published in any primary source I could reach (see Open Question 1).

**Evidence tags used.** `[VERIFIED: x]` = tool-confirmed this session against an authoritative source or a probe whose output I saw. `[CITED: url]` = official documentation. `[OBS: x]` = read or ran locally this session (true of that file or machine). `[ASSUMED]` = training knowledge or inference, needs confirmation. The project's own labels map as PF=VERIFIED/CITED, INF/UNK=ASSUMED.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Verify command and gate mechanics
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

#### Skeleton core and rights scan
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

#### GitHub CI and repository actions (owner-authorized 2026-11-09)
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

### Deferred Ideas (OUT OF SCOPE)
- MSVC, Windows, Linux arm64, macOS Intel legs: scheduled lane later, not the
  PR gate.
- libFuzzer, nofp, be32, wasm32 lanes: once parsers and core substance exist.
- Hygiene `--history` scan in CI: optional; not a success criterion.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| VERIF-01 | One canonical command (`cmake --workflow --preset ci`), identical to the required CI check | Schema-6 workflow preset with a second `test` step that runs the junit inventory check (prototype passed on CMake 3.25.2 and 4.4.3). CI jobs call only `cmake --workflow --preset <lane>`. |
| VERIF-02 | Every gate has a negative control that must fail for a stated reason; verify fails if one passes | `ExpectFail.cmake` wrapper (non-zero exit AND stable reason code), paired positive controls, and a meta-control proving the wrapper itself fails when a gate passes. |
| VERIF-03 | CTest + junit inventory check fails on missing/silently skipped tests; skipped listed with reason | Probed junit shapes for pass/fail/skip/disabled/not-run/timeout (see Pitfall 1 and Pattern 3); `ctest` exits 0 on skipped and disabled tests, so the check is mandatory. |
| VERIF-05 | Single required `CI required` check on PRs and `main`, Linux (GCC) + macOS arm64, SHA-pinned; `.planning/`-only skips heavy lanes | Verified runner labels, image toolchains, action SHAs; aggregator via `needs` + CMake `string(JSON)` script; classifier from sibling with rename handling. |
| VERIF-06 | ASan + UBSan builds of core and runner in CI | `asan` preset, Clang on `ubuntu-26.04`; `-fno-sanitize-recover=all` is mandatory (recoverable default exits 0). UB control = opaque shift by 32, observed on Apple clang 21, Clang 21.1.8, GCC 15.2. |
| VERIF-07 | Hygiene check fails on credentials, non-noreply emails, absolute home paths in tracked files | POSIX `hygiene.sh` (`--tree/--staged/--dir`), regex sets tested against the current tree (zero hits), planted-in-pieces controls. |
| EVID-04 | Signature scan fails on Nintendo logo bytes, known BIOS, ROM/save outside allowlist | Digest-only scan; window extraction via `dd bs=4 skip=1 count=39` + `cmake -E sha256sum` (verified equal to `shasum -a 256`); BIOS digests verified from two independent docs; logo digest [UNK]. |
</phase_requirements>

## Summary

Phase 1 is a harness phase with a tiny C skeleton. Every mechanism it needs was either proven in a sibling (cogmaster: junit inventory, `CI required` aggregator, classifier, hygiene; gabbaboy: expected-tests, forbidden-fixture scan) or probed this session. The central design is: one CMake **workflow preset** whose steps are configure, build, `test` (writes junit), and a second `test` step that runs a post-hoc CTest test reading that junit. This keeps "the same verdict locally and in CI" without any CI-only shell step. It works at preset schema 6 on CMake 3.25.2 and 4.4.3 [VERIFIED: prototype run this session].

Three facts shape every plan: (1) `ctest` exits 0 when tests are skipped or disabled, so without the junit inventory check a "green" run can silently omit tests [VERIFIED: ran, rc=0 with a skipped and a disabled test]. (2) UBSan without `-fno-sanitize-recover=all` prints the error and still exits 0 [VERIFIED: Apple clang 21, Clang 21.1.8, GCC 15.2 all rc=0]. (3) Exit codes after a sanitizer abort differ by platform (macOS 134, Linux 1), so negative controls must assert "non-zero AND stable reason text", never an exact code [VERIFIED].

Two things are not settled by research and need the owner or a checkpoint: the SHA-256 of the 156-byte Nintendo logo window (no primary source publishes it, and I deliberately did not fetch logo bytes), and the scope of `apply-repo-settings.sh` beyond the authorized ruleset. A third operational trap: once the `main` ruleset is active (PR required, no bypass), GSD's direct-to-main commits (`git.branching_strategy: none` in `.planning/config.json`) will be rejected, so apply the ruleset as the last step and switch the branching strategy.

**Primary recommendation:** Build the harness as CTest tests registered at top-level directory scope, with per-lane sorted inventories, a `post`-labelled junit check as the workflow's final step, POSIX-sh gates emitting stable `gate: reason-code: path` lines, and `ExpectFail.cmake` negative controls. Prove CI routing with two real PRs in this order: the source PR first (it carries `ci.yml`, so the workflow exists on `main`), then the `.planning/`-only PR, then apply the ruleset last.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Canonical verify command | Build system (CMake workflow preset) | CI (calls it verbatim) | One definition; CI must not add logic the local run lacks. |
| Test registration + inventory | Build system (CMake, configure time) | Gate script (junit, run time) | Configure-time compare catches removed registrations; junit compare catches skipped/not-run. |
| Hygiene and signature gates | Repo scripts (POSIX sh, CMake -P) | CTest (runs them + negative controls) | No new runtime; tests double as the negative-control harness. |
| Sanitizer enforcement | Compiler flags in `asan` preset | CTest negative controls (asan lane only) | Control must be built with the same flags it proves. |
| Change classification (`.planning/`-only) | CI (`changes` job, script in repo) | CTest (fixture tests of the script) | Needs GitHub API; the logic is a testable shell script. |
| `CI required` aggregation | CI (workflow job) | CMake script + CTest cases | Predicate lives in a script so it is unit-tested, not only exercised live. |
| Branch protection | GitHub ruleset (committed JSON) | `scripts/apply-repo-settings.sh --check` | Ruleset is repo state outside git; the script is the re-runnable record. |
| PR title policy | CI (`title` job) | Fixture test of `check-title.sh` | Squash message comes from GitHub, outside the commit hook's reach. |

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| CMake | floor 3.25 (schema 6); local 4.4.3, runners 4.4.5 | Presets, workflow, CTest, `cmake -E sha256sum`, `string(JSON)` | Workflow presets and `outputJUnitFile` arrive together at schema 6 / CMake 3.25 [CITED: cmake.org/cmake/help/latest/manual/cmake-presets.7.html]. |
| Ninja | 1.13.2 (local and both runner images) | Generator | Sibling standard; present on images [VERIFIED: runner-images READMEs]. |
| C17, extensions OFF | n/a | Skeleton core + runner | Locked by STACK.md. |
| POSIX `sh` + `awk` + `dd` | dash on ubuntu-26.04; BSD tools on macOS | Gates | Sibling-proven under both dash and macOS. |
| `gh` CLI | 2.103.3 on runners; 2.101.0 local (authed, admin on repo) | Classifier input, title read, ruleset apply | Preinstalled on runners [VERIFIED: runner-images]. |
| GCC | 15.2.0 on `ubuntu-26.04` | Linux release build | [VERIFIED: Ubuntu2604-Readme.md]. |
| Clang | 20.1.8 / 21.1.8 / 22.1.2 on `ubuntu-26.04` (use `clang-21`) | ASan/UBSan lane | Versions verified; the `clang-21` binary name and need for `libclang-rt-21-dev` come from the sibling workflow [OBS: cogmaster ci.yml] and are [ASSUMED] for this image until the first CI run. |
| Apple clang | 21.0.0 on `macos-26` (matches the owner's local host exactly); 17.0.0 on `macos-15` | macOS arm64 leg | [VERIFIED: runner-images READMEs; local `clang --version` = 21.0.0]. |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `actions/checkout` | `3d3c42e5aac5ba805825da76410c181273ba90b1` # v7.0.1 | Source checkout | Every job. SHA confirmed to be the commit of tag v7.0.1 and the latest release [VERIFIED: `gh api repos/actions/checkout`]. |
| `actions/upload-artifact` | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` # v7.0.2 | Upload junit/log on failure | Optional (diagnostic only) [VERIFIED: `gh api repos/actions/upload-artifact`]. |
| `jq` | 1.8.1 ubuntu / 1.8.2 macOS image; `/usr/bin/jq` local | Only if the aggregator or ruleset drift check uses it | Avoidable: use `cmake -P` + `string(JSON)` for the aggregator; `apply-repo-settings.sh` may use `jq` (sibling does). |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Post-test CTest step in the workflow | Separate CI "Every test ran" step (cogmaster) | CI-only step breaks "same verdict locally". Cogmaster's own pattern runs it in CI only; Gamebloke's decision requires it inside the workflow. |
| `macos-26` | `macos-15` (sibling choice) | `macos-15` is Apple clang 17 / Xcode 16.4; `macos-26` is clang 21.0.0 / Xcode 26.6, identical to the owner's local compiler. Recommend `macos-26` for parity [INF]; planner may choose `macos-15` for older-clang coverage. |
| awk junit parser | CMake `string(REGEX)` | `system-out` can contain `;`, which corrupts CMake lists; awk `split()` on `<testcase ` is sibling-proven. |
| Clang for sanitizers on Linux | GCC `-fsanitize=address,undefined` | GCC reports an ASan-class write as a UBSan "insufficient space" message, so ASan reason text differs per compiler [VERIFIED: GCC 15.2 run]. Locked decision says Clang. |

**Installation:** No packages are installed from npm/PyPI/crates. One optional apt step on the Linux sanitizer job (sibling pattern):
```bash
dpkg -s libclang-rt-21-dev >/dev/null 2>&1 || { sudo apt-get update && sudo apt-get install -y libclang-rt-21-dev; }
```
**Version verification (this session):** runner images and tool versions from `actions/runner-images` READMEs (Ubuntu 26.04 image 20261101.186.1, macOS 26 arm64 image 20261102.0473.1, macOS 15 arm64 image 20261104.0514.1); action SHAs via `gh api`.

## Package Legitimacy Audit

No npm, PyPI, or crates packages are required by the phase. Items touched:

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| `cmake` (Kitware PyPI wheel; only for an optional 3.25-floor CI/local check) | PyPI | long-lived | seam reports unknown | github.com/scikit-build/cmake-python-distributions | SUS (`unknown-downloads`) | Flagged. Not needed for the core plan. If a floor lane is added, planner inserts `checkpoint:human-verify`, or fetches the official Kitware release tarball by pinned SHA-256 instead. In this session `pip install cmake==3.25.2` was used only in a throwaway venv for probing; 3.25.3 was not available from the index. |
| `libclang-rt-21-dev` | Ubuntu apt archive | n/a | n/a | Ubuntu archive | not applicable to the seam | Sibling-proven conditional install on the runner; tag [ASSUMED] for this image. |
| GitHub Actions `actions/checkout`, `actions/upload-artifact` | GitHub | n/a | n/a | github.com/actions/* | verified by tag-to-commit lookup | Approved (SHAs above). |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** `cmake` (PyPI), only if the optional floor lane is built.

## Architecture Patterns

### System Architecture Diagram

```
 developer / pre-commit hook                    GitHub pull_request / push main
        |                                                    |
        |  hygiene.sh --staged (opt-in)                      v
        v                                      +-------------------------+
 cmake --workflow --preset ci  <---same-----   | changes (gh api files)  |--> heavy=true|false
        |                      command        +-------------------------+
        v                                       |        |          |
  [configure] --registers tests (top-level dir) |        |          |
        |        '-> DEFER: registered list == tests/inventory/<lane>.txt ?  (fail at configure)
        |        '-> removes stale build/<lane>/junit.xml
        v                                       v        v          v
  [build] Release, -Werror              hygiene(always) title(PR)  build matrix (heavy)  asan (heavy)
        v                                       |        |          |   ubuntu-26.04 GCC    ubuntu-26.04 clang-21
  [test: main] ctest --output-junit             |        |          |   macos-26 AppleClang   cmake --workflow --preset asan
        |   unit tests; gates run on the real tree; negative controls (ExpectFail)
        v
  [test: post] inventory.junit  --reads--> build/<lane>/junit.xml
        |   fails on: missing / extra / duplicate / failed / skipped-not-allowed / skipped-without-reason
        |   prints:  SKIPPED <name>: <mechanism> | <reason>
        v
  exit 0 / non-zero  ----------------------> every job result ---> CI required (always runs)
                                                                    passes iff all success,
                                                                    or skipped-as-allowed when heavy=false
```

### Recommended Project Structure
```
CMakeLists.txt              # project(gamebloke LANGUAGES C); GBL_LANE = ci|asan|hygiene; includes test files
CMakePresets.json           # version 6; no $comment, no $schema, no "jobs": ""
include/gamebloke/gamebloke.h   # gbl_version() only
src/version.c
runner/main.c               # gamebloke-run: --version, --help, documented exit codes (0 ok, 2 usage)
cmake/
  GblTests.cmake            # gbl_add_test(), configure-time inventory compare (DEFER)
  InventoryCompare.cmake    # shared compare function, also runnable with -P (for its negative control)
  ExpectFail.cmake          # negative-control wrapper
  CiRequired.cmake          # aggregator predicate (reads NEEDS json from env)
scripts/
  hygiene.sh                # --tree | --staged | --dir <path> | --history (optional)
  signature-scan.sh         # digest + extension + allowlist gate
  check-junit.sh            # sh+awk junit inventory check
  classify-changes.sh       # .planning/-only classifier
  check-title.sh            # Conventional Commits title check
  apply-repo-settings.sh    # ruleset (and --check drift)
  check-action-pins.sh      # every uses: is @<40 hex> # vX
.githooks/pre-commit        # exec scripts/hygiene.sh --staged   (opt-in: git config core.hooksPath .githooks)
.github/workflows/ci.yml
.github/rulesets/main.json
tests/
  check.h                   # ~40 lines
  version_test.c  ub_control.c  asan_control.c
  inventory/{ci,asan,hygiene}.txt
  signatures/{logo.sha256,bios.sha256,allowlist.txt}
  fixtures/{needs-cases.txt,classify-cases.txt,titles.txt}
  gates.cmake  sanitizer.cmake  ci_gates.cmake   # test registrations (include(), not add_subdirectory())
```

### Pattern 1: Schema-6 workflow with a post-hoc inventory step
**What:** The `ci` workflow is configure, build, test (excluding label `post`, writes junit), test (only label `post`).
**When to use:** Every lane (`ci`, `asan`, `hygiene`).
**Example:** [VERIFIED: ran on CMake 3.25.2 and 4.4.3; step-2 failure returned workflow rc=8]
```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    { "name": "base", "hidden": true, "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}" },
    { "name": "ci", "inherits": "base",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_COMPILE_WARNING_AS_ERROR": "ON", "GBL_LANE": "ci" } },
    { "name": "asan", "inherits": "base",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "CMAKE_COMPILE_WARNING_AS_ERROR": "ON", "GBL_LANE": "asan",
        "CMAKE_C_COMPILER": "clang-21",
        "CMAKE_C_FLAGS": "-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined" } }
  ],
  "buildPresets": [ { "name": "ci", "configurePreset": "ci" }, { "name": "asan", "configurePreset": "asan" } ],
  "testPresets": [
    { "name": "ci", "configurePreset": "ci",
      "output": { "outputOnFailure": true, "outputJUnitFile": "${sourceDir}/build/ci/junit.xml" },
      "filter": { "exclude": { "label": "^post$" } }, "execution": { "noTestsAction": "error" } },
    { "name": "ci-post", "configurePreset": "ci", "output": { "outputOnFailure": true },
      "filter": { "include": { "label": "^post$" } }, "execution": { "noTestsAction": "error" } }
  ],
  "workflowPresets": [
    { "name": "ci", "steps": [
      { "type": "configure", "name": "ci" }, { "type": "build", "name": "ci" },
      { "type": "test", "name": "ci" }, { "type": "test", "name": "ci-post" } ] }
  ]
}
```
Schema-6 limits (CMake docs table): `$comment` needs 10, `$schema` needs 8, `"jobs": ""` needs 11 [CITED: cmake-presets(7)]. Do not copy those from cogmaster's schema-11 file. Workflow rule: first step configure, later steps non-configure, same configure preset [CITED: cmake-presets(7)]; two `test` steps worked in the prototype [VERIFIED].

### Pattern 2: Registration helper + configure-time inventory compare
**What:** One `gbl_add_test(NAME ... LABELS ... COMMAND ... [SKIPPABLE])` helper; after all registration, `cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL ...)` compares the sorted `name<TAB>labels` set to `tests/inventory/${GBL_LANE}.txt` and fails configure with a diff. Also remove `${CMAKE_BINARY_DIR}/junit.xml` at configure so a stale file cannot satisfy the post step.
**Constraint:** The `TESTS` directory property lists only tests in that directory. Register every test from the top-level directory via `include()`, not `add_subdirectory()` [CITED: gabbaboy `ScaleSanitizerTimeouts.cmake` comment "Test properties are directory-scoped at the CMake 3.25 floor"; `set_tests_properties(... DIRECTORY)` is not available at the floor].
**Inventory line format (recommended):** `name<TAB>label,label[<TAB>skip-ok]`, sorted, `#` comments. The `post` test (`inventory.junit`) is listed but excluded from the junit comparison by label, as cogmaster does with `-DEXCLUDE=release` [OBS: cogmaster `check_junit`].
```cmake
# Source: sibling pattern ~/projects/cogmaster/cmake/CogmasterTests.cmake (adapted)
function(gbl__check_inventory)
  file(STRINGS "${CMAKE_SOURCE_DIR}/tests/inventory/${GBL_LANE}.txt" listed REGEX "^[^#]")
  get_property(names DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY TESTS)
  foreach(name IN LISTS names)
    get_test_property(${name} LABELS labels)
    list(SORT labels)
    list(JOIN labels "," labels)
    list(APPEND registered "${name}\t${labels}")
  endforeach()
  list(SORT registered)
  # skip-ok column: compare on the first two fields only
  ...
  if(NOT registered STREQUAL listed_two_cols)
    message(FATAL_ERROR "registered tests differ from sorted inventory\nregistered: ${registered}\nlisted: ${listed}")
  endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gbl__check_inventory)
```

### Pattern 3: junit inventory check (POSIX sh + awk)
**What:** `scripts/check-junit.sh JUNIT INVENTORY [EXCLUDE_LABEL_REGEX]`.
**Observed junit shapes** [VERIFIED: ran on CMake 4.4.3; skip/disabled shape re-run on 3.25.2]:

| Case | `<testcase ... status=` | Child element | Notes |
|------|------------------------|---------------|-------|
| pass | `run` | none | `WILL_FAIL` and `PASS_REGULAR_EXPRESSION` passes are also `run` |
| fail / timeout | `fail` | `<failure message="Failed"/>` / `message="Timeout"` | |
| `SKIP_RETURN_CODE` | `notrun` | `<skipped message="SKIP_RETURN_CODE=77"/>` | stdout lands in `<system-out>` |
| `SKIP_REGULAR_EXPRESSION` | `notrun` | `<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/>` | |
| missing executable | `notrun` | `<skipped message="Unable to find executable"/>` | counted in `skipped=` but ctest exit is non-zero |
| `DISABLED` | `disabled` | none | `ctest` exit 0 |

**Reason convention:** a skippable test prints `SKIPPED: <reason>` on stdout then exits with the skip code; the checker prints `SKIPPED <name>: <mechanism> | <reason>` for every skipped test [VERIFIED: extracted `GBL_OWNER_BIOS not set` from `<system-out>` with the awk in this session].
**Fail conditions:** expected test absent or present more than once; a testcase not in the inventory; `status` other than `run`; `<failure`/`<error`; skipped or disabled test not marked `skip-ok`; skipped test with no `SKIPPED:` reason; junit missing or zero testcases. Print a one-line summary (`expected N, ran M, skipped K (allowed J)`).
**Do not** use `status="run"` alone: `notrun` with `<skipped>` is how a skip looks. Cogmaster's awk treats anything but `status="run"` or any `<skipped` as a failure [OBS]; Gamebloke additionally needs the allowed-skip list and the reason report.

### Pattern 4: Negative controls via `ExpectFail.cmake`
**What:** A wrapper that runs a gate command and passes only when the exit code is non-zero **and** the combined output matches a stable reason code. Sibling form: `-DCMD -DCODE -DREGEX` with exact `CODE` [OBS: cogmaster `expect_fail.cmake`]; change to "non-zero" because sanitizer aborts exit 134 on macOS and 1 on Linux [VERIFIED].
```cmake
# cmake/ExpectFail.cmake  -DCMD="a;b;c" -DREGEX="hygiene: home-path"
foreach(var CMD REGEX)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "expect-fail: -D${var} is required")
  endif()
endforeach()
string(REGEX REPLACE "^:" "" opts "$ENV{ASAN_OPTIONS}:detect_leaks=0")
set(ENV{ASAN_OPTIONS} "${opts}")
execute_process(COMMAND ${CMD} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(rc STREQUAL "0")
  message(FATAL_ERROR "expect-fail: gate PASSED but must fail (negative control broken)\n${out}")
endif()
if(NOT out MATCHES "${REGEX}")
  message(FATAL_ERROR "expect-fail: failed for the wrong reason; wanted /${REGEX}/ got exit ${rc}:\n${out}")
endif()
```
**Three companions per control (all CTest tests):** (a) a **positive control**: the same gate on an otherwise identical clean sandbox must pass, so a gate that fails everything cannot satisfy its control (sibling: `hygiene.tree.binary_manifest` + `binary_unlisted`); (b) the **meta-control**: run `ExpectFail.cmake` against a command that succeeds and assert the wrapper itself fails with "gate PASSED but must fail"; (c) sandboxes live under `${CMAKE_BINARY_DIR}/nc/<name>` (git-ignored, outside the tree scan).
**Stable reason codes:** gates print `gate: reason-code: path[:line]`, for example `hygiene: home-path: x.md:3`. Controls match on `hygiene: home-path`, not free text.
**Verify fails if a control passes:** the control is a CTest test in the main step, so a passing gate makes the test fail, the `ctest` step non-zero, and the workflow non-zero [VERIFIED mechanism: wrapper exits via `message(FATAL_ERROR)`; workflow step-2 failure returns rc 8 in prototype].

### Pattern 5: Gates
**Hygiene (`scripts/hygiene.sh`)**
- Modes: `--tree` (tracked plus untracked-not-ignored), `--staged`, `--dir <path>` (no git; used by controls), `--history` (optional, not required).
- Iterate files NUL-safely: `git ls-files -z | xargs -0 sh -c 'for f; do ...; done' sh`. The sibling's newline-split loop breaks on newline-containing names [OBS: cogmaster `check_files`].
- Patterns (POSIX ERE, `LC_ALL=C`). Home path and email sets are the sibling's [OBS: cogmaster hygiene.sh lines 12-15]; the credential set is new. All three were run against the current tracked tree this session: **zero hits** [VERIFIED: ran].
```sh
HOME_PATH='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:\\Users\\[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'
HOME_OK='(/Users/|/home/)runner'
EMAIL='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z][A-Za-z]+'
EMAIL_OK='@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|@example\.(com|org|net)|git@github\.com'
CRED='AKIA[0-9A-Z]{16}|ASIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{22,}|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}|sk-ant-[A-Za-z0-9_-]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----'
CRED_ASSIGN='(api[_-]?key|secret|token|passw(or)?d)[[:space:]]*[:=][[:space:]]*["'"'"']?[A-Za-z0-9/+_.-]{16,}'
```
- `~/projects/...` is not matched by any pattern, so `.planning/` docs keep using `~/`. The tilde form appears 15 times in the tree today [VERIFIED: grep count].
- Do **not** exempt the scanner from itself (sibling skips `SELF`). The patterns above do not self-match when written as regexes (escaped dots, bracket classes); add a CTest that runs the scanner over `scripts/hygiene.sh` to keep that true [INF].
- Do **not** port cogmaster's `check_stop_policy` (requires GSD `mode: interactive`, forbids `auto_advance`) or its "no commit on main" rule: this repo's `.planning/config.json` has `"mode": "yolo"` and `"auto_advance": true`, so the port would fail the tree on day one [OBS: config.json read]. Keep identity check (author/committer must be a noreply address) for `--staged`; history idents are all `szTheory@users.noreply.github.com` today [VERIFIED: `git log --all`].

**Signature scan (`scripts/signature-scan.sh`)**
- Inputs: `tests/signatures/logo.sha256`, `bios.sha256` (`<64-hex> <label>` per line), `allowlist.txt` (one path per line, empty at first), optional `--extra-digests FILE` used only by controls.
- Rules and reason codes: `logo-window` (sha256 of bytes 4..159 of any binary file of at least 160 bytes), `bios-digest` (sha256 of any file of exactly 16384 bytes), `rom-extension` (names ending `gba agb mb bin rom bios sav srm sa1 eep eeprom fla flash ss[0-9]`, discretionary list) unless allowlisted, `binary-not-allowlisted` (any binary not on allowlist).
- Window hashing, verified equal to `shasum -a 256` on macOS (156-byte window, `bs=4 skip=1 count=39`) and portable to dash:
```sh
dd if="$f" bs=4 skip=1 count=39 of="$tmp" 2>/dev/null          # bytes 4..159
set -- $("$CMAKE" -E sha256sum "$tmp"); digest=$1
```
  `cmake -E sha256sum` exists at 3.25.2 and 4.4.3 [VERIFIED: ran]. CMake-native `file(READ ... HEX OFFSET 4 LIMIT 156)` gives hex text, and `string(SHA256)` of that text is **not** the SHA-256 of the bytes, so it cannot be compared with published digests [VERIFIED: hex length 312 observed; semantics by definition]. Use `dd` plus `cmake -E sha256sum`.
- Skip non-binary files for the window hash (spawn cost); binary detection: `grep -Iq ''` (sibling).

**Change classifier (`scripts/classify-changes.sh`)** reuse the sibling [OBS: read in full]: reads changed paths on stdin (both old and new name of a rename), takes the PR's `changed_files` count as `$1`; prints `heavy=false` only when there is at least one path, every path is under `.planning/`, and the count is at most 3000; any API failure line (`(listing failed)`) counts as a non-`.planning/` path. Drop the sibling's `release_inputs` output; it has no counterpart here. The sibling's gh call: `gh api --paginate "repos/$GITHUB_REPOSITORY/pulls/$NUMBER/files" --jq '.[] | .filename, (.previous_filename // empty)'`. For `push` events (no PR number) print `heavy=true`.

### Pattern 6: Aggregator and workflow shape
- Jobs: `changes` (always), `hygiene` (always: `.planning/` is public prose where a home path can leak, so it must run on `.planning/`-only PRs), `title` (PR only), `build` matrix (heavy), `asan` (heavy), `ci-required` (`name: CI required`, `if: always()`, `needs:` every other job).
- Triggers: `pull_request: types: [opened, synchronize, reopened, edited]`, `push: branches: [main]`; **no** `paths:` filter. A workflow skipped by a path filter leaves the required check Pending, while a job skipped by `if:` reports Success [CITED: docs.github.com troubleshooting-required-status-checks]. `edited` is needed so a title edit reruns the title check; the title is read live with `gh pr view` because a rerun reuses the original event payload [OBS: cogmaster ci.yml comment].
- Defaults: `permissions: contents: read` (add `pull-requests: read` only to `changes` and `title`), `persist-credentials: false`, concurrency `group: ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.sha }}`, `cancel-in-progress: ${{ github.event_name == 'pull_request' }}` [OBS: cogmaster ci.yml lines 23-40].
- Matrix: `{ runner: ubuntu-26.04, os: linux, cc: gcc, id: GNU }`, `{ runner: macos-26, os: macos, id: AppleClang }`; each leg runs `cmake --workflow --preset ci`. Add a configure-time guard: if env `GBL_EXPECT_C_ID` is set, fail configure when `CMAKE_C_COMPILER_ID` differs, so "Linux (GCC)" is proven rather than assumed [INF].
- Aggregator logic in `cmake/CiRequired.cmake` reading `$ENV{NEEDS}` (= `${{ toJSON(needs) }}`), `HEAVY`, `EVENT`; success unless: result is `success`; or `skipped` and heavy is `false` and job is not in `changes|hygiene|title`; or `title` skipped and event is not `pull_request`. A heavy job skipped when `heavy=true` (because a dependency failed) must **fail**. Probed on 3.25.2: planning-only case passes, failure case reports `build=failure` [VERIFIED: ran]. Unit-test it with a case table (including the heavy-skipped-when-heavy-true failure) in CTest, and add a test asserting every job id in `ci.yml` appears in the aggregator's `needs`.
- Timeouts: every job gets `timeout-minutes` with a trailing `# provisional` comment until measured runs exist; then `2 x slowest of 2 runs, run <id>` [OBS: cogmaster convention; owner direction in CONTEXT].

### Pattern 7: Rollout order for GitHub actions (owner-authorized)
1. Push phase branch; open **PR B (source change, includes `ci.yml`)**. A `pull_request` run uses the workflow in the PR merge commit, so the workflow must exist for any PR to report `CI required`; PR B carries it and runs every lane.
2. Squash-merge PR B once green. The `push` run on `main` is the first `main` evidence.
3. Open **PR A (`.planning/`-only; records PR B and main run IDs as evidence)**. Its run proves heavy lanes skip and `CI required` still passes. Record run IDs.
4. Apply the ruleset **after** a run has reported `CI required` (sibling reason: before that it blocks the PR that adds CI [OBS: apply-repo-settings.sh comment]). Then merge PR A through the ruleset.
5. Switch GSD's `git.branching_strategy` away from `none` (config currently `none`), or all later direct-to-main commits are rejected (ruleset: PR required, `bypass_actors: []`).
Ruleset JSON shape (committed at `.github/rulesets/main.json`), sibling-proven [OBS: cogmaster `.github/rulesets/main.json`]; GitHub Actions' integration id is **15368**, confirmed live: `gh api /apps/github-actions --jq .id` returned `15368` [VERIFIED]:
```json
{ "name": "main", "target": "branch", "enforcement": "active",
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "bypass_actors": [],
  "rules": [
    { "type": "deletion" }, { "type": "non_fast_forward" }, { "type": "required_linear_history" },
    { "type": "pull_request", "parameters": { "required_approving_review_count": 0, "dismiss_stale_reviews_on_push": false,
        "require_code_owner_review": false, "require_last_push_approval": false,
        "required_review_thread_resolution": false, "allowed_merge_methods": ["squash"] } },
    { "type": "required_status_checks", "parameters": { "strict_required_status_checks_policy": false,
        "required_status_checks": [ { "context": "CI required", "integration_id": 15368 } ] } } ] }
```
Parameter names match the REST docs: `required_status_checks[].context`, `integration_id` (optional), `strict_required_status_checks_policy` (required), `pull_request.allowed_merge_methods` [CITED: docs.github.com/en/rest/repos/rules]. Apply with `POST /repos/{owner}/{repo}/rulesets` (create) or `PUT .../rulesets/{id}` (update by name for idempotence). Current repo state: public, admin permission, zero rulesets, `squash_merge_commit_title: COMMIT_OR_PR_TITLE`, Actions `sha_pinning_required: false` [VERIFIED: `gh api`].

### Anti-Patterns to Avoid
- **Trigger-level `paths:` filters** on the workflow (leaves the required check Pending).
- **`*-latest` runner labels** (`ubuntu-latest` is currently 24.04, `macos-latest` is macOS 26 arm64, and both move) [CITED: docs.github.com github-hosted-runners].
- **`pull_request_target`**, secrets, or `${{ github.event.pull_request.title }}` interpolated into `run:` (script injection). Pass values through `env:`.
- **`continue-on-error`** or `if: always()` on a gate job.
- **Recoverable sanitizer builds** (UBSan default exits 0).
- **Constant-expression UB controls** (compiler may diagnose/fold); use an opaque value.
- **Real logo/BIOS/ROM bytes anywhere in the tree or CI logs**; controls use synthesized stand-ins whose digests are registered in a test-only list.
- **Exact exit-code assertions** for sanitizer controls.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SHA-256 | In-script hashing code | `cmake -E sha256sum <file>` | Same tool on macOS and Linux; equals `shasum -a 256` [VERIFIED]. |
| junit generation | Custom report format | `ctest --output-junit` via preset `outputJUnitFile` | Format identical on 3.25.2 and 4.4.3 for skip/disabled [VERIFIED]. |
| JSON parsing in the aggregator | `sed` over `toJSON(needs)` | CMake `string(JSON ... MEMBER/GET)` | Available since 3.19; probed. |
| Skip/inventory bookkeeping | Per-test ad-hoc greps | One `gbl_add_test` helper + sorted inventory | Prevents drift between registration and expectation. |
| Test framework | Any dependency | CTest + ~40-line `check.h` | Locked decision. |
| Secret detection beyond the pattern list | Entropy scanners | Pattern list + GitHub secret scanning push protection (free on public repos; enabling it is a repo setting, see Open Question 2) | Keeps zero dependencies. |
| Workflow linting | New tool | `check-action-pins.sh` (grep for `@<40 hex> # v`) + the `ci.yml` job-vs-needs test | Covers the two stated criteria without `actionlint`. |

**Key insight:** every gate here is a few lines of shell; the value is in the negative controls and in making skip/missing states loud. Resist adding a framework, a Python runtime, or a lint tool.

## Runtime State Inventory

Not applicable: greenfield phase (no rename, refactor, or migration).

## Common Pitfalls

### Pitfall 1: `ctest` passes while tests silently did not run
**What goes wrong:** `SKIP_RETURN_CODE`, `SKIP_REGULAR_EXPRESSION`, and `DISABLED` tests do not fail `ctest`; it exits 0 [VERIFIED: ran, rc=0 with one skipped and one disabled test].
**How to avoid:** The post-step junit check plus the configure-time inventory compare. Add controls: removed test, skipped-not-allowed, skipped-without-reason, extra test, duplicate.
**Warning signs:** `noTestsAction` not set to `error`; inventory files edited in the same commit that removes a test without a reviewer noticing (the diff is the visible record).

### Pitfall 2: Sanitizer control "passes" because UBSan recovered
**What goes wrong:** Without `-fno-sanitize-recover=all`, UBSan prints `runtime error: ...` and exits 0 on Apple clang 21, Clang 21.1.8 and GCC 15.2 [VERIFIED].
**How to avoid:** `-fno-sanitize-recover=all` in the `asan` preset; control asserts non-zero exit plus reason text.
**Observed texts** [VERIFIED]: Clang: `runtime error: shift exponent 32 is too large for 32-bit type 'uint32_t' (aka 'unsigned int')` and `SUMMARY: UndefinedBehaviorSanitizer`; GCC: `... for 32-bit type 'unsigned int'`. Common prefix to match: `runtime error: shift exponent 32 is too large for 32-bit type`. ASan (Clang): `AddressSanitizer: heap-buffer-overflow`. Exit codes: macOS 134, Linux 1. GCC reports the heap overflow through UBSan (`store to address ... with insufficient space`) before ASan, so match the ASan text only for Clang.

### Pitfall 3: Preset features copied from a schema-11 sibling
**What goes wrong:** `$comment`, `$schema`, `"jobs": ""` fail or are rejected at schema 6 / CMake 3.25 [CITED: cmake-presets(7) version table].
**How to avoid:** Put commentary in `CMakeLists.txt` or docs. Verify the preset file on the 3.25 floor locally (a throwaway venv with `pip install cmake==3.25.2` worked in this session; see the Package Legitimacy Audit for the flag).

### Pitfall 4: Directory-scoped test lists
**What goes wrong:** `get_property(DIRECTORY ... TESTS)` and `set_tests_properties` are directory-scoped at the floor; tests added in subdirectories vanish from the configure-time compare.
**How to avoid:** Register all tests at top level via `include()`.

### Pitfall 5: Stale junit satisfies the post step
**What goes wrong:** A leftover `build/<lane>/junit.xml` passes the inventory even if the test step produced nothing.
**How to avoid:** Delete it at configure time (every workflow run reconfigures). Document that running `ctest` alone after edits is not the verify command.

### Pitfall 6: Required check stuck Pending, or "passes" while a lane failed
**What goes wrong:** Path-filtered triggers leave the check Pending; skipped dependents of a failed job report Success.
**How to avoid:** No trigger path filters; `CI required` has `if: always()` and requires `success` for every heavy job when `heavy=true`. Unit-test the predicate.

### Pitfall 7: Bootstrapping the check
**What goes wrong:** A `.planning/`-only PR opened before `ci.yml` is on `main` never gets a `CI required` run; the ruleset applied before any run blocks every PR.
**How to avoid:** Rollout order in Pattern 7.

### Pitfall 8: Ruleset vs GSD direct-to-main commits
**What goes wrong:** After activation, GSD (`git.branching_strategy: none`) cannot push to `main`; `bypass_actors: []` means even the owner is blocked.
**How to avoid:** Apply last; change branching strategy to per-phase branches; push the one local commit ahead of `origin/main` first (local `main` is 1 ahead [OBS: `git status -sb`]).

### Pitfall 9: Hygiene trips on GSD's own documents
**What goes wrong:** Executors or tools record absolute paths (a `/Users/<name>/...` form or the encoded `-Users-<name>-...` form) in `.planning/` files. The tree passes today [VERIFIED], so any new hit is a regression introduced by this phase's own artifacts.
**How to avoid:** Use `~/` in all planning text; install the opt-in pre-commit hook; keep the `hygiene` job outside the heavy gate so `.planning/`-only PRs are scanned. This RESEARCH.md was written to pass the scan.

### Pitfall 10: Negative-control fixtures tripping their own gates or push protection
**What goes wrong:** A literal token-shaped string or `/Users/<name>` path in a test file fails the tree scan or triggers GitHub push protection.
**How to avoid:** Build planted strings from fragments at test time (sibling: `"/Use" + "rs/someone"` [OBS: cogmaster `tests/hygiene/tests.cmake`]); write sandboxes under `build/`; never commit a realistic literal.

### Pitfall 11: GCC-only diagnostics invisible on the owner's Mac
**What goes wrong:** The macOS host has no GCC; the first Linux GCC build can fail on a warning with `-Werror`.
**How to avoid:** The proposed flag set `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-qual` compiled the skeleton clean with `-Werror` on Apple clang 21.0.0, Ubuntu GCC 15.2/Clang 21.1.8, and GCC 15.3 (docker) [VERIFIED: ran]. Re-check with `docker run gcc:15` before the first push; accept that the first CI run is the real proof.

### Pitfall 12: Logo/BIOS digests: one verified, one unknown
See Open Question 1 and Assumptions A1. A scan that ships with an empty logo digest list protects nothing; the well-formedness test must **fail** (not skip) if `logo.sha256` has no valid 64-hex entry.

### Pitfall 13: Debatable squash title
**What goes wrong:** Repo default `squash_merge_commit_title: COMMIT_OR_PR_TITLE` uses the commit title for single-commit PRs, so the checked PR title may not be the commit that lands [VERIFIED: current setting]. Sibling sets `PR_TITLE` and `PR_BODY` [OBS].
**How to avoid:** `--check` reports this drift; applying it is a repo-setting change beyond the stated authorization, see Open Question 2.

## Code Examples

### UBSan and ASan negative controls (compile only in the `asan` lane)
```c
/* tests/ub_control.c  - verified to compile clean with the warning set above on
   Apple clang 21, Clang 21.1.8, GCC 15.2, and to abort with the observed text */
#include <stdint.h>
#include <stdio.h>
int main(int argc, char **argv) {
  (void)argv;
  volatile unsigned n = (unsigned)(argc + 31);   /* opaque: 32 */
  uint32_t y = 1u << n;                           /* UB: shift exponent 32 */
  printf("%u\n", (unsigned)y);
  return 0;
}
/* tests/asan_control.c */
#include <stdlib.h>
int main(int argc, char **argv) {
  (void)argv;
  char *p = malloc(8);
  p[8 + argc - 1] = 1;                            /* heap-buffer-overflow */
  free(p);
  return 0;
}
```
```cmake
if(GBL_LANE STREQUAL "asan")
  add_executable(ub_control tests/ub_control.c)
  add_executable(asan_control tests/asan_control.c)
  gbl_add_test(NAME sanitizer.ubsan.shift32 LABELS sanitizer,control
    COMMAND ${CMAKE_COMMAND} "-DCMD=$<TARGET_FILE:ub_control>"
      "-DREGEX=runtime error: shift exponent 32 is too large for 32-bit type" -P ${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake)
  gbl_add_test(NAME sanitizer.asan.heap_overflow LABELS sanitizer,control
    COMMAND ${CMAKE_COMMAND} "-DCMD=$<TARGET_FILE:asan_control>"
      "-DREGEX=AddressSanitizer: heap-buffer-overflow" -P ${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake)
endif()
```
`ASAN_OPTIONS=detect_leaks=0` is set inside the wrapper so leak noise cannot mask or replace the reason [OBS: sibling `expect_fail.cmake`]. The asan lane also runs the core and runner tests, which proves "ASan + UBSan builds of the core and runner" (VERIF-06).

### Aggregator script (verified on CMake 3.25.2)
```cmake
# cmake/CiRequired.cmake  env: NEEDS (toJSON(needs)), HEAVY, EVENT
cmake_minimum_required(VERSION 3.25)
string(JSON count LENGTH "$ENV{NEEDS}")
if(count EQUAL 0)
  message(FATAL_ERROR "ci-required: needs is empty")
endif()
math(EXPR last "${count} - 1")
foreach(i RANGE ${last})
  string(JSON name MEMBER "$ENV{NEEDS}" ${i})
  string(JSON result GET "$ENV{NEEDS}" "${name}" result)
  if(result STREQUAL "success")
    continue()
  endif()
  if(result STREQUAL "skipped" AND "$ENV{HEAVY}" STREQUAL "false" AND NOT name MATCHES "^(changes|hygiene|title)$")
    continue()
  endif()
  if(result STREQUAL "skipped" AND name STREQUAL "title" AND NOT "$ENV{EVENT}" STREQUAL "pull_request")
    continue()
  endif()
  list(APPEND bad "${name}=${result}")
endforeach()
if(bad)
  message(FATAL_ERROR "ci-required: not allowed: ${bad}")
endif()
```
Workflow side: `env: { NEEDS: ${{ toJSON(needs) }}, HEAVY: ${{ needs.changes.outputs.heavy }}, EVENT: ${{ github.event_name }} }` then `run: cmake -P cmake/CiRequired.cmake`. Pass JSON through the environment, not `-D`, to avoid quoting problems.

### Action pin line shape (the only accepted form)
```yaml
- uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
  with:
    persist-credentials: false
```
`check-action-pins.sh` fails any `uses:` not matching `@[0-9a-f]{40} +# v[0-9]` (local `./` actions excepted) and has a negative control with an unpinned `@v4`.

### Hook (opt-in)
```sh
#!/bin/sh
# .githooks/pre-commit  -- enable with: git config core.hooksPath .githooks
exec "$(git rev-parse --show-toplevel)/scripts/hygiene.sh" --staged
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `ubuntu-latest` assumed newest | `ubuntu-latest` = 24.04; `ubuntu-26.04` available by explicit label | listed in runner-images README this session | Pin `ubuntu-26.04` explicitly (GCC 15.2, CMake 4.4.5). |
| `macos-14` | retired 2 Nov 2026; `macos-15`, `macos-26` (`macos-latest` = 26 arm64) | per runner-images README | Do not use `macos-14`. `xcode-27` is Preview: avoid. |
| Separate CI "every test ran" step | Post-test step inside the workflow preset | schema 6 (CMake 3.25) | Local and CI verdicts match. |
| Recoverable UBSan | `-fno-sanitize-recover=all` | long-standing | Required for a gate. |

**Deprecated/outdated:** `macos-14` images; any research note that listed `ubuntu-*`/`macos-*` labels as unverified [UNK] (now verified).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The SHA-256 of the 156-byte logo window cannot be obtained from a primary source; the recommended way is an owner-run digest helper or an executor-side transient derivation. Rights acceptability of transient derivation is not confirmed. | Pattern 5, Open Q1 | Without it EVID-04's logo path is unprovable on the real logo; with a wrong digest the scan gives false assurance. |
| A2 | The DS-family GBA-mode BIOS is also exactly 16384 bytes (one secondary web result said so); its SHA-256 from the GBARunner2 wiki is `782eb3894237ec6aa411b78ffee19078bacf10413856d33cda10b44fd9c2856b`. | Pattern 5 | A missed size would let that BIOS slip past the size gate; the extension/binary rules still catch it. |
| A3 | `clang-21` binary name and need for `apt install libclang-rt-21-dev` on `ubuntu-26.04` come from the sibling workflow, not from the image README. | Standard Stack | The asan job's setup step may need adjustment on first run. |
| A4 | Squash-title and Actions `sha_pinning_required` settings are outside the owner's stated authorization (ruleset only). | Open Q2 | Applying them without confirmation exceeds authority. |
| A5 | GitHub push protection could block token-shaped literals; mitigation is building planted strings from fragments. | Pitfall 10 | If wrong, merely extra caution. |
| A6 | CMake 4.4.5 (runners) behaves like 4.4.3 (local) for junit and workflow presets. | Pattern 1, 3 | First CI run would show it. |
| A7 | Job timeouts: initial values (suggest 5 min light jobs, 10 min build/asan) are provisional; measured values replace them. | Pattern 6 | Flaky timeouts or slow failure detection. |
| A8 | Extension list for ROM/save names is a reasonable GBA set; owner may extend. | Pattern 5 | Missed extension leaves a hole until the binary-allowlist rule catches it. |
| A9 | Local sanitizer builds on macOS work (the prototype ran) but are not relied on per the locked decision. | Summary | None; informational. |

## Open Questions (RESOLVED)

> All six questions are resolved for planning. Resolutions are recorded inline as `RESOLVED:` lines (decided by the orchestrator under the owner's CONTEXT.md authorizations on 2026-11-09).

1. **Nintendo-logo digest provenance.**
   - Known: GBATEK places the logo at header offset 004h for 156 bytes and does not publish a hash; searches found no published SHA-256 of the logo window. GBA BIOS SHA-256 is published (see below). PITFALLS.md records that jsmolka's `lib/header.asm` embeds the logo [CITED: `.planning/research/PITFALLS.md` line 93].
   - Unclear: how the real digest enters the repo without anyone handling logo bytes in the tree.
   - Recommendation (default): a `checkpoint:human-action` early in the phase where the owner runs a provided `scripts/signature-digest.sh logo <file>` against any GBA-header-bearing file they already hold, or confirms an executor-side transient derivation (bytes read in a temp directory outside the repo, only the digest and method recorded). `signature.digests.wellformed` must fail until a valid logo digest is present. Everything else in the phase can proceed with the synthesized controls.
   - RESOLVED: the committed digest list carries a `logo` row marked `owner-pending`; an owner `checkpoint:human-action` (last in-phase gate) supplies the real digest via `scripts/signature-digest.sh`. Pending state reports SKIPPED with reason, never PASS; unresolved at phase close it becomes a visible human-needed gap.
2. **Scope of `apply-repo-settings.sh`.**
   - Authorized: the `main` ruleset requiring `CI required`.
   - Not explicitly authorized: `squash_merge_commit_title=PR_TITLE` and `squash_merge_commit_message=PR_BODY`, Actions `sha_pinning_required=true`, secret scanning and push protection (all reversible, free on public repos, sibling applies them).
   - Recommendation: script defaults to ruleset only; `--check` reports drift on the extra settings; applying them needs an owner `checkpoint:human-verify`.
   - RESOLVED: ruleset only (PR required + required check `CI required` + no bypass). Extra settings are reported by `--check` as advisory drift and never applied without owner approval.
3. **macOS image:** `macos-26` (parity with local, Apple clang 21.0.0) vs `macos-15` (sibling). Recommend `macos-26`.
   - RESOLVED: `macos-26` (arm64) and `ubuntu-26.04`; never `-latest`.
4. **CMake 3.25 floor in CI:** runners ship 4.4.5, so the floor is never exercised by CI. This session proved the prototype (schema 6, workflow, junit, `string(JSON)`, `cmake -E sha256sum`) on 3.25.2. Recommend a one-time local floor check recorded as evidence, and defer a CI floor lane (it needs the flagged PyPI wheel or a pinned Kitware tarball).
   - RESOLVED: one-time local floor check with a SHA-256-verified Kitware tarball, recorded as evidence; no CI floor lane and no PyPI wheel in CI.
5. **Text-form logo bytes.** The window-hash catches binary ROMs (including built Phase 5 diagnostics if the scan is pointed at the build output). It does not catch logo bytes pasted as assembler text (jsmolka-style). Leave for Phase 5, which already owns "no logo bytes in diagnostics" (DIAG-01/02).
   - RESOLVED: deferred to Phase 5 (DIAG-01/02).
6. **`--history` hygiene in CI:** optional; deferred by CONTEXT.
   - RESOLVED: deferred per CONTEXT.md.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | everything | yes | 4.4.3 local; 3.25.2 probe venv; runners 4.4.5 | none needed |
| Ninja | generator | yes | 1.13.2 | none |
| Apple clang | local build, macOS leg | yes | 21.0.0 (arm64) | none |
| GCC (local) | Linux GCC proof | no (local `gcc` is Apple clang) | docker `gcc:15` = 15.3.0; `ubuntu:26.04` container GCC 15.2.0 | docker; or first CI run |
| Clang 21 on Linux | asan lane | verified in an `ubuntu:26.04` container (Ubuntu clang 21.1.8 after `apt install clang-21 libclang-rt-21-dev`) | 21.1.8 | none |
| gh CLI | classifier, title, ruleset | yes, authed as the repo owner with admin | 2.101.0 | none |
| jq | optional (ruleset drift) | yes | system | CMake `string(JSON)` |
| docker | optional Linux pre-check | yes (aarch64 daemon) | 29.5.2 | skip |
| git identity | hygiene identity check | yes, noreply | n/a | n/a |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** local GCC (use docker or CI).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest (CMake 3.25 floor) + in-tree `tests/check.h`; junit via `ctest --output-junit` |
| Config file | `CMakePresets.json` (schema 6) + `tests/inventory/<lane>.txt` (none exist; Wave 0) |
| Quick run command | `ctest --test-dir build/ci -R '^(hygiene|signature)\.' --output-on-failure` (single area) |
| Full suite command | `cmake --workflow --preset ci` (and `cmake --workflow --preset asan` on Linux) |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| VERIF-01 | One workflow preset builds, tests, scans, inventories; CI calls exactly it | workflow + static | `cmake --workflow --preset ci`; test `ci.yml.preset` greps that every job runs `cmake --workflow --preset <lane>` | Wave 0 |
| VERIF-01 | Floor works on 3.25 | manual-once (recorded) | run the same preset with a 3.25.x CMake | Wave 0 (evidence note) |
| VERIF-02 | Each gate has a failing control; wrapper fails when a gate passes | meta | tests `nc.meta.passing_gate_fails_wrapper`, `nc.<gate>.*` | Wave 0 |
| VERIF-03 | Removed/skipped/extra/duplicate/no-reason tests fail the inventory; skips listed | unit | `inventory.junit` (real run); `inventory.nc.missing`, `.skipped_not_allowed`, `.skipped_no_reason`, `.extra`, `.duplicate`, `.configure_mismatch` on synthetic junit | Wave 0 |
| VERIF-05 | `CI required` aggregator predicate; classifier; title; pins | unit | `ci.required.cases`, `ci.classify.cases`, `ci.title.cases`, `ci.actions.pinned` (+ negative `@v4` control) | Wave 0 |
| VERIF-05 | Live routing on real PRs | manual evidence | `gh run view <id>` for PR B (all lanes) and PR A (heavy skipped, `CI required` success); ids recorded in `.planning/` | Wave 0 (human-evidence) |
| VERIF-06 | ASan/UBSan lane builds core+runner; shift-by-32 and heap overflow controls caught | integration | `cmake --workflow --preset asan` (Linux) | Wave 0 |
| VERIF-07 | Credentials, non-noreply email, home path fail; clean tree passes | unit+tree | `hygiene.tree`, `hygiene.nc.credential`, `.email`, `.home_path`, positive controls | Wave 0 |
| EVID-04 | Logo window, BIOS size+digest, ROM extension, unlisted binary fail; allowlisted binary passes | unit+tree | `signature.tree`, `signature.nc.logo`, `.bios`, `.rom_ext`, `.binary_unlisted`, `.binary_allowlisted` (positive), `signature.digests.wellformed` | Wave 0 |
| EVID-04 | Owner-local check | optional | `signature.owner_local` exits 77 with `SKIPPED: <reason>` when unset; inventory marks `skip-ok` | Wave 0 |

### Sampling Rate
- **Per task commit:** the quick command for the touched area plus `sh scripts/hygiene.sh --tree` (seconds).
- **Per wave merge:** `cmake --workflow --preset ci`.
- **Phase gate:** `ci` and `asan` workflows green locally/Linux, both real PRs green, `CI required` live evidence recorded, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `CMakeLists.txt`, `CMakePresets.json`, `cmake/{GblTests,InventoryCompare,ExpectFail,CiRequired}.cmake`
- [ ] `scripts/{hygiene,signature-scan,check-junit,classify-changes,check-title,check-action-pins,apply-repo-settings}.sh`
- [ ] `tests/{check.h,version_test.c,ub_control.c,asan_control.c}`, `tests/inventory/*.txt`, `tests/signatures/*`, `tests/fixtures/*`
- [ ] `.github/workflows/ci.yml`, `.github/rulesets/main.json`, `.githooks/pre-commit`
- [ ] Framework install: none (CMake, Ninja, clang already present)

## Security Domain

`security_enforcement` is enabled (ASVS level 1, block on high per config).

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth surface. |
| V3 Session Management | no | None. |
| V4 Access Control | yes (repository) | GitHub ruleset: PR required, required check, linear history, squash only, no bypass; workflows `permissions: contents: read`. |
| V5 Input Validation | yes | Scanner and classifier handle untrusted file names and PR metadata: NUL-safe iteration, values passed via `env:`, regex anchors. Refuse unknown flags. |
| V6 Cryptography | yes (integrity only) | SHA-256 via `cmake -E sha256sum`; no hand-rolled hashing. |
| V14 Configuration / supply chain | yes | SHA-pinned actions with version comments, `persist-credentials: false`, no secrets, no `pull_request_target`, pinned runner labels. |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Script injection via PR title/branch into `run:` | Tampering/Elevation | Read titles with `gh pr view` into env; never interpolate `${{ }}` of user text into shell. |
| Compromised or retagged action | Tampering | Full-SHA pins, version comment, `check-action-pins.sh` gate. |
| PR edits the workflow or gate scripts to weaken a gate | Tampering | `pull_request` runs with a read-only token and no secrets; negative controls fail loudly; review is the control. |
| Secrets or home paths committed to a public repo | Information disclosure | Hygiene gate, pre-commit hook, history idents verified noreply. |
| Nintendo logo/BIOS/ROM/save committed | Rights exposure (High) | Signature gate with synthesized controls; `.gitignore` already lists `*.gba *.agb *.bin *.sav *.srm` [OBS: cat .gitignore]. |
| Filename with newline or leading dash defeats a shell loop | Tampering | `-z`/`xargs -0`, quote everything, `--` before paths. |
| Planted-token fixture blocked/flagged by secret scanning | DoS (CI) | Build planted strings from fragments at test time. |

## Sources

### Primary (HIGH confidence)
- actions/runner-images README (image labels) and `Ubuntu2604-Readme.md`, `macos-15-arm64-Readme.md`, `macos-26-arm64-Readme.md` (tool versions) via raw.githubusercontent.com
- docs.github.com/en/actions/reference/runners/github-hosted-runners (label table, `-latest` mapping)
- docs.github.com/en/rest/repos/rules (ruleset body, `required_status_checks`, `pull_request` parameters)
- docs.github.com/en/pull-requests/.../troubleshooting-required-status-checks (path-filtered Pending vs skipped job Success)
- docs.github.com/en/actions/.../events-that-trigger-workflows (`pull_request` default types, merge-commit SHA)
- cmake.org/cmake/help/latest/manual/cmake-presets.7.html (schema version table, workflow rules)
- `gh api` live: `/apps/github-actions` id 15368; `repos/actions/checkout` tag v7.0.1 -> commit `3d3c42e5aac5ba805825da76410c181273ba90b1`; `repos/actions/upload-artifact` v7.0.2 -> `cf430e030ddbb5b0abf93d22962f4752f3646cd9`; repo settings of this project
- Probes run this session: CTest junit shapes (3.25.2, 4.4.3), schema-6 workflow with post step, sanitizer behavior on Apple clang 21, Ubuntu 26.04 GCC 15.2 and Clang 21.1.8 (docker), warning-flag cleanliness on GCC 15.3, `dd`+`cmake -E sha256sum` vs `shasum`

### Secondary (MEDIUM confidence)
- higan docs (install/general) and GBARunner2 wiki (wiki.ds-homebrew.com/gbarunner2/bios): GBA BIOS 16384 bytes, SHA-256 `fd2547724b505f487e6dcb29ec2ecff3af35a841a77ab2e85fd87350abd36570`; two independent documents agree [VERIFIED across two sources; emulator documentation, not a vendor].
- Sibling repos read in full (engineering patterns only; hardware, gates and stops do not transfer): `~/projects/cogmaster` (`.github/workflows/ci.yml`, `scripts/{hygiene,classify-changes,check-title,apply-repo-settings}.sh`, `cmake/CogmasterTests.cmake`, `tests/cmake/{repository.cmake,repository.sh,expect_fail.cmake,ci_required.cmake}`, `tests/hygiene/tests.cmake`, `CMakePresets.json`, `.github/rulesets/main.json`, `.githooks/`), `~/projects/gabbaboy/cmake/{ExpectedTests,ScanForbiddenFixtures,ScaleSanitizerTimeouts}.cmake`

### Tertiary (LOW confidence)
- Single search result: DS-family BIOS size and MD5/SHA-1 (A2).
- Logo digest: no source found (A1).

## Project Constraints (from CLAUDE.md / AGENTS.md)

`.claude/CLAUDE.md` defers to `AGENTS.md`; actionable directives that bind this phase:
- No commercial ROMs, Nintendo BIOS bytes, private saves, credentials or unlicensed test content in source control or public CI; a matching hash proves identity, not redistribution rights.
- Keep a small, flat dependency tree; add nothing without concrete value (this phase adds none).
- One canonical local command aligned with the fast required CI path; every important gate has a meaningful negative control; missing and skipped checks must be visible.
- After each CI run, inspect critical-path time for duplicate setup (note: the `ci` preset repeats the hygiene tests that the always-on `hygiene` job also runs; measure before removing).
- Keep evidence labels distinct (source facts, observations, inferences, unknowns); research claims must be verified against current sources.
- Owner gates stay visible: G1 (license) is open, do not add a LICENSE file; no release artifacts are published (G11 untouched). Do not reopen the project name.
- External publication is a human gate except where the owner already authorized it in CONTEXT.md (phase branch push, two PRs, ruleset).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, tool and image versions read from the runner-images READMEs and probed.
- Architecture: HIGH for workflow/inventory/control mechanics (prototyped); MEDIUM for rollout order (reasoned from GitHub semantics, not rehearsed).
- Pitfalls: HIGH for items marked VERIFIED; LOW for the logo digest.

**Research date:** 2026-11-09
**Valid until:** 2026-12-09 for tool and image versions (runner images update frequently); mechanics are stable.
