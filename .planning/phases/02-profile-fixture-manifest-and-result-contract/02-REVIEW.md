---
phase: 02-profile-fixture-manifest-and-result-contract
reviewed: 2026-11-10T00:00:00Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - .gitignore
  - CMakeLists.txt
  - cmake/CheckContract.cmake
  - cmake/CheckFixtureDir.cmake
  - cmake/CheckManifest.cmake
  - cmake/CheckProfile.cmake
  - cmake/CheckRegistry.cmake
  - cmake/CheckResult.cmake
  - cmake/GblJson.cmake
  - contracts/evidence.json
  - contracts/examples/manifest.json
  - contracts/examples/result-pass.json
  - docs/evidence-contract.md
  - docs/profile-agb.md
  - docs/verify.md
  - pins.txt
  - profiles/agb.json
  - registry/disputes.json
  - scripts/fetch-pin.sh
  - scripts/linux-lane.sh
  - tests/fixtures/manifest.json
  - tests/gates.cmake
  - tests/gates/contract_case.cmake
  - tests/gates/fetch_case.cmake
  - tests/gates/json_case.cmake
  - tests/gates/json_mutate.cmake
  - tests/gates/manifest_case.cmake
  - tests/gates/pins_case.cmake
  - tests/gates/profile_case.cmake
  - tests/gates/registry_case.cmake
  - tests/gates/result_case.cmake
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
findings:
  critical: 1
  warning: 9
  info: 5
  total: 15
  necessity: 0
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-11-10
**Depth:** standard
**Files Reviewed:** 33
**Status:** issues_found

## Summary

All 33 files were read in full. Claims were reproduced on scratch copies (a `git archive` of HEAD under the session scratchpad); no repository file was modified. cmake 4.4.3, curl, python3 and shellcheck were available locally; the 3.25 floor was not re-run (the caller states it passes).

The strict JSON loader holds up well. Duplicate keys (flat, nested, inside arrays, escaped `a`), trailing content, NUL tails, comments, trailing commas, NaN and single quotes were all probed and fail for the right code. The `fetch-pin.sh` digest-before-placement, scheme allow-list, quoting and symlink-destination handling are sound, and shellcheck is clean.

The material problems are of three kinds:

1. One proven fail-open in the vocabulary validator, the file that is described as the independent oracle (CR-01).
2. Several rules the contract implies but the linter does not enforce: rights cross-checks, result-to-manifest digest identity, and pin-to-manifest agreement (WR-03 to WR-05). These are all accepted by the current code, as reproduced below.
3. A large amount of "control passes, but for an incidental reason" coverage. The reason code in the REGEX matches, but disabling the specific rule it claims to test leaves the control green. This was shown by mutating the validators on a scratch copy (WR-06 to WR-08).

In addition, `fetch-pin.sh` carries inert redirect protections that its header says are active (WR-01), and it creates directories outside the root before it rejects a symlinked parent (WR-02).

The profile and registry content labels were checked against a local GBATEK copy. The 16,777,216 Hz clock, 308 dots, 1232 cycles per line, 280896 cycles per frame, "H-Blank flag is 0 for 1006 cycles", the Micro EWRAM lock-up at the 0Eh setting, and the Micro's missing cartridge switch all match. Source-label honesty is good, with one borderline case (IN-04).

## Critical Issues

### CR-01: Contract validator accepts a malformed vocabulary whose arrays hold one `;`-joined string (fail-open in the oracle)

**File:** `cmake/CheckContract.cmake:56-75` (also `:166`)
**Issue:** `gbl__contract_strings` collects items with `list(APPEND items "${item}")`. An item that contains `;` is silently split into several list elements. The loader header (`GblJson.cmake:33-35`) names this exact hazard, but the contract validator does not guard against it.

Reproduced on a scratch copy of `contracts/evidence.json`:

- `"source_labels": ["REF;SRC;IMPL;INF;UNK"]` gives `contract: ok (9 states, 5 labels, 5 classes, 6 claim fields)`.
- The same shape for `provenance_classes` also prints `contract: ok`.
- `claim_fields` shares the same code path.

So the gate that exists to pin "exactly REF, SRC, IMPL, INF, UNK" passes a file whose JSON arrays do not hold those five strings. Any non-CMake consumer of `evidence.json`, such as the Phase 4 runner, would read one label.

The `states[].name` variant (`list(APPEND names "${name}")`, line 166) does not pass. It dies inside `if(DEFINED GBL_PIN_RULE_${name})` with a raw CMake "Unknown arguments" error and no `contract:` code. That is fail-closed but not a stable reason code.

**Fix:** reject any item that contains a semicolon before appending, and reuse the check for `name`:
```cmake
gbl_json_string(item "${doc}" ${field} ${i})
if("${item}" STREQUAL "<missing>" OR "${item}" MATCHES ";")
  gbl_finding(contract missing-field "${field}[${i}]")
else()
  list(APPEND items "${item}")
endif()
# and at line 165, before list(APPEND names ...):
if("${name}" MATCHES ";")
  gbl_finding(contract bad-state-rule "states[${i}]: name")
  continue()
endif()
```
Add one control for each (a label array of one `;`-joined string, and a `;`-joined state name).

## Warnings

### WR-01: `fetch-pin.sh` never follows redirects, so `--proto-redir` and `--max-redirs` are inert and release-asset pins cannot work

**File:** `scripts/fetch-pin.sh:161-164` (header claims at `:22-25`, `pins.txt:11`, `docs/evidence-contract.md:156`)
**Issue:** curl is run without `-L/--location`. `--proto-redir =https` and `--max-redirs 5` do nothing unless redirects are followed. The header says curl "is limited to the row's single scheme for the request and for redirects ... and has a redirect cap". That is not true of the code. The documented pin kind "maintainer-uploaded release asset" is a GitHub `releases/download/...` URL, which always answers 302. Reproduced with the script's flags against a local server:

- With a 302 response and `--fail`, curl exits 0 and writes a 0-byte payload.
- The script then reports `digest-mismatch ... got e3b0c442...`. That is the wrong reason and hides the real one.
- With `-L` added, the redirect is followed and the real payload arrives.

No control exercises a redirect.

**Fix:** either add `--location` (keeping `--proto-redir "$proto" --max-redirs 5`, which then take effect) and add a control that checks a redirect to a non-https scheme is refused, or delete the two flags and the header/doc claims and state that pins must be direct URLs. If redirects are followed, `--proto-redir =https` is what stops an https-to-http downgrade.

### WR-02: `mkdir -p` runs before the symlink-escape check, creating directories outside the root

**File:** `scripts/fetch-pin.sh:174-180`
**Issue:** `mkdir -p "$parent"` follows an existing symlink component. The containment check (`real_parent` vs `real_root`) happens afterwards. Reproduced: with `root/a` as a symlink to `outside/` and dest `a/b/c.bin`, the script prints `fetch-pin: symlink: ... parent resolves outside the root` and exits 1, but `outside/b` has already been created. The documented postcondition is that a failure places nothing. The `fetch.nc.symlink` control only uses `example/payload.bin`, where the symlinked parent already exists, so `mkdir -p` has nothing to create. It checks only that `payload.bin` did not escape, so it cannot see this.

**Fix:** resolve before creating. Walk the dest components and, for each existing ancestor, `cd ... && pwd -P` and compare with the root, or create directories one component at a time with `[ ! -L ]` checks. Extend the control to a dest with a non-existent subdirectory (`example/sub/payload.bin`) and assert that `outside/sub` does not exist.

### WR-03: Manifest linter has no rights cross-field rules (public-ci / redistributable / BIOS)

**File:** `cmake/CheckManifest.cmake:272-285` and `:364-388`
**Issue:** `license.redistributable`, `license.spdx`, `bios_requirement` and `permitted_use` are each validated alone. AGENTS.md requires that commercial ROMs, BIOS bytes and unlicensed test content stay out of public CI, and says "a matching hash proves identity, not redistribution rights". Reproduced: a row with `redistributable: "no"`, `license.spdx: "x"`, `bios_requirement: "user-official"`, `permitted_use: ["public-ci"]` and a `source.url` with credentials in its userinfo (`user:secret` before the host of an `example.invalid` URL) passes `manifest: ok (1 rows)`. This is the rights gate the manifest exists to provide, and it is fail-open once Phase 5 adds rows.

**Fix:** add a reason code (for example `rights-mismatch`) that fires when `public-ci` is in `permitted_use` and `redistributable` is not `yes`, or when `bios_requirement` is `user-official` and `public-ci` is present. Reject `://user[:pass]@` in `source.url` and in pins. Add a control and a verify.md entry for the new code.

### WR-04: Result validator never ties `fixture.rom_sha256` to the manifest row, so PASS on a wrong digest is accepted

**File:** `cmake/CheckResult.cmake:179-185`, `:259-283`
**Issue:** `claim.cart.rom_sha256` is compared only with `fixture.rom_sha256`, which is the same document. The manifest row digest is exported by `gbl_manifest_read` as `<prefix>_ROW_<id>_SHA256`, but the result checker never reads it. Reproduced: with the manifest row changed to a `url` kind and `source.sha256 = abab...`, the unchanged result (digest `0123...`, status PASS) prints `result: ok (PASS)`. A PASS for fixture X can therefore carry the digest of any other file. `docs/evidence-contract.md` does not claim this rule, but "a matching hash proves identity" is the project's evidence premise.

**Fix:** where the row kind is `url` or `in-tree` and has a digest, require `fixture.rom_sha256` to equal it (`claim-mismatch`). For `git` rows, state in the doc that no digest tie exists yet. Add a control.

### WR-05: Pin cross-check misses URL agreement, unreferenced pins, and url/git rows with no `fetch`

**File:** `cmake/CheckManifest.cmake:553-578`
**Issue:** `gbl__manifest_pin_check` has three gaps. All were reproduced with `MODE=all` printing `manifest: ok` and `pins: ok`.

1. A `url` row compares only the digest. The pinned URL is never compared with `source.url`, so the manifest can name one origin and the fetch can use another.
2. A `git` row checks only that the pinned URL contains the commit as a substring. A pin of `https://attacker.invalid/<commit>/a.bin` satisfies a row whose `source.url` is a different host.
3. Rows in `pins.txt` that no manifest row references (orphans), and url/git rows without `fetch.pin_dest`, produce no finding.

**Fix:** for `url` rows require `${p}_DEST_<dest>_URL` to equal `source.url`. For `git` rows require the pinned URL to start with `source.url` minus any `.git` suffix, as well as containing the commit. Flag pins whose dest no row references, and flag a `git` or `url` row with no `fetch`, or document that the latter is allowed.

### WR-06: Contract controls do not detect removal of the locked state rules

**File:** `tests/gates/contract_case.cmake:103-116`
**Issue:** The headline rule "a fixture that did not run is never PASS" is pinned in `CheckContract.cmake:48-51`, but the controls mutate only generic cases. Mutation on a scratch copy of the validator, running `contract.nc.bad_state_rule` afterwards:

| Mutation | Control result |
| --- | --- |
| `GBL_PIN_RULE_PASS true required - -` changed to `true - - -` | still passes (rc 0) |
| `GBL_PIN_RULE_REFUSED false - - required` changed to `false - - -` | still passes |
| `GBL_PIN_RULE_MISSING false - required -` changed to `true - required -` | still passes |

The `digest_value` control sets `"maybe"`, which is caught by the generic allowed-value check, not by the pin. Both give the same `bad-state-rule` code, so the REGEX cannot tell them apart.

**Fix:** add controls that mutate the committed data to a weakened but individually valid value, and give each finding a distinct detail the REGEX asserts: PASS `digest: "optional"` (expect `PASS: digest must be required`), MISSING `ran: "true"` or `"either"`, REFUSED with `refusal` removed or not `required`, SKIPPED `ran: "either"`.

### WR-07: Profile timing-arithmetic controls cannot tell which invariant fired

**File:** `tests/gates/profile_case.cmake:194-203` (rules at `cmake/CheckProfile.cmake:146-179`)
**Issue:** The validator reports "one finding per broken invariant". The control mutates `cycles_per_line` to 1231, which breaks two invariants at once, and its REGEX is the bare `profile: timing-arithmetic`. Mutation results, running `profile.nc.timing_arithmetic` after each change on a scratch copy of the validator:

| Disabled invariant | `profile.nc.timing_arithmetic` |
| --- | --- |
| `dots_visible + dots_hblank = dots_per_line` | still passes |
| `cycles_per_line * lines_per_frame = cycles_per_frame` | still passes |
| `dots_per_line * cycles_per_dot = cycles_per_line` | still passes |

So three of the five invariants can be deleted with every control green. The `hblank-flag-edge` rule that the entry must carry `disp-hblank-flag-edge` (`CheckProfile.cmake:247-251`) is the same: disabling it leaves `unmodeled_bad_entry` green.

**Fix:** one variant per invariant, each changing one input so that exactly one invariant breaks, with a REGEX on the detail text (for example `dots_visible \+ dots_hblank`). Add an `unmodeled_bad_entry` variant that points `hblank-flag-edge` at another dispute id.

### WR-08: Documented reason-code triggers that have no control

**File:** `tests/gates/result_case.cmake:290-295` (and others, below)
**Issue:** `every reason code has a negative control` holds at the code level, but these documented triggers are untested. Each was verified by disabling that one finding on a scratch copy and running the matching control, which stayed green:

- result `claim-mismatch`: "no evidence item names fixture.id" (`CheckResult.cmake:339-341`). The `claim_mismatch` control only mutates the cart digest.
- result `missing-digest`: an optional digest that is a malformed string (`:194-196`).
- result `bad-evidence`: an item without `fixture` (`:328-331`).
- profile `bad-source-label`: a null value under a non-UNK label (`CheckProfile.cmake:117-118`, `:128-131`). The control only changes the label text.
- manifest `wrong-type`: a non-object `fetch` (`CheckManifest.cmake:393-394`).

**Fix:** add one variant per trigger. A fixture that did not name itself in evidence, a `HARNESS_FAIL` with a 10-hex digest, an evidence item with `fixture` removed, `master_clock_hz.value` null under `REF`, and `"fetch": "x"`.

### WR-09: `fetch-pin.sh` has no reason code when `cmake -E sha256sum` is missing or fails

**File:** `scripts/fetch-pin.sh:166-168` (contract at `:27-40`, `docs/verify.md:128-131`)
**Issue:** `got=$("${CMAKE:-cmake}" -E sha256sum "$tmp/payload")` runs under `set -e` with no `die`. With `CMAKE=/nonexistent`, the script prints a shell error (`line 166: /nonexistent: No such file or directory`), exits 1, and emits no `fetch-pin: <code>` line. `no-curl` has a code and a control; the equally required digest tool does not. The docs say "every other failure exits 1" and list the codes as stable.

**Fix:**
```sh
cmake_prog=${CMAKE:-cmake}
command -v "$cmake_prog" >/dev/null 2>&1 || die no-cmake "cmake not found" 2
got=$("$cmake_prog" -E sha256sum "$tmp/payload") || die digest-tool-failed "$dest"
```
Add the code to the header, verify.md and a control.

## Info

### IN-01: curl honours `~/.curlrc` and proxy and CA environment, so "never disabled" is stronger than the code

**File:** `scripts/fetch-pin.sh:161-164`, `docs/evidence-contract.md:171`
**Issue:** A user `~/.curlrc` containing `insecure` or a `proxy` entry, or `CURL_CA_BUNDLE`, changes the transport. The SHA-256 check still protects integrity, so the impact is limited. Also `--max-filesize` is not enforced when the server gives no length (chunked), so the 256 MiB "size cap" is bounded only by the 600 s timeout.
**Fix:** pass `-q` as the first curl argument and reword the header to say the digest, not curl, is the integrity guarantee.

### IN-02: Grammar differences between the pins linter and `fetch-pin.sh`

**File:** `cmake/CheckManifest.cmake:508-526`, `scripts/fetch-pin.sh:57-73`, `pins.txt:5`
**Issue:** The linter accepts rows the script cannot use or treats differently. Reproduced with `MODE=pins`:

- A row ending in `\r` is accepted; the script splits on spaces, sees dest `a\r`, and reports `no-row`.
- A dest starting with `-` (`-x`) is accepted but cannot be requested because `-*` is read as an option.
- Dests `x/a` and `x/a/b` are both accepted although one is a file and the other needs it to be a directory.
- Tabs and repeated spaces are accepted although `pins.txt` says "single spaces".

**Fix:** reject `\r`, a leading `-`, and prefix collisions in `gbl_pins_read`, or state in the header that the script is the stricter reader.

### IN-03: Profile arithmetic invariants wrap silently on signed overflow

**File:** `cmake/CheckProfile.cmake:148-178`
**Issue:** `math(EXPR)` only emits a CMake author warning on overflow and continues with the wrapped value. Reproduced: `dots_per_line = 4611686018427387981`, `cycles_per_dot = 4`, `cycles_per_line = 308` satisfies `dots_per_line * cycles_per_dot = cycles_per_line` because 4 x (2^62 + 77) wraps to 308. Values above int64 become a `1e+20` form and are rejected as non-integers, so the window is narrow and needs a deliberate edit.
**Fix:** reject any timing value with more than 9 digits (`timing-arithmetic`) before the `math` calls.

### IN-04: `REF` label on an absence-of-statement note

**File:** `registry/disputes.json:18-22`
**Issue:** `disp-dma-latch` source 3 is labelled `REF` for "No statement about a DMA latch was found in the extract that was read". Per AGENTS.md, REF is for reference-grade claims. This note records a negative search over an extract (a partial copy), and it supports the registry's own conclusion that the reference "neither settles nor contradicts". That is an inference from a partial read.
**Fix:** relabel to `INF` (or `UNK`) and name the document revision and the section searched.

### IN-05: Profile narrative timing table is not checked against the JSON

**File:** `cmake/CheckProfile.cmake:263-318`, `docs/profile-agb.md:47-59`
**Issue:** `gbl__profile_doc_sync` compares only the `profile_version:` line and the `## Unmodeled` ids. The timing table (values, labels, cited sections) is hand-copied and can drift from `profiles/agb.json` without a finding. The profile doc says "a check compares the two" about the unmodeled list and version only, so the claim is accurate, but the doc invites trust in the table.
**Fix:** either add a row-by-row comparison of the table's value and label columns, or add one sentence to the doc saying that the timing table is narrative and is not checked.

---

_Reviewed: 2026-11-10_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
