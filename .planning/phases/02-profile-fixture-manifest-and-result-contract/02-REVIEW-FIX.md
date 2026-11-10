---
phase: 02-profile-fixture-manifest-and-result-contract
fixed_at: 2026-11-10T00:00:00Z
review_path: .planning/phases/02-profile-fixture-manifest-and-result-contract/02-REVIEW.md
iteration: 1
findings_in_scope: 11
fixed: 11
skipped: 0
open: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-11-10
**Source review:** `.planning/phases/02-profile-fixture-manifest-and-result-contract/02-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 11 (CR-01, WR-01 to WR-09, and IN-04)
- Fixed: 11
- Skipped: 0
- Open: 0 (IN-01, IN-02, IN-03, IN-05 are out of scope by instruction; only the cheap `curl -q` part of IN-01 fell out of WR-01)

Every control below was shown to fail when its fix (or the pinned rule it covers) was
reverted, by mutating a scratch copy of the repository outside the tree; no mutation was
committed. Findings that touch the same validator, case driver and inventory block were
committed together, and the commit body says why.

## Per-finding results

| Finding | Status | Commit | Controls added or strengthened | Notes |
| --- | --- | --- | --- | --- |
| CR-01 | fixed: requires human verification | `af256ce` | new `contract.nc.semicolon_item` (all three arrays, a non-string item, a `;` state name) | Any array element holding `;` is `contract: missing-field`; state names must match `[A-Za-z0-9_]+` (`bad-state-rule`), so a `;` name no longer dies with a raw CMake error. Logic fix, so flagged for a human read. |
| WR-01 | fixed | `a2c5163` | new `fetch.nc.bad_status`, `fetch.nc.empty_response` | Route taken: honest header, no redirect following. An offline redirect control needs an HTTP server, which the no-network rule forbids. The inert `--proto-redir` and `--max-redirs` were removed; a non-2xx https status is `bad-status` and an empty download is `empty-response`; `curl -q` added (cheap IN-01 part). `bad_status` uses a fake curl that answers 302 with a body. A pin must name the direct URL (documented in `pins.txt` and `docs/evidence-contract.md`), so a redirecting release-asset URL cannot be pinned. |
| WR-02 | fixed | `a2c5163` | new `fetch.nc.symlink_new_subdir` | The deepest existing ancestor of the destination parent is resolved before `mkdir -p`; the old post-mkdir check stays as defence in depth. The control asserts nothing was created outside the root, for a symlink to a directory and for a dangling symlink. |
| WR-03 | fixed | `414cd02` | new `manifest.nc.rights_mismatch`, `manifest.pos.public_ci_ok`; extended `manifest.nc.bad_url`, `manifest.nc.pin_cross_check` (pins userinfo) | New reason code `rights-mismatch` (`public-ci` needs `redistributable: yes` and no `user-official` BIOS). Userinfo before the host is `bad-url` in both manifest and pins. The control inputs build the userinfo string at run time so no committed file holds a credential-shaped string. |
| WR-04 | fixed | `ebba142` | new `result.nc.manifest_digest` | A non-null `fixture.rom_sha256` must equal the `source.sha256` of a `url` or `in-tree` row (`claim-mismatch`). Git rows pin a commit, not a digest, so no tie exists for them; stated in `docs/evidence-contract.md`. The control also proves the equal case is accepted. |
| WR-05 | fixed | `414cd02` | new `manifest.nc.pin_cross_check`; positive `manifest.pos.public_ci_ok` (including the `raw.githubusercontent.com` form) | `url` row: pinned URL equals `source.url`. `git` row: pinned URL contains the commit and sits on the `source.url` host (`raw.githubusercontent.com` counts as `github.com`) under its repository path (a longer name with the same prefix is refused). Orphan pins are `pin-mismatch`. Deliberate departure from the review text: a literal "starts with `source.url`" rule would refuse the normal raw-host URL form, so the host-plus-path rule was used. A `git` or `url` row with no `fetch` stays allowed and is documented. |
| WR-06 | fixed | `af256ce` | new `contract.nc.weakened_state_rule` (eight variants) | Each locked PASS, MISSING, SKIPPED and REFUSED rule is weakened to an individually valid value and the pinned-rule finding text is asserted. Mutating each pin in `CheckContract.cmake` turns it red. |
| WR-07 | fixed | `e074095` | strengthened `profile.nc.timing_arithmetic` (one variant per invariant, detail asserted), `profile.nc.unmodeled_bad_entry` (`hblank-flag-edge` dispute variant) | Disabling any one of the five invariants or the dispute rule now fails a control. |
| WR-08 | fixed | `414cd02`, `ebba142`, `e074095` | strengthened `result.nc.claim_mismatch` (no evidence item names the fixture), `result.nc.missing_digest` (malformed optional digest), `result.nc.bad_evidence` (item without `fixture`), `profile.nc.bad_source_label` (null under a non-UNK label, two variants), `manifest.nc.wrong_type` (non-object `fetch`, three variants) | One variant per listed trigger, each with the finding detail asserted. |
| WR-09 | fixed | `a2c5163` | new `fetch.nc.no_cmake`, `fetch.nc.digest_tool_failed` | New codes `no-cmake` (exit 2) and `digest-tool-failed` (failing tool, silent tool, short output). Listed in the script header and in `docs/verify.md`. |
| IN-04 | fixed | `16c843d` | none (data label; `registry.lint` re-ran clean) | `disp-dma-latch` source 3 is now `INF`, with the note naming it an inference from a partial read of the DMA section. |

## Other commits

- `ab254ae`: the review report quoted a credential-shaped URL as a reproduction, which made
  `sh scripts/hygiene.sh --tree` fail with `hygiene: email`. The example was reworded in
  prose; the finding itself is unchanged.
- `5288021`: the five new `fetch.nc.*` inventory lines were first written with unsorted labels
  in `a2c5163`; the configure-time inventory compare rejected them. Corrected here.

## Verification

All runs were in the main checkout on the local phase branch (no worktree;
`workflow.use_worktrees` is false), on the final tree.

| Run | Result |
| --- | --- |
| `cmake --workflow --preset ci` (default CMake) | exit 0; `inventory: expected 263, ran 261, skipped 2 (allowed 2)` (was 252/250/2) |
| `cmake --workflow --preset hygiene` | exit 0; `inventory: expected 193, ran 191, skipped 2 (allowed 2)` (was 182/180/2) |
| asan lane | not re-run; unaffected (no asan test changed; stays 16/16/0) |
| Kitware CMake 3.25.3 floor, fresh `build/ci`, `--workflow --preset ci` | exit 0; 263 of 263 passed; `inventory: expected 263, ran 261, skipped 2 (allowed 2)` |
| `sh scripts/linux-lane.sh ci gcc` | exit 0 (Docker, aarch64 container, gcc); 100% of the ci tests passed; `inventory: expected 263, ran 261, skipped 2 (allowed 2)` |
| `sh scripts/hygiene.sh --tree` | `hygiene: ok (145 files)` |
| `sh scripts/signature-scan.sh --tree` | `signature: ok (145 files)` |
| `sh scripts/check-traps.sh` | `traps: ok (17 files)` |
| `shellcheck scripts/fetch-pin.sh` | clean |

## Logic fixes that deserve a human read

CR-01 and the cross-check rules (WR-03, WR-04, WR-05) change what the validators accept.
The controls prove each rule fires and that the committed examples and positives still
pass; they cannot prove the rules are the right ones. The one deliberate departure from
the review text is the host-plus-path rule in WR-05 (see its row above).

---

_Fixed: 2026-11-10_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
