# Phase 2: Profile, Fixture Manifest, and Result Contract - Research

**Researched:** 2026-11-10
**Domain:** Hand-written CMake `-P` validators over JSON authority files, a POSIX pinned-fetch script, and a closed result vocabulary, all inside the Phase 1 gate/inventory harness
**Confidence:** HIGH for the validator mechanics (probed on CMake 3.25.3 and 4.4.3), MEDIUM for the GBA reference facts (community sources), LOW only where a claim is labelled `[UNK]` or `[ASSUMED]`

Evidence labels: provenance tags `[VERIFIED]`, `[CITED]`, `[ASSUMED]` per the GSD contract. Project labels from `.planning/research/SUMMARY.md` (`[REF]` community hardware reference, `[IMPL]` what one artifact does, `[INF]` inference, `[UNK]` undetermined, `[REC]` recommendation) are added where a hardware claim is involved. No GBATEK or Nintendo prose is quoted below; sections are cited by name.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Contract formats and validation tooling**
- The profile, manifest, result contract, and dispute registry are JSON files. They are machine-readable authority files; the Phase 4 runner will emit JSON. `string(JSON)` is available at the CMake 3.25 floor.
- Validation is hand-written CMake `-P` validators with stable reason codes, in the style of `cmake/CheckSignatureDigests.cmake`. No JSON Schema tool or other new dependency.
- `profiles/agb.json` is the authority for the profile. `docs/profile-agb.md` is narrative that cites it; a check keeps the unmodeled list in the JSON and the document in sync.
- Every validator reason code has an `ExpectFail` negative control built from synthesized mutations at test time (no committed bad-example fixtures), registered in `tests/gates.cmake` with matching lines in the sorted `tests/inventory/<lane>.txt` files, and a positive control per validator.

**The `agb` profile content**
- Every timing constant carries exactly one source label from the project's evidence vocabulary: `REF`, `SRC`, `IMPL`, `INF`, `UNK`. `calibrated:<fixture>` is reserved for later phases.
- The profile id is `agb` only. The SP is `agb` plus a host presentation preset, claimed only with evidence. `agb-micro` is deferred until owner hardware exists (G12). SP/AGS naming follows research position R8.
- The profile declares `schema: gamebloke.profile/1` and an integer `profile_version` that is bumped on any claim-affecting change.
- The unmodeled list has the three named items plus module-level entries. Named: SP/Micro machine-visible differences (the 0x04000800 EWRAM-wait register, the Micro lacking the cartridge switch), prefetch until modeled, and the HBlank flag edge (U1; sources disagree at 1,006, 1,007, and 1,008 cycles, so never hard-code 960). Module-level not-yet-modeled: PPU, audio, DMA, timers, IRQ controller, backup media.

**Claim tuple, result taxonomy, and dispute registry**
- The claim tuple is `{machine, boot, bios_provider, cart, input_timeline, evidence}`. `cart` is `{kind: "rom-only", rom_sha256}`. `input_timeline` is `{kind: "none"}` or a script digest. `evidence` is a list of `{fixture, provenance_class}`.
- `bios_provider` follows exactly the R1 grammar: `none`, `project-stub`, `hle:<swi,...>` (list non-empty), `replacement:<name>@<commit>`, `user-official@<digest>`. Anything else is rejected; SWI names are not checked against a table. STACK's M0/M1/M1b/M2 BIOS labels stay retired.
- The nine states are data: PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT, CANCELLED, MISSING, SKIPPED, REFUSED. The result validator rejects `PASS` unless the fixture ran with a digest present; `SKIPPED` and `MISSING` require a reason; `REFUSED` requires a refusal code and a postcondition note; an unknown state is rejected. A fixture that did not run is never PASS.
- `registry/disputes.json` holds entries with an id, a summary, labeled sources, a status of `open`, and affected areas. Seeded with: the DMA-latch dispute, the GBATEK errata, the HBlank edge sources, and mGBA-suite issues #22, #24, #27. A fixture whose expected value points at an entry yields a result labeled `disputed`.

**Manifest linter and pinned fetch**
- Git sources must pin a 40-hex commit (a tag may appear only as an extra field). Non-git sources must pin a SHA-256. Branch names, `HEAD`, and `main` are rejected.
- Expected-value provenance classes: `project-authored`, `hardware-observed`, `reference-emulator`, `suite-self-checking`, `disputed`. An unknown class is rejected.
- `pins.txt` rows are `<sha256> <url> <dest>`. A POSIX `scripts/fetch-pin.sh` verifies the digest before placing the file in the untracked `.fixtures/` directory (added to `.gitignore`). A mismatch exits non-zero with a loud error. No third-party binary is ever committed.
- Tests never use the network. The digest-mismatch control uses a locally synthesized file through a `file://` URL. No third-party fixture is fetched in this phase.

### Claude's Discretion
File layout beyond the named paths, exact reason-code names (stable once chosen), manifest row field names (must cover every EVID-01 field: source URL, pinned revision, license and notices, build recipe, toolchain identity/digest, BIOS requirement, expected-value provenance class, permitted use), JSON key ordering, and the number of synthesized mutations per validator.

### Deferred Ideas (OUT OF SCOPE)
- Runner emitting the result JSON, and PROF-03 enforcement: Phase 4.
- Fixture content, toolchain identity, and manifest rows for owned diagnostics: Phase 5.
- jsmolka third-party manifest rows and pinned fetch of real suites: Phase 6.
- A JSON Schema document for external consumers: revisit when an external consumer exists.
- Validating SWI names against a table: revisit when the project-stub or HLE exists.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| PROF-01 | Versioned `agb` profile: machine, boot `direct-start`, BIOS provider, ROM-only cart, timing constants with source labels, explicit unmodeled list (SP/Micro, prefetch, HBlank edge) | "Profile content" and "Authoritative GBA facts" sections give verified constants and section names; validator rule set and reason codes under "Reason-code catalogue" (family `profile`) |
| PROF-02 | Every result carries claim tuple; BIOS provider only R1 values | Result JSON design, R1 grammar regexes (probed), `result` reason codes |
| EVID-01 | Manifest row declares source URL, pinned revision, license+notices, build recipe, toolchain identity/digest, BIOS requirement, provenance class, permitted use | Proposed row shape under "Manifest row shape"; `manifest` reason codes |
| EVID-02 | Linter rejects missing fields, unpinned revisions, unknown provenance class | `manifest` family; registration at `manifest.lint`; sibling patterns |
| EVID-03 | SHA-256 pinned fetch from `pins.txt`; mismatch fails loudly; no third-party binary committed | `scripts/fetch-pin.sh` prototype (probed under dash and `/bin/sh`), curl flags, `fetch-pin` and `pins` reason codes, `.gitignore` and `git ls-files` checks |
| EVID-05 | Nine-state taxonomy; a fixture that did not run is never PASS | Data-driven state table in `contracts/evidence.json`; `result` family rules |
| EVID-06 | Dispute registry seeded; disputed expected value labels the result | Seed list verified against primary sources (issue titles, source constants); `registry` family; result `disputed` label rules |
</phase_requirements>

## Project Constraints (from CLAUDE.md and AGENTS.md)

- Read the root `AGENTS.md` first; `.planning/bootstrap/` is dated input, verify before relying on it. [VERIFIED: .claude/CLAUDE.md read this session]
- Keep source facts, observations, inferences, recommendations, and unknowns distinct; do not transfer another emulator's hardware assumptions. [VERIFIED: AGENTS.md "GSD discussion and project learning"]
- Prefer a small, flat dependency tree. No new dependency is introduced by this phase (CMake, POSIX sh, curl, and `cmake -E sha256sum` already exist in the harness). [VERIFIED: AGENTS.md; see Package Legitimacy Audit]
- No ROMs, Nintendo BIOS bytes, private saves, credentials, or unlicensed test content in source control or public CI. Record each fixture's source, revision, build recipe, digest, license, notices, BIOS requirements, and permitted use. "A matching hash proves identity, not redistribution rights." [VERIFIED: AGENTS.md "Product and engineering direction"]
- Give important gates a meaningful negative control; missing and skipped checks must be visible. [VERIFIED: AGENTS.md "Verification, CI, and releases"]
- Refused operations need documented status and postconditions; never infer permission from a public download, a test-suite license, or a digest. [VERIFIED: AGENTS.md "Rights and security"]
- Do not add a LICENSE file (owner gate G1). Publication is one PR for this phase, squash-merged through the ruleset when `CI required` is green; stop on red. [VERIFIED: .planning/STATE.md, standing authorization entry]
- Every new file must pass `scripts/hygiene.sh --tree` and `scripts/signature-scan.sh --tree`, including this research file and the docs it implies.

## Summary

The phase is mostly mechanical once three facts are respected, all probed this session on CMake 3.25.3 (the official Kitware tarball, SHA-256 verified against Kitware's published list and equal to the Phase 1 evidence digest) and 4.4.3. First, `string(JSON)` is a lenient parser: it silently accepts trailing commas, comments, trailing garbage after the root, leading-zero numbers, raw tabs in strings, and duplicate keys (the last one wins). A validator built only on `string(JSON)` therefore cannot detect a hand-edit mistake that strict JSON would reject. A 25-line "strictness guard" (strip string literals with one regex, then reject comments, trailing commas, trailing content, and a raw key-token count that exceeds the parsed member count) closes all four holes and behaved identically on both versions. Second, the 3.25 and 4.4 behaviors are identical for every subcommand this phase needs; the only differences are error-message text (4.4 echoes the input) and `GET_RAW`, which exists only from 4.3. Third, `-P` script mode leaves policies unset on 3.25.3 (several are already NEW on 4.4.3), so every entry script and every library file must start with `cmake_minimum_required(VERSION 3.25)`, exactly as Phase 1 established.

The architecture is five small validator scripts plus one shared loader, one data contract, and one shell tool: `cmake/GblJson.cmake` (loader and strictness guard), `contracts/evidence.json` (closed vocabularies and the nine-state rule table), `cmake/CheckProfile.cmake`, `cmake/CheckRegistry.cmake`, `cmake/CheckManifest.cmake` (manifest plus `pins.txt` lint), `cmake/CheckResult.cmake` (contract and result), and `scripts/fetch-pin.sh`. Controls are case drivers in the style of `tests/gates/signature_case.cmake`: copy the real committed file into a sandbox, mutate it with `string(JSON SET/REMOVE)`, and assert a non-zero exit plus the stable reason code through `cmake/ExpectFail.cmake`. Because mutations start from the real file and the positive control is the unmutated copy, a control cannot drift from the data it protects. Expect roughly 90 to 110 new tests per lane (the gate file is included by both `ci` and `hygiene`, not `asan`), about 5 more seconds of hygiene-lane time at `-j4`. The registration point named in `CMakeLists.txt` and `docs/verify.md` is a gate test called `manifest.lint` in `tests/gates.cmake`; no preset change is needed.

All six dispute-registry seeds were checked against primary sources. Four items differ from, or sharpen, what the research files assumed and are listed under "Findings that differ from earlier research".

**Primary recommendation:** Build the shared strict-JSON loader first, drive every validator from the loader plus `contracts/evidence.json`, derive every control by mutating a copy of the real committed file, and give each reason code its own ctest line so the inventory proves each control ran.

## Architectural Responsibility Map

This is a verification-infrastructure phase, so the tiers are repository layers rather than web tiers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Closed vocabularies (source labels, provenance classes, nine result states) | Authority data (`contracts/evidence.json`) | Validators read it | "The nine states are data" (locked). One file avoids three hard-coded copies drifting |
| Profile facts and unmodeled list | Authority data (`profiles/agb.json`) | `docs/profile-agb.md` cites it | JSON is the authority; the doc is checked for sync |
| Manifest rows and pins | Authority data (`tests/fixtures/manifest.json`, `pins.txt`) | Validator cross-checks them | Rows describe provenance; pins carry digests |
| Structural validation and reason codes | CMake `-P` validators | Shared loader `cmake/GblJson.cmake` | Locked: no JSON Schema tool; `string(JSON)` is available at the floor |
| Fetch with verify-before-place | POSIX sh (`scripts/fetch-pin.sh`) | `curl`, `cmake -E sha256sum` | Locked: POSIX script; hashing via the tool Phase 1 already uses |
| Control registration and inventory proof | CTest via `gbl_add_test` in `tests/gates.cmake` | `tests/inventory/{ci,hygiene}.txt` | Locked: sorted inventories with matching lines; the junit post step proves every control ran |
| Dispute disclosure on results | Result validator | Manifest and registry cross-references | A result must carry `disputed` when its fixture points at a registry entry |
| Rights and hygiene guard on new files | Existing `hygiene.sh` and `signature-scan.sh` | `.gitignore` for `.fixtures/` | Fetched bytes live outside the tracked tree |

## Standard Stack

No external package is added. Everything below already exists in the harness. [VERIFIED: CMakeLists.txt, scripts/, cmake/ read this session]

### Core
| Tool | Version (this machine) | Purpose | Why standard here |
|------|------------------------|---------|-------------------|
| CMake `-P` script mode with `string(JSON)` | 3.25 floor; local 4.4.3; floor probe 3.25.3 | All validators | Locked decision; sibling-proven (`cmake/CheckSignatureDigests.cmake`, `cmake/RulesetCheck.cmake`) |
| CTest + `gbl_add_test` + sorted inventories | CMake 3.25+ | Registration and proof of execution | Phase 1 harness |
| `cmake/ExpectFail.cmake` (`gbl_expect_fail`, `gbl_expect_pass`) | in repo | Negative and positive controls | Asserts non-zero exit AND stable reason text |
| POSIX `sh` (`set -eu`, `trap` idiom enforced by `scripts/check-traps.sh`) | dash and macOS `/bin/sh` both probed | `scripts/fetch-pin.sh` | Phase 1 script conventions |
| `curl` | 8.7.1 here (SecureTransport/LibreSSL build); Ubuntu runner image lists 8.5.0 for 24.04 | Download in `fetch-pin.sh` | Needed for the `file://` control; see Environment Availability |
| `cmake -E sha256sum` | 3.25+ | Digest in `fetch-pin.sh` | Already the project's hash tool: `gbl_sha256` in `scripts/lib/signature-common.sh` |

### Supporting
| Tool | Purpose | When to use |
|------|---------|-------------|
| `shellcheck` (local only, not a dependency of the build) | Lint the new script | Optional local check; the prototype was clean under `shellcheck -s sh` |
| `git check-ignore`, `git ls-files` | Prove `.fixtures/` is ignored and no third-party binary is tracked | EVID-03 success criterion 3 |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-written validators | A JSON Schema tool | Locked out: new dependency; revisit when an external consumer exists |
| `curl` in `fetch-pin.sh` | `cmake -P` with `file(DOWNLOAD ... EXPECTED_HASH ...)` | Would remove the curl dependency and is always present with cmake, but the locked decision is a POSIX `fetch-pin.sh`. Keep sh; add curl to the Linux lane packages |
| `shasum -a 256` / `sha256sum` | `cmake -E sha256sum` | cmake is the one tool guaranteed present in every lane; the Ubuntu runner image list does not list a separate hash tool |

**Installation:** none. Add `curl` to the apt line in `scripts/linux-lane.sh` (currently `ca-certificates cmake ninja-build git libc6-dev`). [VERIFIED: scripts/linux-lane.sh:61 `apt-get install -y -qq --no-install-recommends ca-certificates cmake ninja-build git libc6-dev $GBL_PACKAGES >/dev/null`]

**Version verification:** CMake 3.25.3 tarball `cmake-3.25.3-macos-universal.tar.gz` downloaded to a scratch directory outside the repository; `shasum -a 256` gave `771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e`, identical to the line in Kitware's `cmake-3.25.3-SHA-256.txt` from the same release and to `.planning/phases/01-ci-and-verification-foundation/01-EVIDENCE.md` line 11. [VERIFIED: download plus hash compare this session]

## Package Legitimacy Audit

No external package is installed by this phase (no npm, PyPI, or crates dependency). The `gsd-tools package-legitimacy` seam was not run because there is nothing to check.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none) | - | - | - | - | - | - |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none
`curl` is a system tool, not a package this repository installs on developer machines; the Linux-lane container installs it from the Ubuntu archive like the other tools in that apt line.

## Architecture Patterns

### System Architecture Diagram

```
                      authority data (tracked, reviewed)
   profiles/agb.json   contracts/evidence.json   registry/disputes.json
   tests/fixtures/manifest.json                  pins.txt        docs/profile-agb.md
          |                    |                       |               |
          v                    v                       v               v
   +-----------------------------------------------------------------------------+
   | cmake/GblJson.cmake: gbl_json_load(<out> <file>)                            |
   |   exists -> file(READ) -> TYPE parse -> top-level OBJECT -> strictness guard |
   |   (strip string literals; reject comment / trailing comma / trailing        |
   |    content; raw key-token count vs parsed member count => duplicate key)    |
   +-----------------------------------------------------------------------------+
          |              |                |                  |
          v              v                v                  v
   CheckProfile      CheckRegistry    CheckManifest      CheckResult
   (+ doc sync,      (seed ids,       (rows, pins.txt    (contract mode:
    + dispute x-ref)  source labels)   lint, x-refs to    vocab + state table;
          \              |             registry/pins)     result mode: tuple,
           \             |                |                state rules, BIOS
            \            |                |                grammar, disputed)
             +-----------+----------------+------------------+
                         |
              each prints "<family>: <code>: <detail>" with plain message()
              (stderr, unwrapped), then ends with a short FATAL_ERROR; exit 1
                         |
          +--------------+-------------------------------+
          v                                              v
   real-file gate tests (tests/gates.cmake)       control drivers tests/gates/<family>_case.cmake
   profile.lint registry.lint manifest.lint       copy real file -> mutate via string(JSON SET/REMOVE)
   result.contract                                -> gbl_expect_fail(REGEX "<family>: <code>")
          |                                       positive control = unmutated copy (gbl_expect_pass)
          v
   CTest -> junit -> tests/inventory/{ci,hygiene}.txt compare (missing or skipped control = red)

   scripts/fetch-pin.sh <dest>   (separate path; shell cannot reuse the CMake validators)
     pins.txt row (3 fields) -> validate dest + digest + scheme -> mktemp under .fixtures/
     -> curl (https only; file:// only with GBL_FETCH_ALLOW_FILE=1) -> cmake -E sha256sum
     -> compare -> symlink/escape check -> mv (same filesystem, atomic) -> .fixtures/<dest>
```

### Recommended Project Structure
```
cmake/
  GblJson.cmake             # loader + strictness guard (library; also pins cmake_minimum_required)
  CheckProfile.cmake        # MODE-less entry: -DFILE= -DDOC= -DREGISTRY= -DVOCAB=
  CheckRegistry.cmake
  CheckManifest.cmake       # -DMODE=manifest|pins|all
  CheckResult.cmake         # -DMODE=contract|result
contracts/evidence.json     # source labels, provenance classes, nine states (data), claim field list
profiles/agb.json
docs/profile-agb.md         # narrative; unmodeled table parsed for sync
docs/evidence-contract.md   # claim tuple, result shape, reason-code tables, dispute rules
registry/disputes.json
tests/fixtures/manifest.json  # schema gamebloke.manifest/1, "fixtures": [] in this phase
pins.txt                    # header comments only in this phase
scripts/fetch-pin.sh
tests/gates.cmake           # + includes tests/evidence_gates.cmake (same directory scope)
tests/evidence_gates.cmake  # registrations for this phase (optional split; keep top-level scope)
tests/gates/{json,profile,registry,manifest,pins,fetch,result}_case.cmake
```
Names other than the ones fixed by CONTEXT (`profiles/agb.json`, `docs/profile-agb.md`, `registry/disputes.json`, `pins.txt`, `scripts/fetch-pin.sh`, `.fixtures/`) are `[REC]` and discretionary. `tests/fixtures/manifest.json` is deliberately not `fixtures/` at the root, to avoid confusion with the untracked `.fixtures/` fetch directory. `tests/fixtures/manifest.json` and `pins.txt` are text, so the signature scan's binary and extension rules do not touch them. [VERIFIED: scripts/signature-scan.sh header rules read]

### Pattern 1: Shared strict loader (the first thing to build)

**What:** One function every validator calls. It is the only place that calls `string(JSON)` on a whole file.
**Why:** Probed behavior shows `string(JSON)` alone would accept `{"a":1,}`, `// c` followed by JSON, `{"a":1} trailing`, `{"a":1}{"b":2}`, and `{"k":"first","k":"second"}` (last wins). Hand-edited authority files fail in exactly these ways.
**Probed prototype (behaved identically on 3.25.3 and 4.4.3):**
```cmake
# Source: probed in this session; not repo code yet.
cmake_minimum_required(VERSION 3.25)   # pin in the library file too (policies at definition time)

function(gbl__json_count_members out json)
  string(JSON ty TYPE "${json}")
  set(n 0)
  if(ty STREQUAL "OBJECT")
    string(JSON len LENGTH "${json}")
    set(n ${len})
    if(len GREATER 0)                      # REQUIRED: foreach(RANGE -1) iterates twice (see Pitfall 2)
      math(EXPR last "${len}-1")
      foreach(i RANGE ${last})
        string(JSON m MEMBER "${json}" ${i})
        string(JSON cty TYPE "${json}" "${m}")
        if(cty STREQUAL "OBJECT" OR cty STREQUAL "ARRAY")   # GET of a string is not JSON: never recurse blindly
          string(JSON child GET "${json}" "${m}")
          gbl__json_count_members(c "${child}")
          math(EXPR n "${n}+${c}")
        endif()
      endforeach()
    endif()
  elseif(ty STREQUAL "ARRAY")
    string(JSON len LENGTH "${json}")
    if(len GREATER 0)
      math(EXPR last "${len}-1")
      foreach(i RANGE ${last})
        string(JSON cty TYPE "${json}" ${i})
        if(cty STREQUAL "OBJECT" OR cty STREQUAL "ARRAY")
          string(JSON child GET "${json}" ${i})
          gbl__json_count_members(c "${child}")
          math(EXPR n "${n}+${c}")
        endif()
      endforeach()
    endif()
  endif()
  set(${out} ${n} PARENT_SCOPE)
endfunction()

# strictness guard: returns a list of problems (empty = clean)
string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "S" stripped "${text}")   # global, left-to-right, aligned on real string starts
#   comment:          stripped MATCHES "//|/\\*"
#   trailing-comma:   stripped MATCHES ",[ \t\r\n]*[]}]"
#   trailing-content: stripped MATCHES "[}\\]][ \t\r\n]*[{\\[S0-9a-z]"   and root must match ^[{[]...[]}]$ after STRIP
#   duplicate-key:    count of  S[ \t\r\n]*:  in stripped  >  gbl__json_count_members(parsed)
```
Probe results for the guard (same on both versions): clean nested object with `"x:y"` and `"https://x/y"` values passed; `{"a":1,"a":2}` raw=2 parsed=1; `{"a":{"k":1,"k":2}}` raw=3 parsed=2; `{"a":1,}` trailing-comma; `// c` plus JSON comment; `{"a":1} {"b":2}` and `{"a":1} trailing` rejected; a string containing `\"a\":b` and one ending in `\\` passed; arrays of objects reusing a key per row passed. [VERIFIED: probe this session]
**Residual:** leading-zero and `+1` numbers, raw tabs inside strings, a BOM, and invalid UTF-8 are accepted and harmless (numbers are normalized by `GET`; validators check numeric fields by regex on the `GET` text). [VERIFIED: probe this session]

### Pattern 2: Typed field access (never trust `GET` alone)

`GET` returns the same empty string for `""` and `null`, `ON`/`OFF` for booleans, and the verbatim digits for numbers (`1e5` becomes `100000.0`, `-0` becomes `0`, `12345678901234567890` is preserved). Use `TYPE` first, then `GET`, then a regex or `STREQUAL` on the text. A required string field is present only if `TYPE` is `STRING` and `LENGTH`-style non-empty; a missing path with `ERROR_VARIABLE` sets `<out>` to `<path>-NOTFOUND` and the error to `member '<k>' not found`. [VERIFIED: probe this session]

### Pattern 3: Report every finding, then one short fatal line

`message(FATAL_ERROR ...)` wraps text at about 78 columns and indents it, so a long reason line can be split across lines and a regex on the reason may miss. A plain `message("family: code: detail")` goes to stderr unwrapped; `message(STATUS ...)` goes to stdout with a `-- ` prefix. Print each finding with plain `message()`, collect a count, then end with `message(FATAL_ERROR "family: failed (N findings)")`. `ExpectFail` merges stdout and stderr, so both work. [VERIFIED: probe this session; cmake/ExpectFail.cmake:53-54 `execute_process(COMMAND ${X_COMMAND} ${wd} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)`]

### Pattern 4: Mutation controls built from a copy of the real file

Copy the committed file into `${BIN}/nc/<case>/`, apply one mutation, run the real validator, assert reason. The positive control is the unmutated copy. This is the `digests_copy` pattern. [VERIFIED: tests/gates/signature_case.cmake:285-289 `# Each mutates a sandbox COPY; \`digests_copy\` proves the unmutated copy is sound, so every control below fails only for its planted reason.`]
`string(JSON REMOVE)` of a missing member is a silent success that returns the document unchanged (both versions), so a misspelled mutation target produces an unmutated copy and a control that "passes" for the wrong reason. Every mutation helper must assert the mutation took effect (`string(JSON EQUAL before after)` must be false), mirroring the existing `mutate: no-match` guard. [VERIFIED: probe this session; tests/gates/signature_case.cmake:126-143 `mutate_line` fails with `mutate: no-match`]
Offending strings in the case drivers must be assembled from fragments where they resemble a hygiene pattern (credentials, emails, home paths), as `tests/gates/hygiene_case.cmake` does.

### Pattern 5: Registration

Register in `tests/gates.cmake` (or a file it includes at the same directory scope). [VERIFIED: CMakeLists.txt:64-67 `#   tests/gates.cmake   gate tests (hygiene and signature in this phase, and the\n#                      Phase 2 \`manifest.lint\` test that completes VERIF-01's\n#                      manifest clause, EVID-02); created by plan 03 (ci and hygiene lanes)`] and [VERIFIED: docs/verify.md:124-125 `Phase 2 adds the manifest lint (VERIF-01, EVID-02) as a \`manifest.lint\` gate test\nregistered in \`tests/gates.cmake\`, with no preset change.`]. `tests/gates.cmake` is included only for the `ci` and `hygiene` lanes, never `asan`: [VERIFIED: CMakeLists.txt:77-80 `if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "hygiene")\n  include(tests/gates.cmake)\n  include(tests/ci_gates.cmake)\nendif()`]. Every new test therefore needs a line in both `tests/inventory/ci.txt` and `tests/inventory/hygiene.txt`, and none in `asan.txt`. Names must match `^[a-z0-9][a-z0-9._-]*$` [VERIFIED: cmake/GblTests.cmake:18 `if(NOT T_NAME MATCHES "^[a-z0-9][a-z0-9._-]*$")`]. Inventory line format and sort: [VERIFIED: tests/inventory/ci.txt:1 `# Line format: <name> <label,label>[ skip-ok]. Single spaces. Labels sorted.`], byte-order sorted (`LC_ALL=C sort`; `_` sorts after `.` and before lowercase letters).

### Anti-Patterns to Avoid
- **Hard-coding the vocabularies in three validators.** Read `contracts/evidence.json`; check the pinned expected sets once in the contract validator.
- **Committing bad-example fixtures.** Locked out; synthesize.
- **Dumping matched text.** Findings print the reason code and path/field only, as the existing gates do. Note 4.4's `string(JSON)` error text echoes the input document, so never print `${error_variable}` from a failed parse; print `json: not-json: <path>` only. [VERIFIED: probe: 4.4.3 error text includes the input, 3.25.3 does not]
- **`if(${var})` or `if(var)` on JSON-derived strings.** `if(n)` is false for the string `no`; compare with `STREQUAL` after `TYPE`. [VERIFIED: probe this session]
- **`foreach(x IN LISTS ...)` over JSON text.** A string value containing `;` splits into two items. Use index loops with `LENGTH`. [VERIFIED: probe: `x;y` expanded to 2 list items]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Stable test registration and proof that each control ran | A new runner or script | `gbl_add_test` plus the sorted inventories and `scripts/check-junit.sh` | Already enforces missing/extra/skipped; a control that is not in the inventory fails configure |
| "Gate must fail for reason X" | Ad hoc `execute_process` asserts | `gbl_expect_fail` / `gbl_expect_pass` | Asserts non-zero exit AND reason regex; has a meta-control |
| SHA-256 of a file | A shell `sha256sum`/`shasum` fallback ladder | `cmake -E sha256sum` via the `CMAKE` env var | Same tool as `scripts/lib/signature-common.sh`; the macOS and Linux tool names differ |
| 64-hex / 40-hex matching in CMake | `{64}` quantifiers | `string(REPEAT "[0-9a-f]" 64 HEX64)` | CMake regex has no `{n}` (probed: `"aaa" MATCHES "^a{3}$"` is false); precedent `cmake/CheckSignatureDigests.cmake:38` `string(REPEAT "[0-9a-f]" 64 HEX64)` |
| JSON writing for mutations | Text splicing | `string(JSON SET/REMOVE)` | Re-serializes keys sorted and 2-space indented, identical on 3.25.3 and 4.4.3; `SET` string values must be passed as quoted JSON (`"\"x\""`) |
| Atomic placement | Download straight into the destination | `mktemp -d` **inside** `.fixtures/`, then `mv` | Same filesystem makes `mv` a rename; verified in the prototype |
| Signal-safe cleanup | `trap '...' EXIT HUP INT TERM` | The Phase 1 idiom (`trap 'rm -rf "$tmp"' EXIT` plus `trap 'exit 129' HUP` and INT 130, TERM 143) | A combined trap lets a signalled script resume and print `ok`; `scripts/check-traps.sh` fails the build otherwise |

**Key insight:** every piece of this phase is "a validator that can fail". The cost of a vacuous pass is high (a green gate over an empty manifest), so each validator prints a count (`manifest: ok (0 rows)`), has a positive control that proves it accepts a real row, and has a negative control per reason code.

## Contract Design

### Closed vocabularies (`contracts/evidence.json`) `[REC]`
```json
{
  "schema": "gamebloke.evidence/1",
  "source_labels": ["REF", "SRC", "IMPL", "INF", "UNK"],
  "provenance_classes": ["project-authored", "hardware-observed", "reference-emulator",
                         "suite-self-checking", "disputed"],
  "result_labels": ["disputed"],
  "claim_fields": ["machine", "boot", "bios_provider", "cart", "input_timeline", "evidence"],
  "states": [
    {"name": "PASS",         "ran": "true",   "digest": "required", "reason": "optional"},
    {"name": "GUEST_FAIL",   "ran": "true",   "digest": "required", "reason": "optional"},
    {"name": "GUEST_STOP",   "ran": "true",   "digest": "required", "reason": "optional"},
    {"name": "HARNESS_FAIL", "ran": "either", "digest": "optional", "reason": "optional"},
    {"name": "TIMEOUT",      "ran": "true",   "digest": "required", "reason": "optional"},
    {"name": "CANCELLED",    "ran": "either", "digest": "optional", "reason": "optional"},
    {"name": "MISSING",      "ran": "false",  "digest": "optional", "reason": "required"},
    {"name": "SKIPPED",      "ran": "false",  "digest": "optional", "reason": "required"},
    {"name": "REFUSED",      "ran": "false",  "digest": "optional", "reason": "optional", "refusal": "required"}
  ]
}
```
The per-state rule table is what makes "a fixture that did not run is never PASS" a data rule: PASS has `ran: true` and `digest: required`; MISSING/SKIPPED/REFUSED have `ran: false`. The nine names equal the REQUIREMENTS.md EVID-05 list exactly. The contract validator pins the expected sets (nine names, five labels, five classes) as its independent oracle, the way the signature digests have an independent well-formedness check. The `ran` and `digest` values are strings (`"true"`, `"required"`) so a single `STREQUAL` handles them after `TYPE` STRING. Shipping exactly the locked rules (PASS needs ran plus digest; SKIPPED and MISSING need a reason; REFUSED needs code plus postcondition) and no more is recommended; making `reason` mandatory for GUEST_STOP belongs to Phase 4 when PROF-03 names the cause (see Open Question 3).

Note: ARCHITECTURE.md's runner schema lists eight statuses and omits GUEST_STOP. The locked nine-state list in CONTEXT/REQUIREMENTS supersedes it. [VERIFIED: .planning/research/ARCHITECTURE.md ~line 398 `status:    one of PASS | GUEST_FAIL | HARNESS_FAIL | TIMEOUT | CANCELLED | MISSING | SKIPPED | REFUSED`]

### Result JSON shape `[REC]`
Keeps status, semantic result, machine time, claim, and limits separate, consistent with ARCHITECTURE.md's runner result schema (`fixture`, `status`, `semantic`, `time`, `limits`, with the claim tuple replacing the looser `machine`/`input` blocks).
```json
{
  "schema": "gamebloke.result/1",
  "fixture": { "id": "arm-basic", "ran": true, "rom_sha256": "<64 lowercase hex or null>" },
  "status": "PASS",
  "reason": "text, required for SKIPPED and MISSING",
  "refusal": { "code": "...", "postcondition": "..." },
  "semantic": { "channel": "iwram-mailbox", "verdict": "pass", "passed": 3, "total": 3 },
  "time": { "cycles_total": 123456, "stop_reason": "verdict" },
  "claim": {
    "machine": "agb",
    "boot": "direct-start",
    "bios_provider": "none",
    "cart": { "kind": "rom-only", "rom_sha256": "<64hex or null>" },
    "input_timeline": { "kind": "none" },
    "evidence": [ { "fixture": "arm-basic", "provenance_class": "project-authored" } ]
  },
  "disputes": [],
  "labels": [],
  "limits": [ "what this result does not prove" ]
}
```
- `input_timeline` is `{"kind":"none"}` or `{"kind":"script","sha256":"<64hex>"}` (the CONTEXT text says "a script digest"; `kind: script` plus `sha256` is the recommended encoding).
- `fixture.ran` is the independent "did it run" fact. The validator rejects PASS with `ran` not true or `rom_sha256` not 64 lowercase hex (reason codes `pass-without-run`, `pass-without-digest`), and rejects any state whose `ran` value contradicts its table row (`ran-state-mismatch`).
- `claim.cart.rom_sha256` must equal `fixture.rom_sha256` (`claim-mismatch`); `null` is allowed only where the state's `digest` rule is `optional`.
- Dispute disclosure: `disputes` lists registry ids; if non-empty, `labels` must contain `disputed`; an evidence item whose `provenance_class` is `disputed` forces both. Unknown ids are rejected against the registry (`unknown-dispute`); `disputed` without ids is `label-without-dispute`.
- `limits` must be a non-empty array of non-empty strings (`empty-limits`). This is a cheap extension that enforces the project's core value ("every result says what it does not prove"); drop it if the planner wants strictly the locked rules.
- Phase 2 validates documents; it does not emit them. Phase 4 owns emission and adds `semantic`/`time` details. The validator should therefore check the presence and type of `semantic` and `time` only for the shape listed above and ignore unknown extra top-level keys. Unknown-key tolerance is `[REC]`; a stricter "no unknown keys" mode would couple Phase 2 to Phase 4's field growth.

### BIOS provider grammar (R1) `[VERIFIED: probed regexes]`
CMake regex (no `{n}`; anchors apply to the whole string, so an embedded or trailing newline cannot slip through: `none\n` and `x\nnone` did not match `^none$`):
```
none
project-stub
hle:<name>(,<name>)*              ^hle:[A-Za-z0-9_]+(,[A-Za-z0-9_]+)*$     rejects hle: , hle:a,,b , hle:a, , hle:,a , hle:a b
replacement:<name>@<commit>       ^replacement:[a-z0-9][a-z0-9._-]*@<40 lowercase hex>$
user-official@<digest>            ^user-official@<64 lowercase hex>$
```
`<name>` charsets and 40-hex/64-hex lengths are `[REC]` choices consistent with the project's digest policy (R1 gives only the shapes). SWI names are not checked against a table (locked). Ordering and duplicates inside `hle:` are not constrained (Open Question 4).

### Profile content (`profiles/agb.json`) `[REC]` shape, `[REF]` values
All constants below were read in GBATEK (no$gba extract, version string "3.06") in a local copy this session, and cross-checked by arithmetic. Cite section names only.
```json
{
  "schema": "gamebloke.profile/1",
  "profile_version": 1,
  "id": "agb",
  "machine": "agb",
  "boot": "direct-start",
  "bios_provider": "none",
  "cart": { "kind": "rom-only" },
  "timing": {
    "master_clock_hz":      { "value": 16777216, "source": "REF", "cite": "GBATEK, LCD Dimensions and Timings: System Clock" },
    "cycles_per_dot":       { "value": 4,        "source": "REF", "cite": "GBATEK, LCD Dimensions and Timings: Horizontal Dimensions" },
    "dots_visible":         { "value": 240, "source": "REF", "cite": "GBATEK, Horizontal Dimensions" },
    "dots_hblank":          { "value": 68,  "source": "REF", "cite": "GBATEK, Horizontal Dimensions" },
    "cycles_per_line":      { "value": 1232, "source": "REF", "cite": "GBATEK, Horizontal Dimensions" },
    "lines_visible":        { "value": 160, "source": "REF", "cite": "GBATEK, Vertical Dimensions" },
    "lines_vblank":         { "value": 68,  "source": "REF", "cite": "GBATEK, Vertical Dimensions" },
    "lines_per_frame":      { "value": 228, "source": "REF", "cite": "GBATEK, Vertical Dimensions" },
    "cycles_per_frame":     { "value": 280896, "source": "REF", "cite": "GBATEK, Vertical Dimensions" },
    "hblank_flag_edge_cycle": { "value": null, "source": "UNK", "dispute": "disp-hblank-flag-edge" }
  },
  "unmodeled": [
    { "id": "sp-micro-differences", "status": "unmodeled", "reason": "...", "details": ["..."] },
    { "id": "prefetch", "status": "unmodeled", "reason": "..." },
    { "id": "hblank-flag-edge", "status": "unmodeled", "reason": "...", "dispute": "disp-hblank-flag-edge" },
    { "id": "ppu" }, { "id": "audio" }, { "id": "dma" }, { "id": "timers" },
    { "id": "irq-controller" }, { "id": "backup-media" }
  ]
}
```
(each unmodeled entry carries `status`, `reason`; abbreviated above.)
- The hard-coded 960 trap: 960 cycles is the **visible drawing time** (240 dots times 4), not the HBlank flag edge. The profile has no numeric field that could be read as the flag edge; `hblank_flag_edge_cycle` is `null` with source `UNK`, pointing to the dispute entry. A validator rule (`hblank-edge-hardcoded`) rejects any non-null value there.
- Invariants the validator can check, so the profile cannot silently drift: `dots_visible + dots_hblank` equals `dots_per_line` (308), `dots_per_line * cycles_per_dot` equals `cycles_per_line` (1232), `lines_visible + lines_vblank` equals `lines_per_frame` (228), `cycles_per_line * lines_per_frame` equals `cycles_per_frame` (280896), and `master_clock_hz` equals 2^24 (16777216). All use `math(EXPR)` (64-bit signed, fine for these magnitudes). Reason code `timing-arithmetic`. [VERIFIED: 308 x 4 = 1232; 1232 x 228 = 280,896; 197,120 + 83,776 = 280,896, matching GBATEK's vertical table]
- Keep numeric fields as JSON numbers and digests as strings, never numbers: large integers survive `GET` verbatim in both versions, but a digest as a number would lose leading zeros.
- `bios_provider` in the profile is the profile's default (`none` for v1.0), validated with the R1 grammar.
- `calibrated:<fixture>` and any other string outside the five labels is rejected for now (`bad-source-label`).

### Dispute registry (`registry/disputes.json`) `[REC]` shape
Entry: `id`, `summary`, `sources[]` each `{label, ref, note}` (label from the five), `status` (`open` only), `affects[]`. Seed ids (stable once chosen): `disp-dma-latch`, `disp-gbatek-errata`, `disp-hblank-flag-edge`, `disp-mgba-suite-22`, `disp-mgba-suite-24`, `disp-mgba-suite-27`. The seed set is pinned in the validator (`seed-missing`).

**Verified seed content (see "Authoritative GBA facts" for sources and what changed from earlier research).**

### Manifest row shape `[REC]` (field names are discretionary; must cover every EVID-01 field)
```json
{
  "schema": "gamebloke.manifest/1",
  "fixtures": [ {
    "id": "example-id",
    "source":  { "kind": "git", "url": "https://...", "commit": "<40 lowercase hex>", "tag": "optional extra" },
    "license": { "spdx": "MIT", "notices": ["..."], "redistributable": "yes|no|unknown" },
    "build":   { "recipe": "prebuilt-upstream | <command or path>",
                 "toolchain": { "identity": "...", "sha256": "<64hex or null>" } },
    "bios_requirement": "none",
    "expected": { "provenance_class": "suite-self-checking", "dispute": null },
    "permitted_use": ["local-only"],
    "fetch": { "pin_dest": "jsmolka/arm.gba" }
  } ]
}
```
- `source.kind` closed set `git | url | in-tree`. `git` requires 40-lowercase-hex `commit` (branch names, `HEAD`, `main`, short SHAs, tag-only all rejected as `unpinned-revision`; `tag` is allowed only as an extra field). `url` requires an `https` URL and 64-hex `sha256`. `in-tree` requires a repository-relative `path` (no `..`, no leading `/`) and `sha256`. This matches "Git sources must pin a 40-hex commit... Non-git sources must pin a SHA-256."
- `bios_requirement` closed set mirroring the R1 provider kinds without parameters: `none | project-stub | hle | replacement | user-official` (EV-2: BIOS-dependent fixtures must be declared so a missing BIOS reports SKIPPED, never PASS).
- `permitted_use` closed set `public-ci | local-only | differential-only`, non-empty array `[REC]`; `redistributable` separate from digest identity ("a matching hash proves identity, not redistribution rights").
- `toolchain.sha256` may be `null` only together with a non-empty `identity` such as `prebuilt-upstream`, so third-party prebuilt ROMs (jsmolka, Phase 6) can be described honestly. Required: field present with non-empty identity.
- Cross-references the linter should check: `expected.dispute` (if set) must exist in the registry and force `provenance_class` `disputed`, and a `disputed` class requires a dispute id; if `fetch.pin_dest` is set, `pins.txt` must contain that dest, with the same `sha256` for `url` rows and for `git` rows a URL that contains the 40-hex commit (so a git row cannot point at a branch URL). Reason codes `dispute-unknown`, `dispute-class-mismatch`, `pin-mismatch`.
- In this phase `tests/fixtures/manifest.json` holds `"fixtures": []` (no fixture exists until Phase 5). The linter prints `manifest: ok (0 rows)`; the positive control builds a one-row complete manifest in a sandbox and asserts `manifest: ok (1 rows)`; a missing `fixtures` key is `missing-field`. A zero-row real manifest is the honest state, not a vacuous one, because the controls prove the gate rejects and accepts rows.

### `pins.txt`
Rows `<sha256> <url> <dest>`, `#` comments and blank lines ignored, exactly three whitespace-separated fields, digest 64 lowercase hex, `https://` URLs only in the committed file, `<dest>` charset `[A-Za-z0-9._/-]`, no `.` or `..` component, no leading `/`, unique. The committed file has only a header comment in this phase. `file://` is a test-only scheme accepted by `fetch-pin.sh` only when `GBL_FETCH_ALLOW_FILE=1`; the committed-file linter rejects it (`pins: bad-url`).

## Authoritative GBA facts (profile and registry sources)

| Fact | Value | Source | Tag |
|------|-------|--------|-----|
| Master clock | 16,777,216 Hz (the text gives `16*1024*1024 Hz`, about 59.59 ns per cycle) | GBATEK, LCD Dimensions and Timings, "System Clock" | `[REF]` `[VERIFIED: local copy of problemkaputt.de/gbatek.htm, 5,032,449 bytes, 2026-11-10]` |
| Dot time and line | 4 cycles per dot; 240 visible dots = 960 cycles; 68 HBlank dots = 272 cycles; 308 dots = 1,232 cycles per scanline | GBATEK, LCD Dimensions and Timings, "Horizontal Dimensions" | `[REF]` `[VERIFIED]` |
| Frame | 160 visible + 68 VBlank = 228 lines; 197,120 + 83,776 = 280,896 cycles | GBATEK, LCD Dimensions and Timings, "Vertical Dimensions" | `[REF]` `[VERIFIED]` |
| Printed rates | The page prints ca. 59.737 Hz and ca. 13.620 kHz; actual is 16,777,216/280,896 = 59.7275... Hz | arithmetic; PITFALLS and SUMMARY already note the error | `[INF]` |
| HBlank flag | The "LCD I/O Interrupts and Status" section says the flag is 0 for a total of 1,006 cycles although drawing takes 960 | GBATEK | `[REF]` `[VERIFIED]` |
| Internal Memory Control | I/O address 0x04000800, 32-bit, "Undocumented - Internal Memory Control (R/W)", mirrored every 64 KiB across the I/O area; bits 24-27 are the 256K WRAM wait control; default value bits 24-27 select 2 wait states; the fastest setting works on GBA and GBA SP only and locks up the GBA Micro; "NOT supported by DS" | GBATEK, section "4000800h - 32bit - Undocumented - Internal Memory Control (R/W)" (also the I/O map entry) | `[REF]` `[VERIFIED]` |
| Micro | "Supports 32bit GBA games only" and 256K main RAM "cannot be overclocked via port 4000800h" | GBATEK, "GBA SP Notes" / "Gameboy Micro (GBA Micro)" notes | `[REF]` `[VERIFIED]` |
| Micro cartridge switch | "The GBA micro does lack the cartridge switch" (cannot detect 8-bit carts, cannot switch voltage) | GBATEK, "GBA Backwards Compatibility CGB Mode", subsection "GBA, GBA SP, and GBA micro" | `[REF]` `[VERIFIED]` |
| SP compatibility | GBATEK says the SP "appears to be 100% compatible" and cannot be detected by software; this is a community claim, not a measurement | GBATEK, "GBA SP Notes" | `[REF]` |
| SP model labels | AGS-001 front-lit (original SP, 2003), AGS-101 backlit (2005); model number printed on a label on the bottom | en.wikipedia.org Game Boy Advance SP; consolemods.org GBA Model Differences | `[CITED]` MEDIUM; secondary, community |
| Other model labels | AGB-001 (original), OXY-001 (Micro) | consolemods.org / nindb.net via search; not stated in the Wikipedia pages fetched | `[CITED]` MEDIUM, `[ASSUMED]` that these are the exact labels to print |

**SP and AGS naming for `docs/profile-agb.md` (R8 position, unchanged):** the profile id is `agb`. The SP is `agb` plus a host presentation preset and is claimed only with evidence; the AGS-001 versus AGS-101 difference is a display (front light versus backlight) difference the machine cannot detect per GBATEK, so it is a host preset, not a profile. `agb-micro` is not offered. Micro-visible differences are listed under the unmodeled item `sp-micro-differences` with two sub-facts: the 0x04000800 EWRAM-wait register behavior (the Micro locks up at the fastest setting) and the missing cartridge switch. The profile must not state "SP is identical to AGB" as a fact; it states the GBATEK claim with its `[REF]` label.

### Dispute-registry seed verification

| Seed | What it actually is | Primary evidence this session | Notes |
|------|--------------------|-------------------------------|-------|
| DMA latch | NanoBoyAdvance's hardware test `dma/latch` asserts each DMA channel has a separate latch and that 16-bit DMAs duplicate the value into both halves; mGBA 0.10.5 keeps one shared `dmaTransferRegister` used by every channel; GBATEK (the local 3.06 text) has no statement about a DMA latch | hw-test `dma/latch/source/main.c` on Codeberg (master at `fbc99140e06f`, last commit 2026-08-10); mGBA `src/gba/dma.c` at tag 0.10.5 (lines 273-297 use `memory->dmaTransferRegister`); GBATEK has no DMA-latch statement: every "latch" hit in the page is unrelated (serial, flash, Wi-Fi, etc.) | `[IMPL]` x2, `[REF]` silent. The hw-test comment does not state hardware provenance. Both are code observations, not hardware facts. NBA (GPL) and mGBA (MPL) were read for behavior only |
| GBATEK errata | The mGBA forum master post "Game Boy Advance Documentation" by endrift lists GBATEK errata: BLDALPHA readable but BLDY not; sprite semi-transparency overrides BLDCNT brighten/darken only when it blends with a target; WIN wrapping not described properly; audio registers when disabled via SNDCNT_X; JOY RECV/SEND descriptions sometimes reversed; PWM rumble; BLDCNT/BLDALPHA/BLDY not latched per scanline; DMA address registers reject invalid addresses; DMA3 SRAM-to-ROM hard-lock; DMA1/DMA2 can read SRAM and ROM; DMA0 reads zeroes from SRAM (flashcart doubt noted by the author) | forums.mgba.io showthread tid=343; post dated 2015-11-13, last edited 2017-10-13 | `[SRC]` only for "the list exists". Hardware claims inside are `[UNK]`. List last edited 2017 and may be stale against current GBATEK (not re-checked against the current text) |
| HBlank flag edge | GBATEK: HBlank flag is 0 for 1,006 cycles. NBA: `scheduler.Add(1007, ..._hblank_vdraw)` at 1,007 cycles (lines 131 and 173 for the draw-line cases) in `src/nba/src/hw/ppu/ppu.cc` at NBA master `2da64afb5863`. mGBA 0.10.5: `VIDEO_HDRAW_LENGTH = 1008`, `VIDEO_HBLANK_LENGTH = 224`, `VIDEO_HORIZONTAL_LENGTH = 1232` in `include/mgba/internal/gba/video.h` | the three sources named, read this session | GBATEK `[REF]`; NBA and mGBA `[IMPL]`. That mGBA sets the flag at 1,008 is inferred from the constant `[INF]` (the constant was read, the flag-set call site was not) |
| mGBA suite #22 | Title: "Memory out-of-bounds load failing on real HW". State open. Reporter used a GBA SP (AGS-001) with an Omega flash cartridge and suite r88; Memory tests 1514/1552; a later comment reports three unmodified AGS-001 consoles on an EverDrive mini with r76 giving Memory 1514/1552, DMA 1244/1256, Misc. Edge Case 7/10 | `gh api repos/mgba-emu/suite/issues/22` (created 2025-02-22, comment 2025-07-22) | matches the research. Flash-cart contamination (CART-7) applies; hardware claim is `[UNK]` |
| mGBA suite #24 | Title: "HBlank timing tests not working on hardware (GBP)". State open. Game Boy Player with a dev cart; most HBlank timing tests fail; no comments | `gh api repos/mgba-emu/suite/issues/24` (created 2026-06-17) | **The research paraphrase said "GBP with dev cart" correctly, but the title in the tracker names GBP (Game Boy Player), not an SP.** A GBP is a GameCube accessory, a different machine from the AGB |
| mGBA suite #27 | Title: "Suite-latest based on e694203 has wrong misc hblank expected values, Layer Toggle 2 Actual is wrong on hardware, Window offscreen reset is broken on Actual and Expected." State open; body says it was confirmed only on open_agb_firm for the latter | `gh api repos/mgba-emu/suite/issues/27` (created 2026-08-15); commit `e694203` resolves to `e6942030d25f`, 2026-07-09, "Fix video test display bug on cleanup" | the PITFALLS quote is accurate. open_agb_firm is a 3DS GBA-mode environment, not a retail GBA, so the hardware claim is weaker than the title suggests `[UNK]` |

The mGBA 0.10.5 release timestamp is 2025-03-09T03:59:39Z, resolving the 03-08 versus 03-09 discrepancy in earlier research. [VERIFIED: `gh api repos/mgba-emu/mgba/releases/tags/0.10.5`]

### Findings that differ from earlier research
1. **#24 is a Game Boy Player report**, not a retail AGB or SP. #22 is an SP (AGS-001) with flash carts. #27's hardware claim was checked on open_agb_firm. The registry entries should record the machine and cartridge for each report, because the machine and cart are the contamination the entries exist to expose.
2. **The mGBA forum errata list has twelve items, not five.** SUMMARY.md named five (sprite semi-transparency, WIN wrapping, DMA3 SRAM-to-ROM, JOY direction, PWM rumble); the post also lists BLDALPHA/BLDY readability, audio-disabled register behavior, per-scanline latching of BLD registers, DMA address rejection, DMA1/DMA2 SRAM/ROM reads, and DMA0 SRAM zeroes. The post is dated 2015-11-13 and was last edited 2017-10-13.
3. **GBATEK now covers GBA, NDS, DSi, and 3DS in one 5 MB file** and states it is "Extracted from no$gba version 3.06". "GBATEK 3.06" in the roadmap is that extract's version label; there is no separate GBATEK version number on the page. Cite section names, not line numbers (line numbers in this local copy are not stable).
4. **GBATEK says the Internal Memory Control register exists on GBA, GBA SP, and GBA Micro** (Micro locks up at the fastest wait setting) and is not supported by DS in GBA mode. That matches R8; the text additionally says bit 3 disables the CGB bootrom, which is relevant to the unmodeled list but outside Phase 2's scope.

## Common Pitfalls

### Pitfall 1: `string(JSON)` accepts malformed JSON
**What goes wrong:** Trailing commas, `//` and `/* */` comments, content after the root (`{"a":1}{"b":2}`), leading-zero numbers, raw tabs in strings, and duplicate keys (last wins) all parse without error. [VERIFIED: probe on 3.25.3 and 4.4.3, identical]
**Why:** the bundled parser is permissive.
**How to avoid:** the strictness guard in `cmake/GblJson.cmake`; reason codes `json: comment`, `trailing-comma`, `trailing-content`, `duplicate-key`; a control for each.
**Warning signs:** a validator passes a file that `python -m json.tool` or any strict parser rejects.

### Pitfall 2: `foreach(i RANGE ${last})` with `last` -1 runs twice
**What goes wrong:** for an empty array, `math(EXPR last "0-1")` gives `-1` and `foreach(i RANGE -1)` iterates `i=0` then `i=-1` on both versions; the body then calls `string(JSON GET ... 0)` on an empty array. [VERIFIED: probe this session]
**How to avoid:** guard every index loop with `LENGTH GREATER 0`. The sibling `~/projects/gabbaboy/cmake/VerifyFixtureManifest.cmake` avoids it by failing on an empty array; this project's real manifest is legitimately empty, so the guard must be explicit.

### Pitfall 3: `GET` conflates empty string, null, and a missing value, and booleans become `ON`/`OFF`
**What goes wrong:** `""` and `null` both `GET` as an empty string; a required string field set to `""` passes a naive presence test. `if(var)` on the string `no` is false.
**How to avoid:** `TYPE` first (`STRING`/`NUMBER`/`BOOLEAN`/`NULL`/`ARRAY`/`OBJECT`), then `GET`, then `STREQUAL` or regex. `missing-field` means absent, null, or empty. [VERIFIED: probe this session]

### Pitfall 4: `REMOVE` of a missing member succeeds silently
**What goes wrong:** a mutation control that removes a misspelled key tests the unmutated document and "passes" or fails for the wrong reason. [VERIFIED: probe: `REMOVE` of absent `nokey` returned the document unchanged, `er=NOTFOUND`, both versions]
**How to avoid:** every mutation helper asserts the document changed (`string(JSON EQUAL <before> <after>)` false), failing with `mutate: no-match` like the existing helpers.

### Pitfall 5: Script-mode policies differ between 3.25 and 4.4
**What goes wrong:** with no `cmake_minimum_required`, `cmake -P` on 3.25.3 leaves CMP0054, CMP0057, CMP0012, CMP0007, CMP0121, CMP0124, CMP0140, and CMP0053 unset (OLD behavior; `IN_LIST` then warns), while on 4.4.3 CMP0054, CMP0057, CMP0012, CMP0007, and CMP0053 are already NEW. A green run on a modern local CMake proves nothing about the floor. [VERIFIED: probe this session]
**How to avoid:** every entry script and every `.cmake` library file starts with `cmake_minimum_required(VERSION 3.25)`, and the whole suite is run once on the Kitware 3.25.3 tarball (the floor check in `docs/verify.md` "Slow-lane checks").

### Pitfall 6: `message(FATAL_ERROR)` wraps text
**What goes wrong:** the reason code in a long fatal message can straddle a line break, so `gbl_expect_fail REGEX` misses it. [VERIFIED: probe: 78-column wrapping and indentation]
**How to avoid:** Pattern 3 (plain `message()` per finding, short fatal at the end), reason code first on every line.

### Pitfall 7: `GET_RAW` and a documented signature that is not the real one
**What goes wrong:** `string(JSON ... GET_RAW ...)` exists only from CMake 4.3; on 3.25.3 it fails with `got an invalid mode 'GET_RAW', expected one of GET, TYPE, MEMBER, LENGTH, REMOVE, SET,  EQUAL`. Conversely the 3.25 documentation shows `GET` and `TYPE` requiring a path, yet both accept no path on 3.25.3 and return the whole document or its type. [VERIFIED: probe this session]
**How to avoid:** use only `GET TYPE MEMBER LENGTH REMOVE SET EQUAL`; the floor run catches accidental newer subcommands.

### Pitfall 8: Error text and `${err}` leak document content on 4.x
**What goes wrong:** 4.4.3's parse error includes the input document; 3.25.3's does not. Printing `${err}` in a reason line echoes file content (the project rule is "findings print path and reason codes only").
**How to avoid:** print `json: not-json: <path>` and nothing from the error variable.

### Pitfall 9: GitHub archive tarball hashes are not stable
**What goes wrong:** a pin of `https://github.com/<o>/<r>/archive/<commit>.tar.gz` can break when GitHub's archive bytes change (the 2023 `git archive` compression change). GitHub guarantees stability only for assets a maintainer uploads. [CITED: github.blog/changelog/2023-01-30-git-archive-checksums-may-change/]
**How to avoid (Phase 6 note):** pin single files at a commit via `https://raw.githubusercontent.com/<owner>/<repo>/<40hex>/<path>` (what `~/projects/cogmaster/pins.txt` does for MAME `hash/*.xml` and emu2413) or a release asset. The row-level cross-check `pin-mismatch` (git row URL must contain the commit) enforces this.

### Pitfall 10: Atomic placement needs the temporary directory on the destination filesystem
**What goes wrong:** `mktemp -d` under `/tmp` then `mv` to `.fixtures/` is copy-and-delete across filesystems (not atomic; a crash can leave a partial file at the destination).
**How to avoid:** `tmp=$(mktemp -d "$root/.fetch.XXXXXX")` inside `.fixtures/`. Verified in the prototype: after a digest mismatch the destination did not exist and the temp directory was gone. `.fixtures/` is already ignored once added to `.gitignore`.

### Pitfall 11: Symlink components under `.fixtures/`
**What goes wrong:** a pre-existing `.fixtures/sub -> ../elsewhere` makes `mv` write outside the root even with a clean `<dest>`; `mv` onto an existing directory moves *into* it.
**How to avoid:** after `mkdir -p`, compare `cd "$parent" && pwd -P` against `cd "$root" && pwd -P`, refuse an existing symlink or directory at the destination. Verified in the prototype for both cases (`fetch-pin: symlink`).

### Pitfall 12: New documentation tripping the hygiene gate
**What goes wrong:** the email rule (`GBL_HY_EMAIL` in `scripts/hygiene.sh`) matches any word, an at-sign, a dotted host name, and an alphabetic top-level label, so a file-name-plus-at-sign-plus-ref phrase can look like an address (this very file was flagged once for writing such an example); the home-path rule matches the two common home-directory prefixes followed by a user name; the credential-assignment rule matches a secret-like key name followed by a colon or equals sign and 16 or more token characters. [VERIFIED: scripts/hygiene.sh:30-34 patterns read; this file's first draft failed `hygiene.sh --tree` at the line that spelled an example out]
**How to avoid:** in docs and JSON write `at` or use `name@<commit>` placeholders, never absolute home paths, and keep examples free of secret-shaped assignments. Run `sh scripts/hygiene.sh --tree` and `sh scripts/signature-scan.sh --tree` before each plan's last commit. A 156-byte-window hash is taken for every file of at least 160 bytes; JSON and Markdown are fine (text, not on the BIOS size, and no `.gba/.bin/.rom/.sav` extensions). [VERIFIED: scripts/lib/signature-common.sh:59-67]

### Pitfall 13: A fetch script that works only in bash
**What goes wrong:** macOS `/bin/sh` is bash 3.2 in POSIX mode and hides non-POSIX constructs.
**How to avoid:** run the new script under `dash` as well (`/bin/dash` is present here); the prototype passed under dash and `/bin/sh`, is `shellcheck -s sh` clean, and passes `scripts/check-traps.sh`.

### Pitfall 14: Curl absent in the Linux-lane container
**What goes wrong:** `scripts/linux-lane.sh` installs `ca-certificates cmake ninja-build git libc6-dev` and no `curl`, so the `fetch-pin` file:// controls would fail there. GitHub-hosted runners include curl (the Ubuntu 24.04 image list shows curl 8.5.0; the 26.04 list was not checked). [VERIFIED: scripts/linux-lane.sh:61; CITED: runner-images Ubuntu2404-Readme]
**How to avoid:** add `curl` to that apt line; `fetch-pin.sh` exits 2 with `fetch-pin: no-curl` if absent so the gap is loud, never a skip.

## Reason-code catalogue (stable names proposed `[REC]`)

Format of every finding: `<family>: <code>: <path-or-field>` (never the value). Each code gets one ctest control named `<family>.nc.<code with underscores>`; each validator gets `<family>.pos.<case>` positives; real-file gates are `<family>.lint` (or the names fixed below). Where a code covers several shapes, the case driver loops through the variants inside the one control (for example `unpinned-revision` runs `main`, `HEAD`, a 7-hex short SHA, and a tag-only value).

| Family | Codes | Variants worth looping | New ctests |
|--------|-------|------------------------|------------|
| `json` (loader, driven by a tiny `-DFILE=` entry) | `unreadable`, `not-json`, `not-object`, `comment`, `trailing-comma`, `trailing-content`, `duplicate-key` | duplicate at top level and nested; `//` and `/* */` | 7 nc + 1 pos = 8 |
| `contract` (vocabulary file) | `bad-schema`, `state-set`, `bad-state-rule`, `label-set`, `class-set` | drop a state; add a tenth; unknown `digest` value | 5 nc + 1 pos + real `result.contract` = 7 |
| `registry` | `bad-schema`, `missing-field`, `duplicate-id`, `bad-id`, `bad-status`, `bad-source-label`, `no-sources`, `seed-missing` | remove each seed id in turn | 8 nc + 1 pos + real `registry.lint` = 10 |
| `profile` | `bad-schema`, `bad-version`, `missing-field`, `bad-machine`, `bad-boot`, `bad-bios-provider`, `bad-cart`, `missing-source`, `bad-source-label` (including `calibrated:x`), `timing-arithmetic`, `hblank-edge-hardcoded`, `unmodeled-missing`, `unmodeled-bad-entry`, `dispute-unknown`, `doc-missing-id`, `doc-extra-id`, `doc-version-mismatch` | remove each required top-level field and each required unmodeled id in turn | 17 nc + 1 pos + real `profile.lint` = 19 |
| `manifest` | `bad-schema`, `missing-field` (loop all EVID-01 fields), `wrong-type`, `duplicate-id`, `bad-id`, `bad-source-kind`, `unpinned-revision`, `missing-digest`, `bad-provenance-class`, `bad-bios-requirement`, `bad-permitted-use`, `dispute-unknown`, `dispute-class-mismatch`, `pin-mismatch`, `bad-path` | each EVID-01 field removed, nulled, and emptied | 15 nc + 2 pos (complete row, empty array) + real `manifest.lint` = 18 |
| `pins` (lint of the committed file) | `bad-row`, `bad-digest`, `bad-url`, `bad-dest`, `duplicate-dest` | `http://`, `file://`, whitespace in URL; `..`, absolute, bad charset | 5 nc + 1 pos = 6 (real file covered by `manifest.lint -DMODE=all`) |
| `fetch-pin` (shell, exit 2 for usage and environment errors, 1 otherwise) | `usage`, `missing-pins`, `no-row`, `bad-row`, `bad-digest`, `bad-url`, `bad-dest`, `no-curl`, `download-failed`, `digest-mismatch`, `symlink` | `bad-url`: `http://`, unlisted scheme, `file://` without the env var; `bad-dest`: traversal, absolute, charset | 11 nc + 2 pos (fetch ok; second fetch is idempotent) + `fetch.ignored` and `fetch.no_tracked` = 15 |
| `result` | `bad-schema`, `missing-field`, `unknown-status`, `pass-without-run`, `pass-without-digest`, `ran-state-mismatch`, `missing-reason`, `missing-refusal`, `bad-machine`, `bad-boot`, `bad-bios-provider`, `bad-cart`, `bad-input-timeline`, `bad-evidence`, `unknown-fixture`, `unknown-dispute`, `undisclosed-dispute`, `label-without-dispute`, `claim-mismatch`, `empty-limits` | `bad-bios-provider`: `hle:` (empty list), `hle:a,,b`, `replacement:x@main`, `user-official@short`, `m0`, uppercase `NONE`; `missing-refusal`: no code, no postcondition | 20 nc + 4 pos (PASS, SKIPPED with reason, REFUSED, disputed) = 24 |

Total roughly 105 new tests per lane; consolidating `bad-machine`/`bad-boot`, the two `missing-refusal` shapes, and the loader's three syntax codes would cut it to about 90. All run in a few seconds each at most (the existing 52-test hygiene lane takes about 5 seconds at `-j4`). The `fetch-pin` `usage` code needs a check that exit status is 2 (a documented code, so `EXIT 2` is allowed by `gbl_expect_fail`); the other codes assert only non-zero plus the reason.

Registration names fixed by earlier phases or CONTEXT: `manifest.lint` (labels `gate manifest`), registered in `tests/gates.cmake`, running `cmake -DMODE=all -P cmake/CheckManifest.cmake` over the real `tests/fixtures/manifest.json`, `pins.txt`, and `registry/disputes.json`.

## Code Examples

### `scripts/fetch-pin.sh` (verify-before-place) `[VERIFIED: prototype run under dash and /bin/sh]`
```sh
#!/bin/sh
# fetch-pin.sh [--pins <file>] [--root <dir>] <dest>
set -eu
set -f                      # no globbing while splitting row fields
LC_ALL=C; export LC_ALL

die() { printf 'fetch-pin: %s: %s\n' "$1" "$2" >&2; exit "${3:-1}"; }
# ... parse options; default pins="$here/pins.txt", root="$here/.fixtures"

check_dest() {              # allow-list charset, no empty/./.. component, no leading /
  case $1 in '' | /* | */ | *//*) return 1 ;; *[!A-Za-z0-9._/-]*) return 1 ;; esac
  oldifs=$IFS; IFS=/
  for part in $1; do case $part in '' | . | ..) IFS=$oldifs; return 1 ;; esac; done
  IFS=$oldifs; return 0
}
# ... select the row whose 3rd field equals <dest>; require exactly 3 fields; digest ^[0-9a-f]{64}$
case $url in
  https://*) proto='=https' ;;
  file://*)  [ "${GBL_FETCH_ALLOW_FILE:-}" = 1 ] || die bad-url "file:// refused"; proto='=file' ;;
  *)         die bad-url "scheme not allowed" ;;
esac
case $url in *[[:space:]\'\"\\]*) die bad-url "unsafe character" ;; esac   # URL begins with a scheme, never with '-'
command -v curl >/dev/null 2>&1 || die no-curl "curl not found" 2

mkdir -p "$root"
tmp=$(mktemp -d "$root/.fetch.XXXXXX")       # same filesystem as the destination
trap 'rm -rf "$tmp"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

curl --fail --silent --show-error --globoff --proto "$proto" --proto-redir "$proto" \
  --max-redirs 5 --connect-timeout 20 --max-time 600 --max-filesize 268435456 \
  --output "$tmp/payload" --url "$url" || die download-failed "$want_dest"
got=$("${CMAKE:-cmake}" -E sha256sum "$tmp/payload"); got=${got%% *}
[ "$got" = "$digest" ] || die digest-mismatch "$want_dest: pinned $digest, got $got"
# ... mkdir -p parent; compare pwd -P of parent and root (symlink escape); refuse existing link/dir
mv -f "$tmp/payload" "$root/$want_dest"
```
Probe results (prototype, file:// URLs, synthesized bytes): ok fetch placed identical bytes; mismatch exited 1 with `fetch-pin: digest-mismatch: ...` and left no destination and no temp directory; `../evil.dat` refused `bad-dest`; `file://` refused `bad-url` without the env var; symlinked parent refused `symlink`; directory at destination refused. `scripts/check-traps.sh` reported `traps: ok`. [VERIFIED: ran this session]
Curl facts verified on 8.7.1: `--proto '=https'` rejects `http://` and `file://` (`Protocol "file" disabled`, `Protocol "http" disabled`, exit 1); `--proto '=file'` copies a local file; a missing file exits 37; `--fail` writes nothing on an HTTP error; `--remove-on-error`, `--max-filesize`, `--max-redirs`, `--proto-redir`, `--connect-timeout`, and `--retry` exist. [VERIFIED: probe this session] No archive extraction in this phase: `fetch-pin.sh` places the verified file as is (extraction would add decompression-bomb and traversal surface and belongs to the phase that needs it).

### Control driver skeleton (style of `tests/gates/signature_case.cmake`)
```cmake
cmake_minimum_required(VERSION 3.25)
foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "profile-case: -D${var}= is required")
  endif()
endforeach()
include("${SRC}/cmake/ExpectFail.cmake")
set(dir "${BIN}/nc/profile_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(COPY "${SRC}/profiles/agb.json" "${SRC}/docs/profile-agb.md" "${SRC}/registry/disputes.json"
     "${SRC}/contracts/evidence.json" DESTINATION "${dir}/src")   # then mutate the copy
# gbl_mutate_remove(<file> <path>...) asserts string(JSON EQUAL) is false, else "mutate: no-match"
gbl_expect_fail(COMMAND "${CMAKE_COMMAND}" "-DFILE=${dir}/src/agb.json" ... -P "${SRC}/cmake/CheckProfile.cmake"
                REGEX "profile: unmodeled-missing: prefetch")
```
Registration (one line per code, mirroring `signature.nc.*`):
```cmake
foreach(case IN ITEMS bad_schema bad_version missing_field ...)
  gbl_add_test(NAME profile.nc.${case} LABELS control profile
    COMMAND "${CMAKE_COMMAND}" -DCASE=${case} "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
            -P "${CMAKE_SOURCE_DIR}/tests/gates/profile_case.cmake")
endforeach()
```

### Doc-sync check (ledger-style; sibling pattern)
`docs/profile-agb.md` has a `## Unmodeled` heading whose table rows carry the entry id as the first backticked token, and a line `profile_version: N`. The check parses that section only (heading-delimited, as `~/projects/gabbaboy/cmake/VerifyEvidenceLedger.cmake` does for its `## Claim table`): every JSON `unmodeled[].id` must appear (`doc-missing-id`), every id in the doc must exist in the JSON (`doc-extra-id`), and the documented version must equal `profile_version` (`doc-version-mismatch`). An empty parsed section is a failure, not a pass (the sibling's `evidence-ledger-empty`).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `string(JSON)` only (3.19+) | Same subcommands at 3.25 and 4.4; `GET_RAW` added in 4.3 | CMake 4.3 | Do not use `GET_RAW`; the floor must stay on the common subset |
| Trusting emulator agreement as an oracle | Provenance class per expected value plus a dispute registry | PITFALLS CP-5 | The five classes and the registry are the project's answer; classes are closed |
| Sibling `fetch-pin.sh` (cogmaster): unpack straight into DEST after hash, `curl -fsSL`, no scheme or path checks, combined cleanup trap absent of signal handlers | Verify in a temp dir on the destination filesystem, https-only protocol restriction, allow-listed `<dest>`, symlink check, Phase 1 signal-trap idiom | this phase | Not copied: cogmaster's SHA-256 ladder (`sha256sum` else `shasum`), its four-field row (`name version url sha256`), and its unpack step |

**Sibling lessons recorded (engineering only; other hardware and other gates are not transferred):**
- `~/projects/gabbaboy/cmake/VerifyFixtureManifest.cmake` (30 lines): fails on an empty or missing `fixtures[]` so a broken manifest cannot pass vacuously; checks size and SHA-256 of each referenced file; failure tokens `fixture-manifest-empty`, `fixture-digest-mismatch`. Keep the vacuity idea (print a row count, prove acceptance with a positive control); do not copy file-size checks (no fixture bytes are tracked here).
- `~/projects/gabbaboy/cmake/FixtureAdmissionPolicy.json`: a release-product allow-list keyed by file with per-destination digests (Game Boy products). Not applicable; note only that digests are strings.
- `~/projects/gabbaboy/cmake/VerifyEvidenceLedger.cmake`: heading-delimited table parse with a `-empty` failure and named tokens. Reuse the idea for the doc-sync check.
- `~/projects/cogmaster/pins.txt` and `scripts/fetch-pin.sh`: one row per download, hash checked before use, mismatch exits 1 with both digests printed, bump policy documented in the file header, per-file raw URLs at a commit. Reuse: loud mismatch message with both digests; per-file commit-addressed URLs; a short bump-policy header. Do not reuse: the four-field row, `set -f` plus unquoted splitting without field-count checks, the unconditional `tar` extraction.
- `~/projects/cogmaster/ASSET_POLICY.md` section 3 ("Built or fetched in CI, hashes committed"): fetch at pins, commit digests, keep built files in the build directory; `PROVENANCE.md` rule that licence is read at the exact commit. Consistent with the locked G3 default here.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The GitHub-hosted `ubuntu-26.04` image includes curl (verified only for the 24.04 image list) | Environment, Pitfall 14 | `fetch.*` controls fail in the `hygiene` job; mitigation is a one-line `apt-get install curl` in the workflow or relying on the loud `no-curl` code |
| A2 | AGB-001 (original) and OXY-001 (Micro) are the exact model labels to print; confirmed only by community/search sources, not by the Wikipedia pages | Authoritative GBA facts | A wrong label in `docs/profile-agb.md`; low impact, cite as community labels or omit |
| A3 | The mGBA forum errata list (edited 2017) is still an accurate description of current GBATEK | Dispute seeds | The `disp-gbatek-errata` entry would over-state a defect that GBATEK later fixed; registry entry records the post date and stays `open` with `[UNK]` hardware claims |
| A4 | jsoncpp leniencies (comments, trailing commas, duplicates) stay the same in CMake releases other than 3.25.3 and 4.4.3 | Pattern 1 | The strictness guard might later reject nothing it should or flag valid files; the floor job and a 4.x job both run the controls, so a change is loud |
| A5 | mGBA sets the HBlank flag at 1,008 cycles; only the `VIDEO_HDRAW_LENGTH = 1008` constant was read, not the call site | Dispute seed table, U1 | The registry would mislabel mGBA's value; tagged `[INF]` in the entry until the call site is read |
| A6 | Permitted-use values (`public-ci`, `local-only`, `differential-only`), `redistributable` tri-state, and the manifest field names are acceptable to the owner | Manifest row shape | Rename later with a `gamebloke.manifest/2` bump; no rights decision depends on them (discretionary per CONTEXT) |
| A7 | `fetch-pin.sh` should not extract archives in this phase | Code Examples | A later phase may need extraction; additive change |

## Open Questions (RESOLVED)

> All eight resolved for planning. The three that were OPEN carry the orchestrator's decision (2026-11-10, within the owner's locked CONTEXT decisions).

1. **Where does the closed vocabulary live?** (RESOLVED, recommendation) One file `contracts/evidence.json` read by all validators, with a contract validator pinning the expected sets. Alternative: hard-code lists in `cmake/GblVocab.cmake`. Rejected because the nine states are specified as data.
2. **Strictness guard versus plain `string(JSON)`.** (RESOLVED, recommendation) Build the guard; the cost is about 40 lines and 8 tests, and it removes the single biggest "validator that cannot fail" hole found in this research. If the planner wants the smallest plan, the minimum is the duplicate-key and trailing-content checks.
3. **Should `GUEST_STOP` and other run states require a `reason`?** (RESOLVED: no, GUEST_STOP needs no reason until Phase 4/PROF-03) CONTEXT locks reason only for SKIPPED and MISSING and the refusal pair for REFUSED. Phase 4 SC2 wants GUEST_STOP's cause named once PROF-03 exists; add the `reason` requirement (or a `cause` field) then, via a `gamebloke.result/2` bump or a data change in `contracts/evidence.json`.
4. **`hle:` list ordering and duplicates.** (RESOLVED: no ordering or duplicate rule in v1) A sorted-unique rule would make claim strings canonical; deferred until the first HLE claim exists.
5. **First real manifest rows.** (RESOLVED) None in this phase; `"fixtures": []` with a printed row count and a positive control that proves non-vacuity. Phase 5 adds owned rows (and decides whether a built ROM digest can be `null` until the toolchain spike); Phase 6 adds jsmolka rows.
6. **Linux-lane curl.** (RESOLVED, recommendation) Add `curl` to the apt line in `scripts/linux-lane.sh`. Check the GitHub `ubuntu-26.04` image for curl on the first CI run (A1).
7. **Should the real-logo-style owner gates apply here?** (RESOLVED: no) This phase touches no rights-bearing bytes. G3 default (pinned fetch, no third-party binaries) applies and no owner decision is needed.
8. **`permitted_use` and `redistributable` vocabulary.** (RESOLVED: closed sets as recommended in Manifest row shape; no owner decision implied) Recommended closed sets are in "Manifest row shape"; the owner is not needed unless a value implies a rights decision.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake (local) | all validators and tests | yes | 4.4.3 | - |
| CMake 3.25.3 (floor) | floor run of the full suite | yes (scratch copy, digest verified) | 3.25.3 | none; keep the floor run mandatory before the phase closes |
| Ninja | presets | yes | 1.13.2 | - |
| POSIX `sh` | `fetch-pin.sh`, tests | yes (`/bin/sh` is bash 3.2 in POSIX mode; `/bin/dash` present) | - | - |
| `curl` | `fetch-pin.sh` and its `file://` controls | yes locally | 8.7.1 | add to the Linux lane apt line; `no-curl` exit 2 if absent |
| `cmake -E sha256sum` | digest | yes | with CMake | - |
| `git` | `git ls-files` / `git check-ignore` tests, hygiene `--tree` | yes | - | - |
| `shellcheck` | optional local lint | yes (Homebrew) | - | skip |
| `gh` | read-only verification of the registry seeds | yes | - | - |
| Docker | `scripts/linux-lane.sh` | yes (client 29.5.2; daemon not probed) | - | the first Linux proof is the CI run |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** `curl` inside the Linux-lane container (add to apt line).

## Validation Architecture

Nyquist validation is enabled (`workflow.nyquist_validation: true`). [VERIFIED: .planning/config.json]

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest through `gbl_add_test` plus the Phase 1 sorted-inventory and junit checks |
| Config file | `CMakeLists.txt`, `CMakePresets.json`, `tests/gates.cmake`, `tests/inventory/{ci,hygiene}.txt` |
| Quick run command | `cmake -S . -B build/hygiene -G Ninja -DGBL_LANE=hygiene && ctest --test-dir build/hygiene -LE post -R '^(json|contract|registry|profile|manifest|pins|fetch|result)\.' -j4` (the hygiene lane needs no compile; 52 existing tests ran in about 5 s at `-j4`) |
| Full suite command | `cmake --workflow --preset ci` (also run `cmake --workflow --preset hygiene`) |
| Floor run | the Kitware 3.25.3 `cmake --workflow --preset ci` from the repository root, per `docs/verify.md` "Slow-lane checks" |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-01 | Real profile is complete and consistent; a profile missing machine/boot/BIOS/cart/timing source/unmodeled item is rejected | gate + control | `ctest --test-dir build/hygiene -R '^profile\.'` | Wave 0 |
| PROF-01 | Doc and JSON unmodeled list stay in sync | control | `-R '^profile\.nc\.doc_'` | Wave 0 |
| PROF-02 | Result claim tuple present; BIOS provider outside R1 rejected | control | `-R '^result\.nc\.(bad_bios_provider\|missing_field\|bad_cart\|bad_input_timeline\|bad_evidence)'` | Wave 0 |
| EVID-01 | Complete manifest row accepted; all eight fields required | positive + control | `-R '^manifest\.(pos\|nc\.missing_field)'` | Wave 0 |
| EVID-02 | Missing field, branch-pinned revision, unknown provenance class rejected each with a stated reason; linter is in the canonical command | control + gate | `-R '^manifest\.'`; `cmake --workflow --preset ci` runs `manifest.lint` | Wave 0 |
| EVID-03 | Pinned fetch places a verified file; mismatch exits non-zero loudly with nothing placed; `.fixtures/` is ignored and `git ls-files` lists no third-party binary | positive + control + gate | `-R '^fetch\.'` (`fetch.nc.digest_mismatch`, `fetch.pos.fetch_ok`, `fetch.ignored`, `fetch.no_tracked`) | Wave 0 |
| EVID-05 | Nine states exist as data; unknown state and PASS-without-run rejected | control | `-R '^(contract\|result)\.nc\.(state_set\|unknown_status\|pass_without_run\|pass_without_digest\|ran_state_mismatch)'` | Wave 0 |
| EVID-06 | Registry holds the six seeds; a result whose fixture points at a registry entry must carry `disputed` | gate + control | `-R '^registry\.'`, `-R '^result\.nc\.(undisclosed_dispute\|unknown_dispute)'` | Wave 0 |
| VERIF-01 (manifest clause) | `manifest.lint` runs in `cmake --workflow --preset ci` | gate | `cmake --workflow --preset ci` | Wave 0 |
| All | Every control is in the inventory; none is skipped | inventory | `inventory.junit` post step (`cmake --workflow --preset ci`) | exists (Phase 1) |
| Hygiene | New files contain no home paths, emails, credential shapes, binary-looking files | gate | `sh scripts/hygiene.sh --tree && CMAKE=$(command -v cmake) sh scripts/signature-scan.sh --tree` | exists (Phase 1) |
| Traps | `fetch-pin.sh` uses the signal-trap idiom | gate | `sh scripts/check-traps.sh` | exists (Phase 1) |

### Sampling Rate
- **Per task commit:** the quick run command for the family touched, plus `sh scripts/hygiene.sh --tree`.
- **Per wave merge:** `cmake --workflow --preset hygiene` and `cmake --workflow --preset ci`.
- **Phase gate:** `cmake --workflow --preset ci` green (also on the 3.25.3 floor once), `sh scripts/check-traps.sh`, then the PR's `CI required` green before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `cmake/GblJson.cmake`, `cmake/CheckProfile.cmake`, `cmake/CheckRegistry.cmake`, `cmake/CheckManifest.cmake`, `cmake/CheckResult.cmake`: the validators
- [ ] `contracts/evidence.json`, `profiles/agb.json`, `registry/disputes.json`, `tests/fixtures/manifest.json`, `pins.txt`: authority data
- [ ] `docs/profile-agb.md`, `docs/evidence-contract.md`; update `docs/verify.md` reason-code list and extension-point paragraph
- [ ] `scripts/fetch-pin.sh`; `.gitignore` gains `.fixtures/`; `scripts/linux-lane.sh` gains `curl`
- [ ] `tests/gates/{json,contract,registry,profile,manifest,pins,fetch,result}_case.cmake` plus a shared mutation helper (assert the mutation took effect)
- [ ] Registrations in `tests/gates.cmake` (or an included file) and the matching sorted lines in `tests/inventory/ci.txt` and `tests/inventory/hygiene.txt` (not `asan.txt`)
- Framework install: none.

## Security Domain

`security_enforcement` is enabled (absent or true), ASVS level 1. [VERIFIED: .planning/config.json `"security_enforcement": true, "security_asvs_level": 1`]

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | - |
| V3 Session Management | no | - |
| V4 Access Control | no (repository files, not a service) | - |
| V5 Validation, Sanitization, Encoding | yes | allow-list validation of every manifest/pins/profile/result field (charsets, closed enums, 40/64-hex); URLs accepted only with an `https://` prefix (so an argument beginning with `-` cannot reach curl as an option); `set -f` and quoting in the shell; never `eval`; no value is interpolated into a shell command line other than as a quoted argument |
| V6 Cryptography | yes (integrity only) | SHA-256 via `cmake -E sha256sum`; never hand-rolled; no secrets involved |
| V9 Communications | yes | `curl --proto '=https' --proto-redir '=https'`; certificate verification left at the default (never `-k`); `file://` only through the explicit test-only env var |
| V12 Files and Resources | yes | destination allow-list charset, no `.`/`..` component, no absolute path, symlink-escape check, refuse existing link or directory, temp directory on the same filesystem, `--max-filesize` bound |
| V14 Configuration | yes (supply chain) | revisions pinned to a 40-hex commit or SHA-256; `pins.txt` reviewed in PRs; no third-party binary tracked (`.gitignore` plus `git ls-files` test plus the Phase 1 signature scan) |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Path traversal through `<dest>` (`../../.git/hooks/pre-commit`) | Tampering, Elevation | allow-list charset, per-component `.`/`..` rejection, `pwd -P` containment check; controls for traversal, absolute, charset, symlink |
| Command or option injection through URL or dest (a field starting with `-`, whitespace, quotes, `$()`) | Tampering | scheme prefix required (https or test-only file), reject whitespace/quotes/backslash in URL, URL passed with `--url`, no `eval`, `set -f`; control for each rejected shape |
| Fetching tampered or substituted content | Tampering | digest checked before the file is placed; mismatch exits non-zero and places nothing (verified in the prototype) |
| Downgrade or redirect to plain HTTP or `file://` | Tampering, Information disclosure | `--proto` and `--proto-redir` restricted to the same single scheme; `http://` rejected at row validation |
| Unstable upstream archive silently changing the pinned bytes | Tampering (availability of integrity) | pin single raw files at a commit or release assets, not generated archives (Pitfall 9) |
| Partial or interrupted download | Tampering, Denial of service | download to a temp file, hash, then rename; a partial file hashes wrong and is never placed; trap cleans up and a signal ends the script (traps gate) |
| Oversized download | Denial of service | `--max-filesize`, `--max-time` |
| A fetched ROM or BIOS ending up in git or CI artifacts | Information disclosure (rights) | `.fixtures/` ignored; `git add -f` is still caught by the signature scan's tracked-file list; tests never fetch real fixtures |
| Hand-edit error in an authority file silently accepted (duplicate key, trailing comma) | Tampering (integrity of evidence) | strictness guard (Pattern 1) with a control per code |
| Mutation control testing nothing | Repudiation of evidence | mutation helper asserts the document changed; positive control on the unmutated copy |
| Leaking file content through error text | Information disclosure | print reason codes and paths only (Pitfall 8) |

## Sources

### Primary (HIGH confidence)
- Local probes on CMake 3.25.3 (Kitware tarball, SHA-256 `771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e` verified against Kitware's `cmake-3.25.3-SHA-256.txt`) and 4.4.3: `string(JSON)` behavior, policies in `-P` mode, `message()` wrapping, regex semantics, `foreach(RANGE)`; `curl` 8.7.1 protocol restrictions; the `fetch-pin.sh` prototype under dash and `/bin/sh`.
- Repository files read this session: `CMakeLists.txt`, `CMakePresets.json`, `cmake/ExpectFail.cmake`, `cmake/GblTests.cmake`, `cmake/CheckSignatureDigests.cmake`, `cmake/InventoryCompare.cmake`, `cmake/RulesetCheck.cmake` (head), `tests/gates.cmake`, `tests/gates/signature_case.cmake`, `tests/gates/hygiene_case.cmake` (head), `tests/ci_gates.cmake` (excerpt), `tests/inventory/{ci,hygiene}.txt`, `scripts/hygiene.sh`, `scripts/signature-scan.sh`, `scripts/lib/signature-common.sh`, `scripts/check-traps.sh`, `scripts/linux-lane.sh` (grep), `docs/verify.md`, `.gitignore`, `.planning/{REQUIREMENTS,ROADMAP,STATE}.md`, `.planning/research/{SUMMARY,ARCHITECTURE,PITFALLS}.md` (relevant sections), the Phase 1 `01-EVIDENCE.md` and `01-REVIEW.md` headings.
- GBATEK (problemkaputt.de/gbatek.htm, no$gba 3.06 extract), local copy: LCD Dimensions and Timings; LCD I/O Interrupts and Status; the 4000800h Internal Memory Control section; GBA SP Notes; Gameboy Micro notes; GBA Backwards Compatibility CGB Mode.
- `gh api` read-only: `mgba-emu/suite` issues #22, #24, #27 (titles, bodies, comments) and commit `e694203`; `mgba-emu/mgba` `src/gba/dma.c` and `include/mgba/internal/gba/video.h` at tag 0.10.5; release 0.10.5 timestamp.
- Codeberg API and raw files (read-only): `nba-emu/hw-test` `dma/latch/source/main.c`; `nba-emu/NanoBoyAdvance` `src/nba/src/hw/ppu/ppu.cc`.
- mGBA forum "Game Boy Advance Documentation" (forums.mgba.io/showthread.php?tid=343): the GBATEK errata list.

### Secondary (MEDIUM confidence)
- en.wikipedia.org Game Boy Advance SP (AGS-001/AGS-101); consolemods.org GBA Model Differences and nindb.net (AGB-001, OXY-001 via search).
- github.blog changelog 2023-01-30 "Git archive checksums may change".
- actions/runner-images Ubuntu 24.04 readme (curl 8.5.0).
- Sibling repositories (read-only, engineering lessons only): `~/projects/gabbaboy/cmake/VerifyFixtureManifest.cmake`, `FixtureAdmissionPolicy.json`, `VerifyEvidenceLedger.cmake`; `~/projects/cogmaster/pins.txt`, `scripts/fetch-pin.sh`, `ASSET_POLICY.md`, `PROVENANCE.md`.

### Tertiary (LOW confidence)
- None used as a basis for a recommendation. Hardware claims inside issue reports and the 2017 errata list are `[UNK]` until reproduced.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH. No new dependency; every tool exercised this session.
- Architecture: HIGH for the loader, controls, and inventory integration (probed and read from source); MEDIUM for field names and enums, which are discretionary.
- Pitfalls: HIGH for the CMake and shell items (probed); MEDIUM for GitHub archive stability (official changelog, policy may have changed since).
- GBA facts: MEDIUM. Values are from GBATEK, a community reference; the seed disputes record what each artifact does, not what hardware does.

**Research date:** 2026-11-10
**Valid until:** 2026-12-10 for CMake and tooling behavior (stable); re-check the mGBA suite issue states and NBA/mGBA constants if the registry is touched later, since both projects are actively committed.
