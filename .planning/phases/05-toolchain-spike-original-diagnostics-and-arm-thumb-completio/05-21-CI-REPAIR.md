# Bounded CI repair and local preflight

The owner-renewed repair is implemented and locally verified. Source commit:
`cadd236a60db954c61ee744455b2e2c7cfc74822` (`fix(05-21): reduce duplicate CI control compilation and parsing`).
The parent acts next: independent review, exact publication freeze/proof refresh,
and the single authorized hosted retry. No hosted attempt was used here. Plan21
admission, shared planning completion and Phase6 remain outside this repair.

## Changes and preserved checks

- Added the missing `result.pos.instruction_reasons positive,result` hygiene
  inventory entry. Before the repair, actual `cmake --preset hygiene` refused
  configure with the exact missing-registration reason. The complete actual
  native and Ubuntu hygiene workflows now configure, build, test and validate
  their post-inventories.
- Added the narrow `tests/gates/mutation_objects.cmake` helper. Each normal
  build prepares fresh control objects once using the existing harness's exact
  compiler, C17 mode, flags, warning policy, instrumentation and link policy.
  Before each reuse it checks the resolved compiler executable's bytes,
  compiler ID, project version, compile/link flags, both helper scripts, every
  original project source/header/test input, copied input sets and object bytes.
  Eligible mutants recompile their changed translation unit and link a separate
  mutant executable; their clean companion still links separately and runs.
  The build output is local to one configured lane and refreshed on every build;
  it is not a shared or persistent cache. System SDK/header installations are
  assumed stable within the single job; no reuse crosses jobs or hosts.
- Kept the existing six-case CPU source-only eligibility list. Header-changing
  CPU controls and synthetic-logo loader controls retain full compilation.
  Unknown mutations still fail their mutation dispatch. All existing concrete
  semantic failure expressions, clean companions, names, labels, sanitizer
  controls and inventory rules remain. A compiler/setup error is still refused
  as semantic mutation evidence.
- Removed repeated whole-receipt parsing when reading an individual JSON
  array. Flat output rows use one extraction only after CMake proves that
  reconstructing them is JSON-equivalent to the original array. Nested or
  unusual string content falls back to the general parser. All existing output
  row, source, tool, hash, authority, required-job and budget checks remain.
  The exact parent signature witness and its seven actual refusal controls
  are unchanged.

No workflow, timeout, required job, concurrency, dependency, producer source,
CPU behavior, fixture pin, license/rights record, private content or publication
setting changed. The producer-input digest remains
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.

## Measured costs and negative controls

Evidence root: `build/phase05-ci-renewal/` (ignored local artifacts).

| Observation | Result and limit | Evidence |
| --- | --- | --- |
| Actual pre-repair hygiene configure | Refused exactly the missing inventory registration; 0.15s wall | `hygiene-red.log`, `hygiene-red-time.log` |
| Same three representative bus/loader/CPU mutation controls | Before 7.62s wall, 4.75user, 0.92sys; after 3.39s wall, 1.00user, 0.43sys. Preparation measured separately at 7.35s wall, 5.34user, 0.85sys; it is not free | `measurements/baseline-mutations*`, `measurements/reuse-mutations*`, `build-time.log` |
| Complete mutation controls | All25 pass with concrete expected failures and clean companions; 44.09s wall, 19.46user, 4.86sys | `mutation-green.log`, `mutation-green.xml`, `mutation-green-time.log` |
| Reuse refusal controls | Existing `cpu.pos.clean` first runs the clean CPU executable, then separately refuses changed flags, source and headers before compilation; also passes in both complete sanitizer lanes | `preflight/native-asan.log`, `preflight/linux/clang21-asan.log` |
| Complete diagnostic receipt controls | Clean plus every existing nine refusal classes pass, with added flat/nested/braces/semicolon extraction-equivalence checks. Initial row-only change44.26s; array extraction28.27s. These sequential local observations are not hosted timings | `proof-controls*`, `proof-controls-v2*` |
| Matched722-name array micro-measurement | Same value digest before/after; 1.21s wall/0.99user/0.03sys becomes0.23s/0.19user/0.01sys. Measures one helper only | `measurements/receipt-array*.log`, `measurements/receipt-array.cmake` |
| Native ROM byte preservation | Every722 fresh native output literally equals the preserved reviewed-head native output; no new two-host parity or hosted witness claim | `preflight/native-byte-preservation.json` |

The missing-inventory configure refusal is honest baseline evidence, not a
CTest/JUnit TDD RED claim. Already-green behavior was checked with isolated
fault controls; production code was not regressed to manufacture RED.

The failed hosted logs were preserved. Attribution from their actual completed
test rows is in `measurements/hosted-attribution.json`: completed mutation
controls cost134.91s in Linux normal,75.61s in macOS normal and159.07s in ASan.
Those cancelled jobs remain failures. macOS diagnostic receipt controls cost
49.42s; Linux x64 controls38.18s. No result is inferred for missing test rows.

## Complete frozen-source preflight

All lanes below ran source `cadd236a60db954c61ee744455b2e2c7cfc74822` from
tracked-only `git clone --no-local` checkouts. Each completed its real
configure/build/test/post path and complete JUnit inventory. Native ASan used
Apple Clang through the configured compiler override; Linux used GNU15.2 and
Clang21.1.8 with unchanged warning and sanitizer flags.

| Actual lane | Wall / user / sys seconds | Inventory result |
| --- | --- | --- |
| Native hygiene cold workflow | 89.66 /52.59 /32.95 |205 entries; only the allowed owner-local skip; post passes |
| Native canonical CI cold workflow |189.43 /98.04 /56.76 |355 entries; only the allowed owner-local skip; post passes |
| Native ASan configure |1.71 /0.36 /0.50 | Configured Apple Clang with full instrumentation |
| Native ASan build |13.45 /20.05 /4.09 | Build and fresh prepared objects pass |
| Native ASan CTest |139.85 /50.27 /37.70 |97 entries, no skips |
| Native ASan post |0.07 /0.03 /0.02 | Complete inventory passes |
| Native verified tool check |0.31 /0.03 /0.08 | Exact installed xPack15.2.1-1.1 passes; no download |
| Native diagnostics cold workflow |207.70 /90.68 /67.28 | Six executable checks, no skips; post passes |
| Pinned Linux hygiene cold workflow |98.29 /70.70 /31.23 |205 entries; only the allowed owner-local skip; post passes |
| Pinned Linux GNU canonical cold workflow |186.60 /180.07 /45.70 |355 entries; only the allowed owner-local skip; post passes |
| Pinned Linux Clang21 ASan cold workflow |96.70 /105.24 /10.36 |97 entries, no skips; post passes |

Exact logs, JUnit paths and timing values are indexed by
`preflight/summary.json`; commands are in `preflight.sh`. Native logs are
`preflight/native-*.log`, Linux logs/JUnit/timings are in `preflight/linux/`.
`preflight-continuation.log` ends with the exact frozen HEAD and
`PREFLIGHT COMPLETE`. The pinned native ARM64 image was
`ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`,
limited to four CPUs/eight GiB. It mounted the source read-only, cloned tracked
content into `/work`, and never copied ignored/private workspace content.
Container provisioning plus all three sequential Linux lanes took448.33s;
that orchestration interval is not a hosted lane duration.

The ignored orchestration helper initially called the fetcher on an existing
installation without `--check`. Its expected `install-exists-use-check` refusal
stopped before diagnostics; `preflight.log` preserves it. The corrected helper
resumed at the verified tool check, retaining passed native suites rather than
repeating them. No package install failed, no tool was redownloaded, and no
source fix was required for that helper invocation.

## Critical path and fixed180-second limits

Local correctness preflight passed; a current hosted timing pass does not exist.
Native canonical CI189.43s, native diagnostics207.70s and Linux GNU186.60s are
explicitly above180s locally. Do not label them budget passes or use them as a
hosted-performance guarantee. Host scheduling and filesystem behavior differ;
this is not an emulator-throughput benchmark.

Current native canonical CTest178.81s contains42.25s of mutation controls,
47.35s of hygiene/signature checks and35.71s of profile checks
(`preflight/native-ci-attribution.json`). The actual failed hosted macOS run
contained75.61s,27.43s and26.79s respectively. These different environments
cannot establish a precise end-to-end speedup. The focused same-command
measurements above establish the removed duplicate work.

Current native diagnostic build groups cost9.06s smoke,87.38s ARM and58.49s
Thumb; receipt controls30.97s, output revalidation4.06s and offline controls
10.41s. Preserved reviewed-head group observations were7.13/50.30/33.86s,
with receipt controls42.24s. Producer inputs and all722 native bytes are
unchanged. The extra current producer wall cost must not be presented as a
change in emulator behavior or proof that hosted producer time grew.

The prior failed real diagnostic job finished tests/post but was cancelled
at182s. Its real checkout/fetch/cleanup interval is already part of that failed
job; the unchanged fetch step was16s on each host. The verified local tool
check0.31s does not replace the hosted fetch allowance. Reducing the measured
receipt-parse work is a concrete improvement, but neither historical setup
arithmetic nor local noise proves sufficient hosted margin. Every actual
required hosted job must complete green within180s, including setup and cleanup.
No timeout was enlarged and no gate was removed to satisfy this comparison.

## Handoff and limits

The parent owns independent review and any authorized review fixes, the final
source freeze/proof refresh, publication and the one-attempt counter. This
report commit moves HEAD without changing producer inputs; do not relabel
old-head receipts. Obtain the fresh exact-source proof at the publication
freeze as already planned. Do not admit fixtures or complete Plan21 from these
local results. A new hosted red stops implementation, fixes and retries until
owner renewal; cancelled, missing or skipped required jobs cannot be accepted.

No unresolved implementation stub or unrun required local preflight remains.
No shared STATE/ROADMAP/requirements/WINDOWS/routing/stop/retry record was
modified. Existing owner changes and untracked planning notes were preserved.
Fresh precommit tree/staged hygiene, signature scan and diff-whitespace checks
passed for the source commit. The only new file access is trusted build-local
test preparation; no network endpoint, content admission or new external trust
boundary was introduced.

Documentation consulted: official CMake3.25
[custom targets](https://cmake.org/cmake/help/v3.25/command/add_custom_target.html)
and [file operations](https://cmake.org/cmake/help/v3.25/command/file.html), after
Context7 MCP and the `ctx7` executable were unavailable.

## Self-check

Source commit exists on the phase branch; all five changed source files and this
report exist. Frozen native/Linux completion markers, all seven JUnit files,
post-inventory logs and timing receipts were checked. Source and producer input
identities match the recorded frozen commit. No hosted attempt was dispatched.
