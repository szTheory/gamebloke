# Verifying Gamebloke

One command proves a change, locally and in CI:

```sh
cmake --workflow --preset ci
```

CI runs the same command. The workflow adds only routing around it.

For a cheap consistency check before a longer run:

```sh
cmake -P cmake/CiPreflight.cmake
```

Every canonical configure runs this check before compiler discovery. It reconciles
the seven workflow definitions (11 expanded required jobs), their stable names,
platform/group routing and exact budgets with `cmake/CiJobBudgets.json`. Only
`build (macos-26, cc, AppleClang)` and `diagnostics (darwin-arm64, arm)` have
300-second budgets. `build (ubuntu-26.04, gcc, GNU)` has a 240-second budget;
the other eight required jobs retain 180 seconds. These whole-job allowances
include setup and cleanup; they are not emulator performance targets. The existing
CI routing test includes missing/misbound-budget, renamed/duplicate-job and
source-pin refusal controls. It also checks explicit admitted stages for all five
required presets and full-history checkout configuration for build, ASan and
diagnostic jobs. Missing or proof-valued stages and shallow checkout mutations
have clean and refusal controls. This preflight does not substitute for the workflow.

The pre-admission producer pin remains
`85c361edfd6b56640e68e64c5e32527f85408836`, with input digest
`95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`.
Preflight verifies the exact current input digest. Full-history checkouts also
verify pinned ancestry and every listed input's bytes and executable mode through
`gbl_check_diagnostic_authority`. Admitted native, sanitizer and diagnostic
configuration refuses shallow history before compiler discovery. Their hosted
checkouts use `fetch-depth: 0`. Shallow hygiene and explicit optional developer
proof metadata checks report that ancestry/modes were not checked. The standalone
`-DREQUIRE_AUTHORITY=ON` check also requires full history.

`scripts/collect-hosted-proof.py` is the promoted project-authored local collector,
using only the local Python standard library. CI and the emulator gain no Python
dependency. Supply `--reviewed-head` with the actual run/current checkout HEAD;
the immutable producer authority comes from the budget policy. The collector
requires all 11 live jobs to be green at that run/HEAD within their exact budgets,
then binds four distinct observed log witnesses to their run/host/group and the
producer pin. The default collector and `final-hosted-proof` CMake validator now
require admitted execution witnesses. `diagnostics.final.outputs` emits them
only after its exact mandatory receipt guard: per-output identity, actual
PASS/GUEST_FAIL/named stop, bounded nonzero progress, source and tested consumer,
run/job/host/group and the exact native API evidence (ARM-owned). No binary is
uploaded. The collector and CMake gate independently reject missing results,
wrong stages, semantic/progress errors and stale consumer/native bindings.
For producer qualification tooling only, select collector `--stage pre-admission`
and CMake `-DHOSTED_STAGE=pre-admission` explicitly; this does not establish
final admitted execution. Run the committed CMake validator after collection.
Its outer receipt binds `reviewed_head`; existing local receipts retain their
actual producer `source_head`. A consumer-only descendant may reuse unchanged
producer proof after bytes/modes/ancestry verification; it does not relabel an old
receipt as evidence for a new producer. Changed producer inputs require a new
freeze and fresh producer proof. Synthetic parser snapshots require explicit
isolated mode, boolean tags on every input, and a marked disposable build
tree containing source, output and all inputs/logs. Live collection refuses
synthetic tags and marked synthetic contexts. The final CMake gate independently
checks all four document tags.
Internal execution and native API receipts also bind `reviewed_head` separately
from producer `source_commit`; missing or stale consumer identities are refused.

## What the command does

The `ci` workflow preset has four steps:

1. **configure** the `ci` preset (Ninja, Release, warnings as errors). Configure
   also compares the registered tests with `tests/inventory/ci.txt`.
2. **build** the `ci` preset.
3. **test**: every test except the `post` label, writing `build/ci/junit.xml`.
4. **ci-post**: the `post` test (`inventory.junit`) checks the junit against the
   inventory and prints `inventory: expected N, ran R, skipped S (allowed A)`.

Every `post` step is verbose on purpose: CTest hides the output of a passing test
otherwise, and that summary line is the readable proof in a green log.

Running `ctest` alone is not the verify command. It skips the build, the post step
and the inventory check, and a stale `junit.xml` is deleted only at configure.

## Lanes

| Lane | Command | Notes |
| ---- | ------- | ----- |
| `ci` | `cmake --workflow --preset ci` | Default lane: build, unit tests, gates, controls. |
| `asan` | `sh scripts/linux-lane.sh asan` | AddressSanitizer and UBSan with clang-21. Linux only; the script runs it in a digest-pinned `ubuntu:26.04` container (needs Docker). |
| `hygiene` | `cmake --workflow --preset hygiene` | The hygiene and signature gates and their controls, plus the Phase 2 evidence validators and their controls, with no compile of the product. |
| `local-timer0` | See [the Timer 0 fixture notice](../tests/fixtures/timer0-NOTICE.md) | Local-only guest generation/indexing and Timer 0 bus-time/determinism checks. Refuses to configure when `CI` is truthy; never use in hosted CI. |
| `local CPU fixtures` | `sh scripts/run-local-cpu-suites.sh --fetch --build-dir build/ci` | Local-only jsmolka ARM, Thumb, and memory suites; never run in hosted CI. |

## Local jsmolka CPU suites

The local command fetches only the three reviewed raw ROM pins from upstream
revision `a7113b67e63f83a9b321696ddd7042ccfad6c881`, verifies each digest before
placement, then rechecks source binding and ignored/untracked fixture storage.
It creates a deterministic guest-bootstrap derivative under the build tree;
the result records the executed derivative digest and the pinned upstream input
digest separately. The original `.fixtures/jsmolka/` files are never modified.
Without `--fetch`, it uses only existing `.fixtures/jsmolka/` files and reports
each missing suite as `MISSING`. Hosted CI is refused before fetch or guest
launch.

Each suite has a 50,000,000 emulated master-cycle budget and a 30-second process
timeout. Before considering those budgets, the local command runs a 50-cycle bus
trace for each ROM and requires its initial stack push to write all three words
to valid IWRAM. It then runs a 2,000-cycle compatibility probe per suite. Only
a `TIMEOUT` with reason `cycle-budget` proceeds to the longer budget; terminal
PASS/failure/stop results are validated, recorded and reused without repeating
the guest run. Both preflights run before a 50,000,000-cycle suite launch.
Preflight results are stored under `<build-dir>/jsmolka/preflight/`. The runner
observes the source-bound result register at its exact
`eval` entry, before the display-only `m_vsync` wait. ARM and memory use R12;
the Thumb suite uses R7 but enters its `eval` label in ARM state. A qualified
PASS requires an observed zero. GUEST_FAIL, GUEST_STOP, TIMEOUT, and harness
failures remain in their result documents and make the command fail.

To run the startup and compatibility preflights without launching a 50,000,000-
cycle suite, run `sh scripts/run-local-cpu-suites.sh --preflight-only --build-dir build/ci`.
This command exits successfully when the bounded probes complete; it does not
mean the three suites passed. Under the current profile, ARM stops at test 234's
malformed `CMP` and Thumb stops at test 227's empty `LDM` list. The memory suite
reaches its zero evaluator. See the [source audit](jsmolka-source-audit.md) for
the exact pinned source and opcode evidence.

The command writes one result document per suite plus
`build/ci/jsmolka/inventory.json`. `jsmolka-bios` is invoked through its
metadata skip path without reading content and must report `SKIPPED`,
`ran=false`, and `official-bios-required`. No BIOS ROM or official BIOS bytes
are fetched.

See [the source and binary audit](jsmolka-source-audit.md) and
[the fixture rights notice](../tests/fixtures/jsmolka-NOTICE.md). The upstream
source declares MIT, but no separate redistribution grant was identified for
the Nintendo logo bytes embedded in the prebuilt ROMs. Keep these ROMs in the
ignored local fixture directory; do not commit, upload, or run them in public
CI. These CPU results do not establish VBlank timing, PPU/graphics behavior,
audio, physical-hardware behavior, or broad compatibility.

For a fresh local macOS ASan/UBSan run with the installed AppleClang, use
an unused build directory and write JUnit inside that actual directory:

```sh
cmake --preset asan -B build/phase05-asan-macos -DCMAKE_C_COMPILER=/usr/bin/clang && \
  cmake --build build/phase05-asan-macos && \
  cmake -E chdir build/phase05-asan-macos ctest --output-on-failure --output-junit junit.xml -LE '^post$' --no-tests=error && \
  cmake -E chdir build/phase05-asan-macos ctest --output-on-failure -L '^post$' --no-tests=error
```

The override identifies an AppleClang host check; it does not change the Linux
`clang-21` preset. Avoid an existing binary directory when fresh compilation is
the intended evidence.

`sh scripts/linux-lane.sh <preset> [gcc|clang]` runs any workflow preset on the
host's native architecture. `sh scripts/hygiene.sh --tree` and
`sh scripts/signature-scan.sh --tree` run the two repository scans directly.
The Linux helper mounts a disposable full-history Git clone with tracked working
edits/deletions and unignored new files overlaid. It never mounts the checkout or
copies ignored build/private contents. Its native-architecture container keeps
the existing four-CPU/eight-GiB limit. Without a verified ARM tool install, its
optional guest skips remain explicit; use the mandatory diagnostic lane for
guest evidence. Container setup time is separate from build/test time.
`linux.snapshot` is a recurring offline CI/hygiene control for this boundary. It
uses a two-commit synthetic repository and a shell Docker witness to check
ancestor bytes, exclusions, working edits/deletions/modes and exact resource/
invocation arguments. Success, failure and actual HUP/INT/TERM paths must remove
the helper's snapshot and exit correctly. No real Docker, private input or
network is used by that unit control.

## Phase 3 bus and loader checks

The focused core command configures and builds before selecting the bus,
machine, and loader checks:

```sh
cmake --preset ci && cmake --build --preset ci && \
  ctest --test-dir build/ci -R '^(bus|machine|loader)\.' --no-tests=error --output-on-failure
```

`bus_test` selectors are `decode`, `timing`, `video`, and `state`; `state` is
registered as `machine.state`. `loader_test` selectors are `lengths`,
`transaction`, `diagnostics`, and `integration`. CTest registers them as
`loader.lengths`, `loader.transaction`, `loader.diagnostics`, and
`loader.integration`. The separate `loader_logo_test` target uses the same
`tests/loader_test.c` and canonical core source list with only an owned synthetic
expected digest. Its `logo-digest` selector is registered as `loader.logo_digest`.
`logo_digest.authority_cases` exercises the configure-time parser with owned scratch
metadata. The integration selector composes the loaded lifecycle
with ROM alias, mirrored RAM, mode-sensitive video, bus-clock, and independent
interleaving checks. It uses synthesized in-memory bytes and does not qualify an
installed consumer or a stable ABI.

The profile validator also checks the Phase 3 and Phase 4 project-policy
fields. Its policy-specific reason codes are `bus-policy-missing`,
`bus-policy-value`, `bus-policy-source`, `bus-policy-doc`, `init-policy`,
`read-policy`, `cpu-policy-missing`, `cpu-policy-source`, `cpu-policy-value`,
and `cpu-policy-doc`; their negative controls mutate a scratch copy and the
positive companions validate clean copies.

## Phase 4 runner checks

The runner selectors registered in CI are `runner.hash`, `runner.version`,
`runner.tracer`, `runner.unit`, `runner.result_contract`, `runner.relocation`,
`runner.relocation_admitted`, `runner.relocation_control`, `runner.help`, `runner.metadata`,
`runner.usage_error`, and `runner.no_args`. Run the complete producer/metadata and
installed-consumer slice after configure and build with:

```sh
cmake --preset ci && cmake --build --preset ci && \
  ctest --test-dir build/ci \
    -R '^runner\.(hash|version|tracer|unit|result_contract|relocation(_admitted|_control)?|help|metadata|usage_error|no_args)$' \
    --no-tests=error --output-on-failure
```

The relocation pair installs only `bin/gamebloke-run`, moves the prefix, and invokes
both build-tree and installed production executables from an unrelated working
directory with the same generated owned image and cycle budget. It compares exact
JSON and trace output bytes, checks semantic/mailbox and time observations, executable
and ROM digests, and rejects source/build/prefix path leakage. Its control detects a
missing installed executable while the matching clean install succeeds. The image is
unregistered in the canonical production manifest, so its expected result is a raw
non-PASS diagnostic; the synthetic control index is not used for this relocation.

`runner.relocation_admitted` additionally uses the actual production index after
`diagnostics.execute`: an admitted ARM semantic positive, its real assertion-broken
companion, and a no-BIOS SWI stop. The gate copies these original inputs to an
explicit owned input directory, relocates the installed prefix, and compares
plain and requested-trace result JSON plus trace bytes against the build runner.
It checks exact case IDs, verdict/reason, actual and admitted digests, bounded
nonzero guest progress and executable identity. Only `bin/gamebloke-run` is
installed; no manifest, CMake script, source tree or index override is supplied
to either runtime. Optional absent-tool skips are inventoried and visible.
Required ARM/all diagnostic lanes register the same installed gate with skips
disabled and the real diagnostic setup fixture. The Thumb split adds no ARM
fixtures. Dependency controls separately require optional absence to be visible
and mandatory relocation absence to fail, with no skip marker.
After building required diagnostics, run the nonoptional gate explicitly:

```sh
cmake -DSRC=. -DBIN=build/diagnostics -DMODE=admitted -P tests/gates/runner_relocation.cmake
```

The phase-end verification commands are `cmake --workflow --preset ci`, then a fresh
local ASan/UBSan configure with the host compiler explicitly selected, followed by the
ASan tests and inventory post-check:

```sh
cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && \
  cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post
```

Run `cmake --workflow --preset hygiene` for the current repository hygiene and signature
gates. The owner-local logo-file signature check is an allowed skip when no local file
is configured; a skip for any newly required CPU/runner check is not a pass. The ASan
workflow remains separately available through the Linux `clang-21` lane; the local
compiler override above does not change that hosted configuration.

### Phase 4 final run evidence (2026-11-11)

| Lane | Result | Measured work |
| ---- | ------ | ------------- |
| `ci` | 329 expected; 328 passed and one allowed `signature.owner_local` skip; `ci-post` passed. | Configure 0.1 s; incremental build had no work; CTest 83.00 s; inventory post 0.08 s. |
| `asan` | Fresh AppleClang 21 ASan/UBSan build; 72/72 tests passed; `asan-post` passed with 72 expected, zero skips. | Fresh configure 0.3 s; 50 build steps; CTest 64.80 s; inventory post 0.02 s. |
| `hygiene` | 204 expected; 203 passed and one allowed `signature.owner_local` skip; `hygiene-post` passed. | Configure 0.0 s; build had no work; final CTest 37.08 s; inventory post 0.04 s. |

The canonical workflow uses one configure/build/test/post sequence; the local CI and
hygiene builds were incremental because no compiled source changed in this plan. The
fresh sanitizer lane compiled the current sources and used the same ASan/UBSan flags for
its scratch mutation controls. The 83-second local CI test critical path remains below
the existing 180-second hosted job cap; no timeout, cache, or concurrency setting was
changed. These are build and test wait times, not emulator throughput measurements.

Bus and loader scratch controls compile disposable source mutations with the
configured compiler and flags. A mutation is negative evidence only when its
scratch build succeeds and the intended semantic assertion fails with the
expected reason. A compiler or setup error is an infrastructure failure, not a
successful control. Every mutation family has a clean companion that must pass.

The host owns ROM storage and provides allocation callbacks. The core validates
declared lengths before reading the image or allocating, copies into the exact
size returned by the host, and commits a complete candidate before releasing
the previous ROM. A refused or allocation-failed load preserves canonical
machine state and caller bytes; reset retains the owned ROM; disposal releases
it once. The callbacks must return suitably sized disjoint storage, must not
reenter or mutate the machine, and must not retain a released pointer. See
[bus-api.md](bus-api.md#rom-loading-and-host-allocation) for the complete
postconditions. These tests prove the selected project contract, not hardware
ROM-tail behavior.

### Logo-window digest diagnostic

After ordinary ROM length, pointer, and allocator validation succeeds, the loader copies
the candidate image and hashes exactly 156 bytes at offsets `[0x04, 0xA0)` (the 0x9C-byte
header window). The resulting match or mismatch is an advisory diagnostic: it does not
refuse the ROM or change the established load transaction. Tests check first- and
last-window-byte changes, bytes just outside the window, loaded reset, and refusal-state
preservation through the real `gbl_load_rom` path.

The production core gets its private expected digest only by reading the existing
`tests/signatures/logo.sha256` row at configure time. Its strict parser accepts comments
and requires one `nintendo-logo-window` row with either `owner-pending` or exactly 64
lowercase hexadecimal characters separated by one space. Invalid metadata fails
configuration with `logo-digest: invalid-authority` and does not echo the row. The
authority file is a configure dependency; the runtime loader never reads it.

The synthetic test core compiles the same source list and target options as production,
but receives an expected digest derived from an owned build-tree pattern containing bytes
1 through 156. This synthetic expectation is independent of the Nintendo authority row.
Zero-byte and pattern-window vectors are fixed independently from the helper, and CMake
also checks the pattern file with `file(SHA256)`. The private helper is an original
fixed-window implementation based on NIST FIPS 180-4 sections 4.1.2, 4.2.2, 5.1.1,
5.3.3, and 6.2.2; this is not a FIPS validation claim and adds no dependency. A synthetic
match proves the loader comparison mechanics only, not Nintendo identity, canonical-logo
acceptance, or rights.

## Adding a test

Register it with `gbl_add_test(NAME ... LABELS ... COMMAND ...)` (see
`cmake/GblTests.cmake`), then add its line to the sorted inventory for each lane
that runs it (`tests/inventory/<lane>.txt`, format `<name> <labels>`). Configure
fails with `inventory: configure-mismatch` or `inventory: unsorted` until both
agree. A test that checks a program's output must also check its exit status: CTest ignores the
exit code when `PASS_REGULAR_EXPRESSION` is set, so run the program through
`cmake -DEXPECT=pass -DCMD=<program> -DARGS=<a|b> -DREGEX=<re> -P cmake/ExpectFail.cmake`
(see `runner.version`). Mark a test `SKIPPABLE` and add `skip-ok` to its inventory line only when it
prints `SKIPPED: <reason>`; any other skip is `inventory: skipped-not-allowed`.

## Negative controls

Every gate has a control that must fail for its stated reason, plus a positive
companion on otherwise identical clean input. Use `gbl_expect_fail(COMMAND ... REGEX
<reason>)` and `gbl_expect_pass(...)` from `cmake/ExpectFail.cmake`: a control passes
only when the gate fails AND its output carries the reason code, so a gate that dies
for the wrong reason, or passes, fails the control. Build the bad input at test time;
never commit it.

## Adding a fixture

A fixture is data first. See [evidence-contract.md](evidence-contract.md) for every field.

1. Write the row in `tests/fixtures/manifest.json` (source, license, build, BIOS need,
   provenance class, permitted use). `manifest.lint` rejects a row with a missing field,
   a branch instead of a 40-hex commit, or an unknown provenance class.
2. For a fetched file, add a row to `pins.txt` (`<sha256> <url> <dest>`, https only) and
   set the row's `fetch.pin_dest` to the same destination.
3. Run `sh scripts/fetch-pin.sh <dest>`. The file lands in `.fixtures/` only after its
   digest matches the pin.
4. Run `cmake --workflow --preset ci`.

Never commit fetched bytes: `.fixtures/` is git-ignored and the `fetch.untracked` gate
fails if anything under it is tracked. A matching hash proves identity, not rights.

## Reason codes

Each gate prints a stable `<family>: <code>` line:

- `hygiene`: `credential`, `email`, `home-path`, `identity`, `usage`.
- `signature`: `logo-window`, `bios-digest`, `rom-extension`, `binary-not-allowlisted`,
  `bad-digest-row`, `missing-list`.
- `inventory`: `missing`, `extra`, `duplicate`, `unsorted`, `failed`, `skipped-not-allowed`,
  `skipped-no-reason`, `disabled`, `no-junit`, `empty-junit`, `configure-mismatch`.
- `core`: `host-include` (the machine core may include only freestanding headers; a quoted
  include must resolve inside the scanned tree, a continued or commented include is still
  an include, and a computed include is refused) and `no-files` (a scanned directory is
  absent or holds no source file).
- `traps`: `exit-with-signals`, `no-signal-handler`, `no-files` (`scripts/check-traps.sh`: a
  script that cleans up on `EXIT` must end on `HUP`, `INT` and `TERM` with `trap 'exit <n>'`;
  one `trap ... EXIT HUP INT TERM` line lets a signalled scan resume and print `ok`).
- `workflow`: the policy gate over every `.github/workflows/*.yml` and `*.yaml` (for example
  `persist-credentials`, `moving-label`, `no-timeout`, `pr-target`, `expression-in-run`,
  `job-permissions`) and the action-pin gate over the same files. `ci.yml` also gets the
  required-check rules (`required-check`, `needs-missing`, and `non-preset-run`: its heavy jobs
  may run only `cmake --workflow --preset <name>` and the toolchain install guard);
  `missing-ci-yml` fires if it is absent. Each line ends with the file name in brackets.
- `title`: `not-conventional`, `too-long`, `empty` (Conventional Commits PR titles).
- `ruleset`, `repo-settings`: the checked-in `main` ruleset and its guarded apply script.

The evidence validators (the contracts are described in [evidence-contract.md](evidence-contract.md))
print `<family>: <code>: <detail>` lines and report every finding, not only the first.
Every JSON file is read by a strict loader: it rejects any text after the root object
(a structural walk, not a parse check), comments, trailing commas, and duplicate keys.
`pins.txt` must start with the `# gamebloke.pins/1 <sha256> <url> <dest>` header line.

- `json`: `usage`, `unreadable`, `not-json`, `not-object`, `comment`, `trailing-comma`,
  `trailing-content`, `duplicate-key` (success in script mode prints `json: ok`).
- `contract`: `usage`, `bad-schema`, `missing-field`, `label-set`, `class-set`,
  `claim-set`, `state-set`, `bad-state-rule` (`contracts/evidence.json`).
- `registry`: `usage`, `bad-schema`, `missing-field`, `bad-id`, `duplicate-id`,
  `bad-status`, `no-sources`, `bad-source-label`, `seed-missing`
  (`registry/disputes.json`).
- `profile`: `usage`, `bad-schema`, `bad-version`, `missing-field`, `bad-machine`,
  `bad-boot`, `bad-bios-provider`, `bad-cart`, `missing-source`, `bad-source-label`,
  `timing-arithmetic`, `hblank-edge-hardcoded`, `unmodeled-missing`,
  `unmodeled-bad-entry`, `dispute-unknown`, `doc-no-section`, `doc-missing-id`,
  `doc-extra-id`, `doc-version-mismatch` (`profiles/agb.json` against
  `docs/profile-agb.md`).
- `manifest`: `usage`, `bad-schema`, `missing-field`, `wrong-type`, `bad-id`,
  `duplicate-id`, `bad-source-kind`, `unpinned-revision`, `missing-digest`, `bad-url`,
  `bad-path`, `bad-provenance-class`, `bad-bios-requirement`, `bad-permitted-use`,
  `bad-redistributable`, `rights-mismatch`, `dispute-unknown`, `dispute-class-mismatch`,
  `pin-mismatch`
  (`tests/fixtures/manifest.json`).
- `pins`: `missing-file`, `missing-header`, `bad-row`, `bad-digest`, `bad-url`,
  `bad-dest`, `duplicate-dest` (`pins.txt`).
- `fetch-pin`: `usage`, `missing-pins`, `no-curl`, `no-cmake`, `bad-dest`, `no-row`,
  `bad-row`, `bad-digest`, `bad-url`, `download-failed`, `bad-status` (not a 2xx answer;
  redirects are not followed), `empty-response`, `digest-tool-failed`, `digest-mismatch`,
  `symlink`, `dest-is-dir` (`scripts/fetch-pin.sh`; stderr lines, exit 2 for `usage`,
  `missing-pins`, `no-curl`, `no-cmake`, exit 1 for the rest; success prints
  `fetch-pin: ok: <dest>`).
- `fixture-dir`: `usage`, `git-failed`, `not-ignored`, `tracked` (the `.fixtures/`
  directory must be git-ignored and hold nothing tracked).
- `result`: `usage`, `bad-schema`, `missing-field`, `unknown-status`, `pass-without-run`,
  `pass-without-digest`, `ran-state-mismatch`, `missing-digest`, `missing-reason`,
  `missing-refusal`, `bad-machine`, `bad-boot`, `bad-bios-provider`, `bad-cart`,
  `bad-input-timeline`, `bad-evidence`, `claim-mismatch`, `unknown-fixture`,
  `unknown-dispute`, `undisclosed-dispute`, `label-without-dispute`, `bad-label`,
  `empty-limits`, `bad-semantic`, `status-semantic-mismatch`, `bad-time`,
  `time-overflow`, `cycles-exceed-budget`, `bad-termination`, `bad-trace`,
  `incomplete-trace` (result documents).
- Test helpers (`tests/gates/json_mutate.cmake`): `mutate: no-match` (a control's mutation
  changed nothing, so the control would have tested an unmutated copy) and
  `no-echo: leaked` (a validator printed file content it must never print).

## Signatures, allowlist, and the logo digest

Only SHA-256 digests are committed, never logo, BIOS or ROM bytes. The digest lists
are `tests/signatures/logo.sha256` (a 156-byte window at header offset 4) and
`tests/signatures/bios.sha256` (a 16384-byte file). `tests/signatures/allowlist.txt`
exempts exact paths from the `rom-extension` and `binary-not-allowlisted` rules only;
it never exempts the logo or BIOS digests. The current logo row was derived from
two owner-provided local files under explicit authorization; their windows
matched. Only the digest is published. Profile version 3 activates the loader
comparison, and `signature.logo.real_digest` checks that the authority is present.

`GBL_OWNER_LOGO_FILE` optionally enables a local cross-check against an owner-held
file. It remains skipped in public CI, which receives no private file. Local
copies live in the already-ignored `/private/owner-roms/` folder; no additional
allowlist exemption is added. A matching hash proves identity, not rights,
original-cartridge provenance or broad compatibility. Pending-authority builds
remain supported and explicitly report unchecked/skipped rather than success.

## Opt-in hook

`git config core.hooksPath .githooks` enables a pre-commit check of the staged
files and the commit identity (`git config --unset core.hooksPath` turns it off).
The CI `hygiene` job is the enforcing gate; the hook only saves a round trip.

## CI shape

`.github/workflows/ci.yml` defines `changes`, `hygiene`, `title`, `build`
(ubuntu-26.04 GNU and macos-26 AppleClang), `asan`, `diagnostics`
(ARM and Thumb groups on both hosts), and `CI required`. The ruleset
requires only `CI required`, which judges every other job. A pull request whose
every changed path is under `.planning/` skips `build` and `asan`; a push to `main`
always runs everything. There is no trigger-level path filter, so the required check
is never left pending.

## Original ARMv4T diagnostics

The complete local guest lane is `cmake --workflow --preset diagnostics`.
`diagnostics-arm` owns the smoke pair, ARM cases and API-only witnesses;
`diagnostics-thumb` owns the Thumb cases. The four hosted diagnostic jobs retain
that split. Each preset has its own literal sorted inventory, including
`tests/inventory/diagnostics-arm.txt`; configure and the JUnit post step both
reconcile actual registration against it.

The pinned tool root is `build/toolchains`, overridable with
`-DGBL_TOOLCHAIN_ROOT=<directory>` at configure. The existing installer is
`sh scripts/fetch-toolchain.sh --root build/toolchains`; its `--check` operation
verifies an installed host package without downloading. Native ci/asan metadata
generation does not need an ARM toolchain. A genuinely absent optional install
produces exactly `SKIPPED: arm-toolchain-absent` for the inventoried guest cases.
A present but unusable install fails. Required diagnostic lanes and
`GBL_REQUIRE_DIAGNOSTICS=ON` reject absence, wrong identity and unusable tools;
compile, link, header, map, signature, guest and result-checker failures are never
converted into tool-absence skips. No compiler or BIOS fallback exists.

`diagnostics.execute` runs the selected complete group once, retaining actual
runner JSON, native TAP and fresh execution receipts under the build directory.
Every `diagnostics.case.*` test checks its own real result, exact nonzero case ID,
semantic state, stop reason and output identity. `diagnostics.execution_set`
reconciles the complete required output set and the API-only native assertions.
It cannot accept an empty or partial receipt set, a stale configure token,
changed producer inputs, changed runner/ROM/result bytes or a required all-skipped
run. API-only rows retain their native boundary; they never acquire invented ROM
fixtures. The actual declared-BIOS control reports `SKIPPED` with `ran=false`
and zero progress. Zero/missing IDs and incomplete mailboxes cannot become
completed PASS/GUEST_FAIL results.

`GBL_DIAGNOSTIC_STAGE=proof` is explicit pre-admission tooling. Its private
production index projects historical metadata with an immutable unqualified
marker. Even matching historical ROM bytes requested with `--fixture` are
refused as `fixture-unknown` before execution. Raw source observations remain
`GUEST_STOP`/unregistered results; the canonical result checker refuses their
missing fixture admission. The separately generated artificial control index
continues testing the runner contract. Production and control targets use the
same runner sources and options. A proof metadata projection can work in a
shallow checkout without granting historical source authority.

Default/admitted index generation still requires real source ancestry and exact
producer freshness. Before changing a listed producer input, including ledger
witness state or its checker, prepare a new source-only revision. Build the full
output set, obtain fresh complete local literal byte parity and the real hosted
group witnesses, then re-admit exact output digests against that revision.
Unchanged ROM bytes alone do not permit reusing a changed producer identity's old
proof. Consumer-only descendants keep the original producer pin, verify all
listed producer bytes/modes and ancestry, and bind their actual tested HEAD
separately as described above.
Required admitted execution needs sufficient Git history to verify the actual
pinned ancestor. Arrange and verify that history before hosted publication.

During an explicitly staged source-proof run, a native metadata build can be
configured with `-DGBL_DIAGNOSTIC_STAGE=proof`; this setting grants no guest
fixture authority. All canonical native, sanitizer and all/ARM/Thumb diagnostic
presets explicitly select `admitted`, overriding a cached proof stage on each
workflow configure. A diagnostic lane, mandatory diagnostics or `CI=true`
refuses a proof override before compilation. Optional local native proof remains
developer tooling. Restore `admitted` after a developer proof experiment.
`cmake -DCASE=mandatory-final -DSRC=. -DBIN=build/diagnostics
-P tests/gates/diagnostic_dependency.cmake` refuses proof mode and verifies the
complete admitted receipt set when that stage is available.

Before publishing a changed admission/history policy, run the real independent
clone control with a fresh contained output path:

```sh
cmake -DSRC=. -DBIN=build/ci-history-control -P tests/ci/history_cases.cmake
```

It overlays the current consumer policy into full and depth-one local clones,
configures full-history native/ASan and all/ARM/Thumb diagnostic presets using the
existing checked tool install, and refuses shallow admitted configuration before
compiler discovery. It neither executes a hosted job nor grants hosted authority.
The ASan configure control explicitly uses `/usr/bin/clang` on macOS; hosted Linux
ASan retains its `clang-21` preset and distinct compiler claim.

The local-only `tests/ci/admitted_hosted_cases.py` takes an actual mandatory-final
log and the preserved local producer proof. It checks that real log, then tests
both hosted parsers with explicitly tagged synthetic API documents and logs in
a marked disposable build tree. Its green controls never qualify a hosted run.

Network-free dependency controls run with
`cmake -DCASE=all -DSRC=. -DBIN=build/phase05-dependency
-P tests/gates/diagnostic_dependency.cmake` after the native runners are built.
The diagnostic ARM lane additionally instruments the real compiler pipeline's
failure paths with clean source-built companions. It reuses only the real
aggregate builder's matching parent verification, avoiding repeated whole-ledger
audits; it never manufactures a successful guest or hosted-proof receipt.

## Slow-lane checks

- `sh scripts/linux-lane.sh <preset>`: about 5 to 15 minutes, needs Docker. Without
  Docker the first Linux proof is the CI run.
- CMake 3.25 floor: download the official Kitware `cmake-3.25.3` tarball, verify its
  SHA-256 against Kitware's published list, extract it outside the repository, and run
  `<that cmake> --workflow --preset ci` from the repository root. About 2 to 5 minutes,
  needs network. Never install CMake from PyPI.

## Extension points

`manifest.lint` is registered in `tests/gates.cmake` and runs inside
`cmake --workflow --preset ci` with no preset change (VERIF-01 manifest clause,
EVID-02). The other Phase 2 registrations, the validator gates and their controls, live
in the evidence section of the same file.

## Phase 7 time and Timer 0 checks

After a `ci` build has been configured, run the short model-time gate before
longer device and guest checks:

```sh
scripts/verify-phase7-fast.sh
```

The script builds `timer_test`, validates the selected test inventory, and
requires exactly these five CTest cases to be discoverable and run:
`time.events.order`, `time.events.boundary`, `time.events.refusal`,
`timer0.model.boundary`, and `phase7.selection.control`. It rejects an empty,
missing, extra, broad, duplicated, or malformed selection with a named control
reason. CTest's `phase7_fast` fixture makes the longer CPU, runner,
determinism, timer, Timer 0, time-event, and timing-authority tests wait for all
five setup cases; a failed setup blocks dependent checks even under parallel
execution. `phase7.order` checks the registered graph and a failing/succeeding
fixture sentinel. The fast script needs an existing configured build directory;
start a fresh tree with `cmake --preset ci`.

The single canonical full local command remains:

```sh
cmake --workflow --preset ci
```

Run a long workflow in the background and retain its exit status and output;
its configure and build time are separate from CTest's summed test runtime.
After the code-review fixes, the fast-script invocation took 8.45 seconds
wall clock; the selector test took 8.46 seconds in the full local workflow, and
the `phase7.order` control took 2.51 seconds. The latest local CI workflow
reported 1,131 tests, zero failures, 405 executed, and 726 allowed
ARM/Thumb-toolchain skips; the JUnit case-time sum was 1,189 seconds. The
rights-restricted Timer 0 CLI guest is excluded from CI/ASan and runs only in
`local-timer0`. These measurements do not claim emulator throughput or
hardware timing.

The timer and bus cost evidence uses `gamebloke-timer-model-1`, classified as
`project-model-only`. `project-selected` access costs and timer boundary rules
are reproducible implementation policy; `hardware-sourced` rows carry only
their cited scope, and `unmodeled` categories remain unknown. In particular,
the uniform access-cycle choices do not qualify bus timing. The original
Timer 0 polling guest is generated locally from project-owned test source,
uses no BIOS, and reports mailbox semantic completion, machine cycles, and the
Timer 0 request flag separately. Its generated images are build-local and
covered by [the Timer 0 fixture notice](../tests/fixtures/timer0-NOTICE.md);
redistribution remains unknown and permitted use is local-only. The `ci` and
`asan` lanes do not generate or index those images and do not register the
guest execution test. Run the guest only in the guarded `local-timer0` lane as
shown in the notice. The disabled
timer control times out with `timer0-overflow-timeout`; neither result proves
CPU IRQ exception entry or broad compatibility.

The full-workflow evidence, including actual totals and allowed skips, is
recorded in [Phase 7 evidence](../.planning/phases/07-time-authority-and-timer-0-polling-proof/07-EVIDENCE.md).
The `DIAG-04` ARM/Thumb evidence gap and `DET-02` hosted-x64 evidence gap remain
open carryovers. Hardware timing edges `TIME-01` and `BUS-06` remain
unclassified. The six prohibition judgments were individually accepted after source/evidence review; hardware-edge classification and Phase 6 carryovers remain open.

## Phase 8 IRQ, HALT, and timestamped input checks

The canonical local command remains `cmake --workflow --preset ci`. It builds,
runs the complete configured CI inventory, and checks its JUnit results. Run it
once after the focused checks below when source, configuration, fixtures, and
toolchain inputs are current. This command is also the CI entry point; local
results do not establish hosted CI evidence.

For quick feedback, run:

```sh
scripts/verify-phase8-fast.sh
ctest --test-dir build/ci --output-on-failure --no-tests=error \
  -FA '^(phase7_fast|phase8_fast)$' \
  -R '^phase8[.](order|policy)$'
```

The fast script first passes the exact five Phase 7 prerequisites on the same
build inputs, then checks the exact five Phase 8 cases: `cpu.irq.boundary`,
`keypad.timeline`, `keypad.refusal`, `time.halt.boundary`, and
`phase8.selection.control`. The follow-up command runs the registered ordering
and profile-policy gates after the fast fixtures have passed, excluding only
automatic fixture insertion for those explicit selections. The ordering gate
checks the real CTest graph and its failed-predecessor control. The policy gate
compares runtime observations with the AGB profile and documentation, including
its drift controls. Both inventories list those gates with their exact labels.

For task-focused checks after a `ci` build:

```sh
build/ci/cpu_test irq.boundary
build/ci/cpu_test irq.refusal
build/ci/bus_test irq.registers
build/ci/timer_test keypad.timeline
build/ci/timer_test keypad.condition
build/ci/timer_test keypad.refusal
build/ci/bus_test keypad.registers
build/ci/cpu_test power.boundary
build/ci/timer_test time.halt.boundary
build/ci/timer_test time.halt.refusal
ctest --test-dir build/ci --output-on-failure --no-tests=error \
  -R '^(runner[.](help|input[.]cli|relocation)|determinism[.]phase8([.]controls)?)$'
```

The runner input-script adapter accepts at most 16 KiB total and 256 records;
it probes one byte beyond the size cap before scanning input. This bounds host
file work and does not add a host clock or physical-controller path to the
machine. Input is a deterministic logical timeline. Model cycles, wait cycles,
IF requests, sampled IRQ eligibility, exception entry, keypad state, guest
progress, and HALT wake are separate observations.

IRQ, keypad, HALT, event ordering, and costs use the declared
`gamebloke-irq-input-model-1` policy and are `project-model-only`. A no-BIOS IRQ
may end with the classified `GUEST_STOP` (`no-bios-irq`); this does not mean a
BIOS handler ran. These tests do not establish GBA IRQ latency, physical input
behavior, broad game compatibility, or hardware qualification. BIOS-stub or
IRQ-driven playable work remains behind G4. `DIAG-04` and `DET-02` remain open
evidence carryovers; local checks do not authorize hosted CI or publication.

## Phase 9 WAITCNT and DMA checks

Configure the existing `ci` build before using the fast script:

```sh
cmake --preset ci
scripts/verify-phase9-fast.sh
```

The script requires a configured `build/ci` whose CMake source root is this
checkout. To reuse another configured build for this checkout, pass
`scripts/verify-phase9-fast.sh --bin DIR`. A missing or foreign build, missing
test executable, wrong CTest selection, skipped/not-run required check, or
failing predecessor exits nonzero. The script builds only `bus_test` and
`dma_test`, runs the Phase 8 fast script on the same build, binds the current
source/build with `phase9_selection.cmake CASE=verify`, then runs this exact
seven-test Phase 9 predecessor set:

- `bus.waitcnt.matrix`
- `bus.waitcnt.boundary`
- `dma.registers`
- `dma.events`
- `phase9.policy`
- `phase9.dispute`
- `phase9.selection.control`

The Phase 8 script also checks its exact five cases and the Phase 7
prerequisites. These focused commands exclude automatic fixture insertion for
the already-passed Phase 7/8 fast fixtures; normal `ci` and ASan inventories
retain their configured fixture graphs. Before a longer run, the direct order
gate checks exact membership, test commands, labels, inventories, and that a
failed predecessor blocks dependent Phase 9 cases:

```sh
cmake -DSRC=. -DBIN=build/ci -DCASE=order -P tests/gates/phase9_selection.cmake
```

`CASE=verify` checks the configured CTest selection; `CASE=control` exercises
causal selection and ordering failures. A successful script prints
`verify-phase9-fast: exact-seven passed`. A selected check that CTest reports
as skipped or not run is rejected; the full workflow separately reports all
allowed optional skips and their reasons in its inventory summary.

The longer semantic checks stay in the existing inventories. CI and ASan share
these registered selectors:

- `bus.waitcnt.registers`, `cpu.waitcnt`
- `cpu.dma.cpu`, `cpu.dma.refusal`, `cpu.dma.tracer`, `cpu.dma.waitcnt`
- `dma.arbitration`, `dma.controls`, `dma.events`, `dma.latch`,
  `dma.refusal`, `dma.registers`
- `determinism.phase9`, `determinism.phase9.controls`
- `phase9.dispute`, `phase9.dispute.control`, `phase9.order`,
  `phase9.policy`, `phase9.policy.control`, `phase9.selection.control`

The `result.pos.phase9` and `result.nc.phase9` typed-result cases are CI-only.
The required CI command also runs the existing inventory post-check, which
reports allowed lane skips and their declared reasons.

The unchanged canonical local and required CI command is:

```sh
cmake --workflow --preset ci
```

It configures, builds, runs the required CI inventory, and checks the JUnit
inventory. Run it once as the final local witness after the cheap Phase 9
policy, dispute, selection, and order checks. The Phase 9 additions stay in the
existing CI workflow and configured build; no separate lane is added.

Read the results as distinct observations. The model's elapsed cycles, DMA
startup/source/destination costs, completed units, and the reported CPU pause
are separate fields; CPU pause is explanatory and is not added to elapsed time.
Semantic transfer bytes and addresses, channel ownership, and device IF/events
are checked independently. A passing guest transfer does not prove hardware
latency. The manual-backed WAITCNT rows, project-selected costs, and the
community-sourced 128 KiB forced-N boundary keep their separate provenance;
hardware mapping remains `UNKNOWN`. Prefetch execution is unmodeled. The
per-channel retained DMA latch remains project-selected and disputed under
`disp-dma-latch`; all dependent observations retain that dispute link.

This local command does not run hosted CI, read BIOS or ROM content, qualify
physical hardware, close `DIAG-04`/`DET-02`, or authorize content use or
publication. G4 and the existing rights, carryover, and owner gates remain in
force.
