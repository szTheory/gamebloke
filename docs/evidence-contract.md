# The evidence contract

This guide explains the data files that decide what a fixture run may claim: the
machine profile, the fixture manifest, the pins list, the dispute registry, and the
result document. Each file has one authority and one validator; the validators run
inside `cmake --workflow --preset ci` (see [verify.md](verify.md)). The reason codes
each validator prints are listed in the "Reason codes" section of verify.md.

A matching hash proves identity, not redistribution rights. A pin or a digest says
"these are the bytes that were reviewed"; it never says the bytes may be copied,
shipped, or published. Rights live in the manifest `license` and `permitted_use`
fields and are decided by the owner.

## Authority files

| Path | Schema id | Holds | Checked by |
| ---- | --------- | ----- | ---------- |
| `contracts/evidence.json` | `gamebloke.evidence/1` | The closed vocabularies: source labels, provenance classes, claim fields, and the nine result states with their rules | `cmake/CheckContract.cmake`, gate `contract.lint` |
| `profiles/agb.json` | `gamebloke.profile/1` | The machine profile: boot, BIOS provider, cart, eleven timing constants, the unmodeled list | `cmake/CheckProfile.cmake`, gate `profile.lint` |
| `registry/disputes.json` | `gamebloke.disputes/1` | Known disagreements between references, implementations, and reports | `cmake/CheckRegistry.cmake`, gate `registry.lint` |
| `tests/fixtures/manifest.json` | `gamebloke.manifest/1` | One row per fixture: source, license, build, BIOS need, provenance, permitted use | `cmake/CheckManifest.cmake`, gate `manifest.lint` |
| `pins.txt` | `gamebloke.pins/1` (header line) | Reviewed pinned fetches, one row per downloaded file | `cmake/CheckManifest.cmake` (pins mode), run by gate `manifest.lint` |
| `contracts/examples/*.json` | `gamebloke.manifest/1`, `gamebloke.result/1` | Illustrative documents used as positive inputs for the controls | `cmake/CheckManifest.cmake`, `cmake/CheckResult.cmake` |

Every JSON file is read by `cmake/GblJson.cmake`, a strict loader. CMake's own parser
accepts comments, trailing commas, content after the root object, and duplicate keys;
the loader rejects all four, so a hand edit fails the gate instead of passing silently.

The profile narrative is [profile-agb.md](profile-agb.md). `profiles/agb.json` stays
the authority; a check compares the document's unmodeled table and its
`profile_version:` line with the JSON, so the two change together.

## Claim tuple

A result never says only "PASS". It says what passed, under which claim. The claim has
six fields, named in `contracts/evidence.json` under `claim_fields`:

- `machine`: exactly `agb` in v1.0.
- `boot`: exactly `direct-start`. No result may claim boot-ROM behavior.
- `bios_provider`: one of the forms in the next section.
- `cart`: an object, `kind` exactly `rom-only`, and `rom_sha256` either null or 64
  lowercase hex.
- `input_timeline`: an object whose `kind` is `none`, or `script` with a 64-hex
  `sha256` of the input script.
- `evidence`: a non-empty array of items, each naming a `fixture` (a manifest row id)
  and a `provenance_class` from the vocabulary. The class must equal the one in that
  fixture's manifest row.

## BIOS provider

The accepted `bios_provider` forms (the R1 grammar) are:

- `none`
- `project-stub`
- `hle:` followed by one or more names of letters, digits, and underscores, separated
  by commas
- `replacement:` and a lowercase name, then an at-sign and a 40-character lowercase
  hex commit (shape `replacement:<name>@<commit>`)
- `user-official`, then an at-sign and a 64-character lowercase hex SHA-256 (shape
  `user-official@<sha256>`)

Names inside an `hle:` list are not checked against a table of BIOS calls. The older
milestone labels (M0, M1, M1b, M2) are retired and match none of the forms. The one
function that decides this is `gbl_bios_provider_valid` in `cmake/CheckContract.cmake`;
the profile validator and the result validator both call it.

## Result states

A run ends in exactly one of nine states. The table below mirrors
`contracts/evidence.json`, which stays the authority; `cmake/CheckContract.cmake` pins
the locked rows independently, so weakening the data fails the contract gate.

| State | `fixture.ran` | ROM digest | Reason | Refusal |
| ----- | ------------- | ---------- | ------ | ------- |
| `PASS` | true | required | optional | none |
| `GUEST_FAIL` | true | required | optional | none |
| `GUEST_STOP` | true | required | optional | none |
| `HARNESS_FAIL` | either | optional | optional | none |
| `TIMEOUT` | true | required | optional | none |
| `CANCELLED` | either | optional | optional | none |
| `MISSING` | false | optional | required | none |
| `SKIPPED` | false | optional | required | none |
| `REFUSED` | false | optional | optional | required |

A fixture that did not run is never PASS: `PASS` needs `ran` true and a digest, so a
missing, skipped, or refused fixture cannot be reported as passing. `GUEST_STOP`
carries no required reason until Phase 4 defines what a stop looks like. A refusal is
an object with a non-empty `code` and `postcondition`.

## Result documents

A result (`gamebloke.result/1`, an example is `contracts/examples/result-pass.json`) has
these fields:

- `fixture`: `id`, `ran` (a boolean), and `rom_sha256` (64 lowercase hex or null).
- `status`: one of the nine states, exact case.
- `reason` and `refusal`: as the state row requires.
- `claim`: the six claim fields above.
- `disputes`: registry ids. `labels`: result labels. `limits`: what the result does not
  prove, a non-empty list of non-empty strings.

`cmake/CheckResult.cmake` cross-checks the result against the manifest and the registry:
`fixture.id` and every evidence fixture must be a manifest row id; the evidence class
must equal the row's class; `claim.cart.rom_sha256` must equal `fixture.rom_sha256`
(two nulls are equal); some evidence item must name `fixture.id`.

Dispute disclosure: when the manifest row for the fixture, or for any evidence fixture,
carries a dispute id, the result's `disputes` list must contain it, and a non-empty
`disputes` list requires the `disputed` label. The `disputed` label with an empty list
is rejected. Every id in `disputes` must exist in the registry. `disputed` is the only
label; a new label would arrive with its own rule and a `gamebloke.result/2` bump.

Extra top-level keys (such as `note`) are ignored until Phase 4, when the runner starts
emitting results and the semantic and time rules arrive.

## Dispute registry

`registry/disputes.json` holds entries with `id`, `summary`, `status`, `affects`, and
`sources`. An id matches `disp-` followed by lowercase letters, digits, and dashes. The
only status is `open`. Each source has a `label` from the vocabulary, a `ref`, and a
`note`. Six seed ids are pinned by the validator, so deleting or renaming one fails the
gate: `disp-dma-latch`, `disp-gbatek-errata`, `disp-hblank-flag-edge`,
`disp-mgba-suite-22`, `disp-mgba-suite-24`, and `disp-mgba-suite-27`. A profile value
that is not settled carries the label `UNK` and a dispute id, never a number.

## Manifest rows

`tests/fixtures/manifest.json` has a `fixtures` array. It holds zero rows today, which
is the stated empty state: Phases 5 and 6 add the first fixtures. The row fields and
the EVID-01 item each one satisfies:

| Field | Satisfies | Rule |
| ----- | --------- | ---- |
| `id` | identity | `^[a-z0-9][a-z0-9._-]*$`, unique |
| `source` | source and revision | `kind` is `git`, `url`, or `in-tree`; see below |
| `license` | license and notices | `spdx`, a non-empty `notices` array, and `redistributable` of `yes`, `no`, or `unknown` |
| `build` | build recipe and toolchain | `recipe`, and `toolchain.identity` plus `toolchain.sha256` (null allowed) |
| `bios_requirement` | BIOS need | `none`, `project-stub`, `hle`, `replacement`, or `user-official` |
| `expected` | expected-value provenance | `provenance_class` from the vocabulary; `dispute` is null or a registry id |
| `permitted_use` | where it may run | a non-empty unique array of `public-ci`, `local-only`, `differential-only` |
| `fetch` | how it is obtained | optional; when present, `pin_dest` is a relative path |

Source kinds and pinning rules (D-13, R-3): a `git` row carries a `url` (https only) and
a `commit` of exactly 40 lowercase hex; a branch or tag name is not a pin. A `url` row
carries an https `url` and a 64-hex `sha256`. An `in-tree` row carries a relative `path`
and a `sha256`, and has no `fetch`. A `disputed` provenance class needs a dispute id, and
a dispute id under any other class is rejected.

The provenance classes are `project-authored`, `hardware-observed`,
`reference-emulator`, `suite-self-checking`, and `disputed`.

## Pins and fetch

`pins.txt` starts with the header line `# gamebloke.pins/1 <sha256> <url> <dest>`. Each
row after it has three single-space-separated fields: a 64-hex digest, an `https://` URL,
and a destination relative to `.fixtures/`. A pin names one raw file at a 40-hex commit
or a maintainer-uploaded release asset, never a generated archive. A header-only file is
the stated zero-row state. A manifest row's `fetch.pin_dest` must match a pins row, and
the digest or commit must agree (`pin-mismatch` otherwise).

Fetch a pinned file with:

```sh
sh scripts/fetch-pin.sh [--pins <file>] [--root <dir>] <dest>
```

The script downloads into a temporary directory on the same filesystem as the
destination, hashes the payload, and moves it into place only when the digest equals the
pin. A mismatch leaves nothing at the destination. Exit status 0 means
`fetch-pin: ok: <dest>`; exit 2 covers usage, a missing pins file, and a missing curl;
every other failure exits 1. Certificate verification is never disabled.
`file://` URLs work only with `GBL_FETCH_ALLOW_FILE=1`, which exists for the tests;
the committed `pins.txt` never contains one, and the tests never use the network.

Fetched bytes land in `.fixtures/`, which git ignores. The gate `fetch.untracked`
(`cmake/CheckFixtureDir.cmake`) proves the ignore rule is present and that nothing under
`.fixtures/` is tracked, so a forced add fails.

## Reading further

- [verify.md](verify.md): the verify command, lanes, and every reason code.
- [profile-agb.md](profile-agb.md): the profile narrative and its sync check.
