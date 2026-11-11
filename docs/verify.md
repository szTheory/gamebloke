# Verifying Gamebloke

One command proves a change, locally and in CI:

```sh
cmake --workflow --preset ci
```

CI runs the same command. The workflow adds only routing around it.

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

For a fresh local macOS ASan/UBSan run with the installed AppleClang, use
`cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post`.

`sh scripts/linux-lane.sh <preset> [gcc|clang]` runs any workflow preset on the
host's native architecture. `sh scripts/hygiene.sh --tree` and
`sh scripts/signature-scan.sh --tree` run the two repository scans directly.

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

The profile validator also checks the Phase 3 project-policy fields. Its
policy-specific reason codes are `bus-policy-missing`, `bus-policy-value`,
`bus-policy-source`, `bus-policy-doc`, `init-policy`, and `read-policy`; their
negative controls mutate a scratch copy and the positive companions validate
clean copies.

## Phase 4 runner checks

The runner selectors registered in CI are `runner.hash`, `runner.version`,
`runner.tracer`, `runner.unit`, `runner.result_contract`, `runner.relocation`,
`runner.relocation_control`, `runner.help`, `runner.metadata`,
`runner.usage_error`, and `runner.no_args`. Run the complete producer/metadata and
installed-consumer slice after configure and build with:

```sh
cmake --preset ci && cmake --build --preset ci && \
  ctest --test-dir build/ci \
    -R '^runner\.(hash|version|tracer|unit|result_contract|relocation(_control)?|help|metadata|usage_error|no_args)$' \
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
  `empty-limits` (result documents).
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

`.github/workflows/ci.yml` has six jobs: `changes`, `hygiene`, `title`, `build`
(ubuntu-26.04 GNU and macos-26 AppleClang), `asan`, and `CI required`. The ruleset
requires only `CI required`, which judges every other job. A pull request whose
every changed path is under `.planning/` skips `build` and `asan`; a push to `main`
always runs everything. There is no trigger-level path filter, so the required check
is never left pending.

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
