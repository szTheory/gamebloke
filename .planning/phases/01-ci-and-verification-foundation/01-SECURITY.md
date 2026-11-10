---
phase: "01"
slug: ci-and-verification-foundation
status: verified
threats_open: 0
asvs_level: 1
created: "2026-11-10"
---

# Phase 01 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Repository content to public GitHub | Everything tracked is published (public repo) | Source, planning docs; must carry no credentials, home paths, non-noreply emails, or rights-bearing bytes |
| Pull request metadata to workflow | PR titles, branch names, and file listings are attacker-influenced | Strings reaching `run:` blocks and the classifier |
| Workflow files to the required check | A PR can edit workflows and gate scripts | Gate logic integrity |
| Local machine to GitHub API | `apply-repo-settings.sh` and `gh` use owner credentials | Ruleset writes (the only authorized write) |
| Owner-held files to the signature helper | Rights-bearing files stay outside the repo | A digest only, never bytes |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-01-01 | Repudiation | scripts/check-junit.sh, CTest skip/disabled states | high | mitigate | Post step fails on missing/extra/duplicate/failed/disabled/skipped-not-allowed/skipped-no-reason/empty/no junit; each shape has an `inventory.nc.*` control on a test-time junit shape | closed |
| T-01-02 | Tampering | cmake/ExpectFail.cmake | high | mitigate | Wrapper fails on exit 0 and on wrong reason; `nc.meta.passing_gate_fails_wrapper` proves the wrapper fails when the gate passes; positive controls prevent a gate that fails everything from satisfying its control | closed |
| T-01-03 | Tampering | test names parsed by awk from junit | low | mitigate | gbl_add_test rejects names outside `^[a-z0-9][a-z0-9._-]*$` (`gbl_add_test: bad-name`), so attributes need no XML unescaping | closed |
| T-01-04 | Information disclosure | tracked files (.planning/ prose, scripts) | high | mitigate | hygiene.tree in both `ci` and `hygiene` lanes; findings print path:line only; controls for credential, email, home-path, identity | closed |
| T-01-05 | Denial of service | GitHub push protection on planted fixtures | low | mitigate | Planted strings built from fragments and string(RANDOM) at test time under build/; no realistic literal committed (git grep acceptance check) | closed |
| T-01-06 | Tampering | file names with newlines or leading dashes | medium | mitigate | `git ls-files -z` with `xargs -0`, `find -exec ... {} +`, `--` before paths; control hygiene.nc.newline_name | closed |
| T-01-07 | Elevation of privilege | developer bypasses the hook with --no-verify | low | accept | Hook is a convenience; the CI hygiene job is the enforcing gate; hygiene.hook and its three controls keep the hook present, executable, and calling `--staged` | closed |
| T-01-08 | Information disclosure (rights exposure) | committed files (ROM, BIOS, logo-bearing binaries, saves) | high | mitigate | signature.tree in `ci` and `hygiene` lanes with logo-window, bios-digest, rom-extension, binary-not-allowlisted rules, each with a synthesized control; .gitignore already lists ROM/save extensions | closed |
| T-01-09 | Elevation of privilege | tests/signatures/allowlist.txt | high | mitigate | Allowlist exempts only extension and binary rules; logo and BIOS controls allowlist their planted file and still must fail | closed |
| T-01-10 | Spoofing | committed digest lists (wrong or missing digest gives false assurance) | medium | mitigate | signature.digests.wellformed with one control per failure reason (six `signature.nc.digests_*`) and a sandbox-copy positive; owner-pending row reported as SKIPPED, never PASS; owner-local cross-check with a mismatch control and a match positive | closed |
| T-01-11 | Information disclosure | scan and helper output in CI logs | medium | mitigate | Scripts print paths and digests only; acceptance check confirms no window hex dump | closed |
| T-01-12 | Tampering | crafted file without NUL bytes evading binary detection | medium | mitigate | Logo window hashed for every file of at least 160 bytes, not only binaries | closed |
| T-01-13 | Tampering (false green) | asan preset flags | high | mitigate | -fno-sanitize-recover=all in the preset; controls require non-zero exit plus reason text; Pitfall 2 negative check recorded | closed |
| T-01-14 | Tampering | ubuntu:26.04 image used by scripts/linux-lane.sh | medium | mitigate | Image pinned by digest; read-only source mount; local developer tool only, never in CI | closed |
| T-01-15 | Elevation of privilege | ci.yml run steps (script injection) | high | mitigate | PR values only via `env:` or gh; no `${{ }}` inside `run:` (static check with control in plan 07); no pull_request_target; no secrets | closed |
| T-01-16 | Tampering | third-party actions | high | mitigate | Full 40-hex SHA pins with version comments here; plan 07 adds check-action-pins.sh in ci and hygiene lanes with two controls | closed |
| T-01-17 | Repudiation (false green) | CI required aggregation | high | mitigate | if: always(); heavy job skipped when HEAVY is not exactly false fails; ci.required.cases covers it | closed |
| T-01-18 | Tampering | classifier fed a failed, truncated, or short listing | high | mitigate | `(listing failed)`, >3000 files, an empty listing, and fewer listed names than changed files each force heavy=true; ci.classify.cases covers each | closed |
| T-01-19 | Elevation of privilege | GITHUB_TOKEN scope | medium | mitigate | Top-level contents: read; pull-requests: read only on `changes` and `title`; persist-credentials: false on every checkout | closed |
| T-01-20 | Tampering | .github/workflows/ci.yml edits that weaken gates or unpin actions | medium | mitigate | check-action-pins.sh and check-workflow.sh in ci and hygiene lanes with twelve mutation controls (two pin, ten policy) and a clean-copy positive; pull_request runs have a read-only token; review is the final control | closed |
| T-01-21 | Elevation of privilege | ruleset bypass actors | high | mitigate | `bypass_actors: []` enforced by ci.ruleset.wellformed and ci.ruleset.nc.bypass; bypass_actors compared in full by canon, so remote drift is caught by `--check` (ci.repo_settings.nc.ruleset_drift) | closed |
| T-01-22 | Elevation of privilege | scripts/apply-repo-settings.sh applying unauthorized settings | high | mitigate | Script writes only the ruleset; validator rejects extra rule types; other settings are advisory output only; stub-gh controls prove the refusal path makes no write | closed |
| T-01-23 | Elevation of privilege | PR title in the title job | high | mitigate | Title read with gh into a variable, passed as a quoted argument, matched with grep -E; never evaluated or interpolated into run | closed |
| T-01-24 | Information disclosure | 01-EVIDENCE.md, PR bodies, commit history | high | mitigate | hygiene --tree before push; instructions forbid local absolute paths and personal emails; the CI hygiene job scans the PR | closed |
| T-01-25 | Tampering | Kitware CMake 3.25.3 tarball | medium | mitigate | SHA-256 verified against Kitware's published list; extracted outside the repository; used once locally; never in CI | closed |
| T-01-26 | Repudiation | merging with unverified CI | high | mitigate | Merge only after a green run whose jobs are all success; bootstrap stop rule if no run appears; no admin bypass | closed |
| T-01-27 | Tampering | local main reset after squash | medium | mitigate | Tree-equality check before reset; local backup ref; stop on difference | closed |
| T-01-28 | Denial of service | ruleset applied before any CI required run | medium | mitigate | `--apply` refuses until a successful `CI required` check run exists on main; ci.repo_settings.nc.no_ci_required_run proves it offline | closed |
| T-01-29 | Denial of service | GSD direct-to-main commits after the ruleset | medium | mitigate | branching_strategy switched to `phase` through PR A before the ruleset matters; close-out work moves to the phase branch | closed |
| T-01-30 | Elevation of privilege | applying settings beyond the ruleset | high | mitigate | apply-repo-settings.sh writes only the ruleset; acceptance checks confirm squash title and sha_pinning_required unchanged | closed |
| T-01-31 | Repudiation | a .planning/-only PR passing while heavy lanes silently ran or failed | medium | mitigate | per-job conclusions recorded; merge blocked if heavy lanes ran; aggregator unit-tested in plan 06 | closed |
| T-01-32 | Information disclosure (rights exposure) | owner-held ROM or logo-bearing file | high | mitigate | Owner runs the helper locally; agent never touches the file; helper prints only the digest; acceptance check confirms no ROM-like file is tracked | closed |
| T-01-33 | Spoofing | a wrong or mistyped digest giving false assurance | medium | mitigate | Strict line format validation; optional owner-local cross-check test; evidence records the method and that a hash proves identity only | closed |
| T-01-34 | Repudiation | stale build/ci/junit.xml; post-step output hidden on a passing run | high | mitigate | junit deleted at configure (Pitfall 5); `ci-post` is verbose so the inventory summary and SKIPPED lines appear in every green log; acceptance mutation proves the summary disappears without it | closed |
| T-01-35 | Elevation of privilege | publishing the close-out branch through a third PR beyond the two-PR authorization | medium | mitigate | blocking-human checkpoint:decision (Task 3) with no auto_select; plan 11 opens the PR only on `close-out-pr`; the inactive real-logo rule on main is recorded either way | closed |
| T-01-36 | Repudiation | close-out branch merged without a green `CI required`, or local main reset over unmerged work | medium | mitigate | Task 3 merges only through the ruleset with `gh pr merge --squash` (no admin), watches the main push run, and resets local main only after a tree-equality check and a backup ref | closed |
| T-01-37 | Repudiation | `--check` reporting false drift after a correct apply (server-filled defaults), or masking real drift | medium | mitigate | canon projects the remote onto the committed file's keys and compares rule types, bypass actors, ref lists, and required checks in full; ci.repo_settings.pos.check_server_defaults and ci.repo_settings.nc.ruleset_drift prove both directions offline | closed |
| T-01-SC-01 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages; CMake, Ninja, and Apple clang are already present | closed |
| T-01-SC-02 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages | closed |
| T-01-SC-03 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed in this plan | closed |
| T-01-SC-04 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed in this plan | closed |
| T-01-SC-05 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages; apt packages come from the Ubuntu archive inside a throwaway container | closed |
| T-01-SC-06 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages; actions pinned by SHA verified against tags in research | closed |
| T-01-SC-07 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed | closed |
| T-01-SC-08 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed | closed |
| T-01-SC-09 | Tampering | npm/pip/cargo installs | high | mitigate | PyPI cmake wheel forbidden; floor check uses the Kitware tarball by SHA-256 | closed |
| T-01-SC-10 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed | closed |
| T-01-SC-11 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages installed | closed |

*Status: all 48 threats closed (37 STRIDE ids T-01-01..T-01-37 and 11 supply-chain rows T-01-SC-01..SC-11).*
*Verified by the security auditor on 2026-11-10 against the branch tip including the review fixes (CR-01, WR-01..WR-08). Evidence per threat is in the auditor verdict summarized in 01-REVIEW-FIX.md and the plan SUMMARY files.*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-01 | T-01-07 | A developer can bypass the opt-in pre-commit hook with `--no-verify`. The CI `hygiene` job is the enforcing gate; the hook's wiring is guarded by `hygiene.hook` and its controls. | Plan 01-03 threat model (disposition accept) | 2026-11-09 |

*Accepted risks do not resurface in future audit runs.*

### Residual notes (not register items)

- **Real-logo rule inactive on origin/main (human-needed, EVID-04):** `tests/signatures/logo.sha256` holds an `owner-pending` row until the owner supplies a digest. `signature.logo.real_digest` reports SKIPPED with its reason; the extension, BIOS-digest, and binary-not-allowlisted rules still apply, and synthesized controls prove the logo rule works. Owner: repository owner. Next action: run `scripts/signature-digest.sh logo <file>` on a file they hold outside the repo and commit the printed line through a PR.
- **IN-01 (info, unfixed):** the allowed-email suffix in `scripts/hygiene.sh` has no left boundary, so an address such as `x-noreply@github.com` would pass. Accidental-leak guard only.
- **WR-05 live proof pending:** the classifier's `F`/`R` markers in `ci.yml` are covered by local case tables but have not yet run on GitHub (the review fixes are unpushed). The first PR carrying them is the live proof.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-10 | 48 | 48 | 0 | gsd-security-auditor |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-11-10
