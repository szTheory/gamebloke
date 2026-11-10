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
| `hygiene` | `cmake --workflow --preset hygiene` | The hygiene and signature gates and their controls, with no compile of the product. |

`sh scripts/linux-lane.sh <preset> [gcc|clang]` runs any workflow preset on the
host's native architecture. `sh scripts/hygiene.sh --tree` and
`sh scripts/signature-scan.sh --tree` run the two repository scans directly.

## Adding a test

Register it with `gbl_add_test(NAME ... LABELS ... COMMAND ...)` (see
`cmake/GblTests.cmake`), then add its line to the sorted inventory for each lane
that runs it (`tests/inventory/<lane>.txt`, format `<name> <labels>`). Configure
fails with `inventory: configure-mismatch` or `inventory: unsorted` until both
agree. Mark a test `SKIPPABLE` and add `skip-ok` to its inventory line only when it
prints `SKIPPED: <reason>`; any other skip is `inventory: skipped-not-allowed`.

## Negative controls

Every gate has a control that must fail for its stated reason, plus a positive
companion on otherwise identical clean input. Use `gbl_expect_fail(COMMAND ... REGEX
<reason>)` and `gbl_expect_pass(...)` from `cmake/ExpectFail.cmake`: a control passes
only when the gate fails AND its output carries the reason code, so a gate that dies
for the wrong reason, or passes, fails the control. Build the bad input at test time;
never commit it.

## Reason codes

Each gate prints a stable `<family>: <code>` line:

- `hygiene`: `credential`, `email`, `home-path`, `identity`, `usage`.
- `signature`: `logo-window`, `bios-digest`, `rom-extension`, `binary-not-allowlisted`,
  `bad-digest-row`, `missing-list`.
- `inventory`: `missing`, `extra`, `duplicate`, `unsorted`, `failed`, `skipped-not-allowed`,
  `skipped-no-reason`, `disabled`, `no-junit`, `empty-junit`, `configure-mismatch`.
- `core`: `host-include` (the machine core may include only freestanding headers).
- `workflow`: the policy gate over every `.github/workflows/*.yml` and `*.yaml` (for example
  `persist-credentials`, `moving-label`, `no-timeout`, `pr-target`, `expression-in-run`,
  `job-permissions`) and the action-pin gate over the same files. `ci.yml` also gets the
  required-check rules (`required-check`, `needs-missing`, and `non-preset-run`: its heavy jobs
  may run only `cmake --workflow --preset <name>` and the toolchain install guard);
  `missing-ci-yml` fires if it is absent. Each line ends with the file name in brackets.
- `title`: `not-conventional`, `too-long`, `empty` (Conventional Commits PR titles).
- `ruleset`, `repo-settings`: the checked-in `main` ruleset and its guarded apply script.

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

Phase 2 adds the manifest lint (VERIF-01, EVID-02) as a `manifest.lint` gate test
registered in `tests/gates.cmake`, with no preset change.
