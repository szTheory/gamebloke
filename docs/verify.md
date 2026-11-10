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

`sh scripts/linux-lane.sh <preset> [gcc|clang]` runs any workflow preset on the
host's native architecture. `sh scripts/hygiene.sh --tree` and
`sh scripts/signature-scan.sh --tree` run the two repository scans directly.

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
  `bad-redistributable`, `dispute-unknown`, `dispute-class-mismatch`, `pin-mismatch`
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
it never exempts the logo or BIOS digests. The logo row is `owner-pending` until the
owner supplies a digest: run `sh scripts/signature-digest.sh logo <file>` on a cartridge
image you already hold, outside this repository, and replace the row with the printed
line. A matching hash proves identity, not rights. Until then `signature.logo.real_digest`
reports `SKIPPED`.

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
