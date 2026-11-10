---
phase: 01-ci-and-verification-foundation
reviewed: 2026-11-09T00:00:00Z
depth: standard
files_reviewed: 50
files_reviewed_list:
  - .githooks/pre-commit
  - .github/rulesets/main.json
  - .github/workflows/ci.yml
  - CMakeLists.txt
  - CMakePresets.json
  - cmake/CheckCoreIncludes.cmake
  - cmake/CheckSignatureDigests.cmake
  - cmake/CiRequired.cmake
  - cmake/ExpectFail.cmake
  - cmake/GblTests.cmake
  - cmake/InventoryCompare.cmake
  - cmake/RulesetCheck.cmake
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - runner/main.c
  - scripts/apply-repo-settings.sh
  - scripts/check-action-pins.sh
  - scripts/check-junit.sh
  - scripts/check-title.sh
  - scripts/check-workflow.sh
  - scripts/classify-changes.sh
  - scripts/hygiene.sh
  - scripts/lib/signature-common.sh
  - scripts/linux-lane.sh
  - scripts/signature-digest.sh
  - scripts/signature-scan.sh
  - src/version.c
  - tests/asan_control.c
  - tests/check.h
  - tests/ci/ci_cases.cmake
  - tests/ci/classify_cases.sh
  - tests/ci/ruleset_cases.cmake
  - tests/ci/title_cases.sh
  - tests/ci/workflow_cases.cmake
  - tests/ci_gates.cmake
  - tests/core.cmake
  - tests/gates.cmake
  - tests/gates/hygiene_case.cmake
  - tests/gates/signature_case.cmake
  - tests/harness.cmake
  - tests/harness_case.cmake
  - tests/inventory/asan.txt
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
  - tests/sanitizer.cmake
  - tests/signatures/allowlist.txt
  - tests/signatures/bios.sha256
  - tests/signatures/logo.sha256
  - tests/ub_control.c
  - tests/version_test.c
findings:
  critical: 1
  warning: 8
  info: 6
  total: 15
  necessity: 0
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-11-09
**Depth:** standard
**Files Reviewed:** 50
**Status:** issues_found

## Summary

The phase builds a verification foundation whose value rests on its gates failing
closed. Most of the design is sound: the aggregator predicate (`CiRequired.cmake`)
and the classifier fail closed on the inputs I tried, `ExpectFail.cmake` ties every
control to a reason code, the junit parser is robust to semicolons and unusual
output, and `ci.yml` keeps untrusted PR metadata out of shell interpolation
(everything flows through `env:`).

The defects cluster in three places, each a path where a gate can report success
without having checked what it claims to check:

1. The temp-dir cleanup trap in the scan scripts resumes the script after a signal
   with the findings file already deleted, so `hygiene.sh` and `signature-scan.sh`
   print `ok` and exit 0 while findings exist (reproduced).
2. The workflow policy and pin gates are wired to `ci.yml` only, and the policy
   gate does not inspect job-level permissions or absolute-path tool invocations
   (all reproduced on copies of `ci.yml`).
3. Several gates lack a "did I scan anything" guard or have an easy bypass shape:
   the core-purity gate (quoted system includes, missing directories), the
   `classify-changes.sh` completeness check (rename padding), `check-junit.sh`'s
   unanchored exclude regex, and the credential-assignment regex (case-sensitive).

Every claim below marked "reproduced" was run against the real scripts using copies
or sandboxes under the scratchpad; no repository file was modified. Necessity
(category 4) was assessed per `necessity-review.md`; no finding met its evidence rule.

## Critical Issues

### CR-01: Signal trap deletes the temp dir and lets the script continue, so gates print `ok` with findings present

**File:** `scripts/hygiene.sh:74,124-128`, `scripts/signature-scan.sh:98,183-187`
(same idiom: `scripts/signature-digest.sh:45`, `scripts/apply-repo-settings.sh:92`)

**Issue:** All four scripts use `trap 'rm -rf "$tmp"' EXIT HUP INT TERM`. In POSIX
sh a trap action for a signal runs and then execution RESUMES after the interrupted
command; it does not exit. The handler removes `$tmp`, and the script goes on to
`[ -s "$tmp/findings" ]`, which is now false because the file was deleted, so the
script prints `hygiene: ok (N files)` / `signature: ok (N files)` and exits 0.

Reproduced: a directory holding a real finding (a home path) and a shim that delays
`xargs`; `kill -TERM` sent to the shell pid while the scan was running produced
`hygiene: ok (2 files)` with exit 0. The same directory without the signal produced
the `home-path` and `credential` findings and exit 1. A minimal repro of the idiom
(`trap 'rm -rf "$tmp"' EXIT HUP INT TERM; kill -TERM $$; echo STILL RUNNING`) shows
the script continuing with the temp dir gone and exiting 0, under both `sh` and
`dash`. A caller such as `timeout 60 sh scripts/hygiene.sh --staged` (coreutils
`timeout` signals only the child) would read a timeout as a clean pass. These are
the secret/home-path and rights-signature gates, so a false pass is the failure
class this phase exists to remove.

**Fix:** Make signals terminate the script and let the EXIT trap do the cleanup:
```sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/gbl-hygiene.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
```
Apply to all four scripts, and add a control that signals a running scan (a shim
`xargs` that sleeps, then `kill -TERM`) and asserts a non-zero exit with no `ok` line.

## Warnings

### WR-01: Policy and pin gates are registered for `ci.yml` only; a second workflow file is unchecked

**File:** `tests/ci_gates.cmake:17,28`, `scripts/check-workflow.sh:4,39`, `docs/verify.md:68`

**Issue:** `ci.actions.pinned` runs `check-action-pins.sh .github/workflows/ci.yml`
and `ci.yml.policy` runs `check-workflow.sh .github/workflows/ci.yml`. The pin script
already scans every `.github/workflows/*.yml|yaml` when called with no arguments,
but the registered test passes an explicit file, so that mode is never exercised.
A PR that adds `.github/workflows/anything.yml` with an unpinned action,
`pull_request_target`, or `permissions: write-all` passes `CI required`.
`docs/verify.md:68` says the `workflow` family is "the policy gate over
`.github/workflows/*.yml`", which is not what runs. The ruleset requires zero
approvals and has no bypass actors, so this CI is the only review gate.

**Fix:** Run the pin gate with no argument so it covers every workflow file. Make
`check-workflow.sh` loop over every workflow file, or add a gate that fails when
`.github/workflows/` contains any file other than the expected set
(`workflow: unexpected-file: <name>`). Add a control that plants a second workflow
copy and expects the failure. Correct `docs/verify.md` to match.

### WR-02: Policy gate never inspects job-level `permissions`

**File:** `scripts/check-workflow.sh:127-144,152-184,230-231`

**Issue:** Only the top-level `permissions:` block is checked
(`perm_children != 1 || permline[1] != "contents: read"`). The job parser records
only `name`, `if`, `timeout-minutes` and `needs`. Reproduced: inserting
```yaml
    permissions:
      contents: write
      id-token: write
```
under the `hygiene` job of a copy of `ci.yml` still prints `workflow: ok (6 jobs)`.
Job-level permissions override the top-level default, so the "top-level is exactly
`contents: read`" rule can be defeated for any job without tripping the gate. The
stated purpose (header comment) is that "a later edit cannot quietly weaken the
required check".

**Fix:** In the job-key branch, capture `permissions` children per job and report
`workflow: job-permissions: <job>` unless the set is a subset of
`contents: read` and `pull-requests: read`. Add `permissions_job` to
`tests/ci/workflow_cases.cmake` and `tests/ci_gates.cmake`.

### WR-03: `non-preset-run` misses tools invoked by path

**File:** `scripts/check-workflow.sh:84,204-211`

**Issue:** `cmd_re` requires the command word to be preceded by a character outside
`[A-Za-z0-9_./-]`, which excludes `/`. Reproduced: `run: /usr/bin/cmake --preset hygiene`
in the `hygiene` job passes the gate (`workflow: ok (6 jobs)`), while
`... || true` is correctly refused. The exclusion exists to avoid matching names like
`foo.cmake`, but it also lets `/usr/local/bin/ctest`, `./ninja`, `sudo /usr/bin/make`
through the rule that is meant to force heavy lanes to run only
`cmake --workflow --preset <name>`.

**Fix:** Allow an optional path prefix: match
`(^|[^A-Za-z0-9_.-])([^[:space:]]*/)?(cmake|ctest|cpack|ninja|make)([^A-Za-z0-9_-]|$)`.
More robust: allowlist instead of denylist. For jobs in `heavy`, accept only the
exact preset line plus a short list of known setup lines (the `dpkg -s ... apt-get`
line), and report anything else as `non-preset-run`.

### WR-04: Core-purity gate has bypass shapes and passes when it scans nothing

**File:** `cmake/CheckCoreIncludes.cmake:22,32,47-63`

**Issue:** Three reproduced holes in the gate that backs D-05 (no host dependence in
the core):
- `#include "stdio.h"` passes (`core: ok`). A quoted include of a system header
  resolves through the system include path, and the comment "quoted in-tree includes
  are fine" is not checked against the tree.
- A line continuation (`#include \` newline `<stdio.h>`) passes.
- `-DDIRS=<empty dir>` and `-DDIRS=<nonexistent dir>` both print `core: ok`. If
  `src` or `include` is renamed or moved, `core.no_host_includes` keeps passing while
  scanning zero files; `core.nc.host_include` plants its own directory and would not
  notice. The glob also ignores `.inc`, `.def`, `.cc`, `.cpp`, `.S`.

**Fix:** Fail with `core: no-files: <dir>` when a directory has no matching file or
does not exist. Join continuation lines before matching, and treat a quoted include
as in-tree only if the file exists under the scanned directories. Defense in depth:
compile `gamebloke_core` with `-ffreestanding` and an include path restricted to
the freestanding headers, or add an `nm -u` check on the built archive. Add controls
for the quoted and continuation shapes.

### WR-05: Rename padding lets a truncated listing satisfy the completeness check

**File:** `scripts/classify-changes.sh:72-74` (listing built at `.github/workflows/ci.yml:56-58`)

**Issue:** The script treats `listed >= changed_files` as proof the listing is
complete, on the argument that "a complete listing is never shorter". The converse
does not hold: each renamed file contributes two names, so a listing missing real
entries can still reach the count. Reproduced: a PR of 3 files (a `.planning/` rename,
`.planning/b`, and `src/x.c`) whose listing is cut to the first three names
(`.planning/o`, `.planning/n`, `.planning/b`) yields `heavy=false`. The comment on
the script names exactly this failure (pagination cut short without an error) as the
fault being prevented.

**Fix:** Count file entries, not names. Emit one marker line per API entry from the
workflow, for example
`--jq '.[] | "F " + .filename, (if .previous_filename then "R " + .previous_filename else empty end)'`,
and have the classifier compare the number of `F ` lines with the count and test all
paths for the `.planning/` prefix. Add the rename-padding case to
`tests/ci/classify_cases.sh`.

### WR-06: `check-junit.sh` exclude pattern is an unanchored regex, unlike the preset's `^post$`

**File:** `scripts/check-junit.sh:66`, `cmake/GblTests.cmake:73`

**Issue:** The post step passes the literal `post` and the script tests
`parts[i] ~ exclude`, a substring regex match. Any label containing `post` (for
example a future `postgres`, `postprocess`, `post-link`) removes that test from the
`expected` set. The test still appears in the inventory (`listed`) so it is never
`extra`, and `if (!(name in expected)) continue` skips it, so a test that is
missing from the junit or skipped goes unreported. Reproduced: inventory
`b.t postgres`, junit without `b.t`, run with exclude `post` gives
`expected 1, ran 1` and exit 0. The ctest preset filters with the anchored `^post$`,
so the two sides disagree.

**Fix:** Pass `^post$` from `GblTests.cmake` (and the harness cases), or compare
labels with `==` instead of `~`. Add a control with a label that merely contains
`post`.

### WR-07: Credential-assignment rule is case-sensitive and misses compound names

**File:** `scripts/hygiene.sh:27,58`

**Issue:** `GBL_HY_CRED_ASSIGN` matches only lowercase `api_key|secret|token|password`
immediately followed by `:`/`=`. Reproduced: none of
`API_KEY=<32 chars>`, `AWS_SECRET_ACCESS_KEY = <32 chars>`, `secret_key = <32 chars>`,
`GITHUB_TOKEN: <32 chars>` is reported (`hygiene: ok`), while lowercase
`password = ...` is. The common environment-variable and `*_key` shapes are exactly
what ends up in checked-in notes and CI files. The `hygiene.nc.credential` control
only plants lowercase `api_key`, so it cannot detect this.

**Fix:** Make the assignment grep case-insensitive and allow name affixes, for
example `grep -n -i -E -e '[A-Za-z0-9_-]*(api[_-]?key|secret|token|passw(or)?d)[A-Za-z0-9_-]*[[:space:]]*[:=][[:space:]]*["'"'"']?[A-Za-z0-9/+_.-]{16,}'`,
keeping the property that the pattern text does not match its own source line.
Extend `hygiene_case.cmake` with an uppercase planted line.

### WR-08: `runner.version` and `runner.help` ignore the program's exit status

**File:** `tests/core.cmake:3-7,16-20`, `cmake/GblTests.cmake:40-41`

**Issue:** CTest ignores the exit code when `PASS_REGULAR_EXPRESSION` is set.
Reproduced with a scratch project: a test that prints the expected text and
`exit 7` reports `Passed`. The runner documents exit code 0 for `--version` and
`--help` (`runner/main.c:7`), and the sibling tests `runner.usage_error` and
`runner.no_args` do assert exit status via `ExpectFail.cmake`. A future regression
where `--version` prints then returns non-zero or crashes at exit is invisible.

**Fix:** Use `gbl_expect_pass` through a script-mode wrapper (the existing
`expect-pass` shape) so both exit status 0 and the regex are required, or add a
sibling test that runs the program bare and relies on the exit code.

## Info

### IN-01: Allowed-email suffix match has no left boundary

**File:** `scripts/hygiene.sh:25,56`

**Issue:** The allowance `grep -v -E "^N:.*(…|git@github\.com|noreply@github\.com)$"`
is applied to the matched token with a leading `.*`, so any local part ending in
`git` or `noreply` is allowed (reproduced: `digit@github.com` and
`someone-noreply@github.com` both pass). The header says the allowance applies to the
whole token; it applies to a token suffix.

**Fix:** Anchor the exact allowed forms: `^[0-9]+:(git@github\.com|noreply@github\.com|noreply@anthropic\.com|[A-Za-z0-9._%+-]+@(users\.noreply\.github\.com|example\.(com|org|net)))$`.

### IN-02: Core drops stack-protector and FORTIFY with no recorded rationale

**File:** `CMakeLists.txt:48-49`

**Issue:** `gamebloke_core` is built with `-fno-stack-protector -U_FORTIFY_SOURCE`.
The project's own rule is that ROM/BIOS/save data are untrusted input to this
component. Removing both mitigations from the component that will parse it is a
hardening regression, and the file gives no reason. If the intent is a freestanding
core with no `__stack_chk_fail`/`__*_chk` host symbols (D-05), that rationale and the
accepted risk belong in a comment or ADR; if not, the flags should go.

**Fix:** Add a comment naming the reason, or drop the options and rely on the
include-purity gate (see WR-04) for host independence.

### IN-03: Title length limit is counted in bytes

**File:** `scripts/check-title.sh:21,33-35`

**Issue:** With `LC_ALL=C`, `${#title}` counts bytes, but the message and header say
"characters". A 60-character title in a multibyte script is rejected as `too-long`.

**Fix:** Either say "bytes" in the reason text and header, or count characters with
`printf '%s' "$title" | LC_ALL=C.UTF-8 wc -m` where available.

### IN-04: `--repo` guard accepts `.`/`..` segments; the `main` ref is hardcoded

**File:** `scripts/apply-repo-settings.sh:68-75,166`

**Issue:** `--repo ../..` passes the validation (one slash, only allowed characters)
and becomes `repos/../../rulesets`. The guard's stated purpose is that the value
becomes part of an API path. Separately, the ruleset targets `~DEFAULT_BRANCH` but
the pre-apply check reads `commits/main/check-runs`, which is wrong if the default
branch is ever renamed.

**Fix:** Reject any segment that is `.` or `..` (`*/./*|*/../*|./*|../*|*/.|*/..|.|..`),
and resolve the default branch with `gh api repos/$repo --jq .default_branch`
or from the repo JSON via `RulesetCheck.cmake`.

### IN-05: `owner-local` with an `owner-pending` row reports a misleading reason

**File:** `cmake/CheckSignatureDigests.cmake:131-148`

**Issue:** When `GBL_OWNER_LOGO_FILE` is set but the committed row is
`owner-pending`, `row_digest` is empty and the check fails with
`owner-file-mismatch: the owner file's logo window digest is not the committed row`.
That is true but unhelpful for the exact moment the owner supplies a file for the
first time.

**Fix:** Add a distinct branch: if the row matches `^owner-pending`, print the
helper's digest line and fail with `owner-pending: replace the row in
tests/signatures/logo.sha256 with the printed line`.

### IN-06: Three-minute job timeouts leave thin margin over a handful of samples

**File:** `.github/workflows/ci.yml:33,63,75,104,124,143`

**Issue:** Each timeout is "2 x slowest" of two runs (5-36 s), rounded up to the 3
minute floor. A timeout cancels the job and turns `CI required` red; with no bypass
actor and no required approvals, an unlucky runner (cold macOS image, `apt-get
update && install clang-21` in the `asan` fallback at line 132) blocks merging until
a rerun. This is a judgment item, not a defect.

**Fix:** Keep the "2 x slowest" rule but recompute it from a larger sample (the
guidance in `AGENTS.md` already asks for measured critical-path review after each
run), and give the `asan` job extra headroom for the install fallback.

---

_Reviewed: 2026-11-09_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
