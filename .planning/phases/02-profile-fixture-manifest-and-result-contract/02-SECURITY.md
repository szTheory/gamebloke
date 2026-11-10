---
phase: "02"
slug: profile-fixture-manifest-and-result-contract
status: verified
threats_open: 0
asvs_level: 1
created: "2026-11-10"
---

# Phase 02 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Manifest, `pins.txt`, and JSON authority files to validators | Hand-edited files are untrusted input to the CMake validators | Paths, URLs, digests, labels, reason-relevant fields |
| `pins.txt` row to `scripts/fetch-pin.sh` | URL, destination, and digest fields reach a download tool and the filesystem | Network bytes, destination paths under `.fixtures/` |
| Fetched fixture to working tree | Third-party bytes placed in an untracked directory | Rights-bearing or untrusted binaries (never committed) |
| Result documents to the result validator | Runner output (Phase 4) must not claim more than evidence supports | Claim tuple, state, digests, dispute disclosure |
| Public repository content | Everything tracked is published | No credentials, home paths, non-noreply emails, or rights-bearing bytes |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-02-01 | Tampering | `string(JSON)` leniency in every validator | high | mitigate | gbl_json_load strictness guard rejects comments, trailing commas, trailing content (structural root walk, not a bracket-class regex), and duplicate keys; one control per code (json.nc.*) mutating a copy of the real vocabulary file; trailing-content tails are also appended to every real committed JSON document | closed |
| T-02-02 | Repudiation | a validator that passes vacuously (empty or absent input, unmutated control) | high | mitigate | absent, empty, and whitespace-only files fail (`json: unreadable`, `json: not-json`); mutation helpers fail with `mutate: no-match` (json.nc.mutate_no_match); positive controls json.pos.strict_clean, json.pos.real_docs (with a printed document count each plan asserts), and contract.pos.copy | closed |
| T-02-03 | Information disclosure | parser error text on CMake 4.x echoes the input | medium | mitigate | findings print `<family>: <code>: <path>` only; json controls plant a fragment-built marker that occurs in no reason code or path and assert with gbl_assert_no_echo that captured output lacks it; json.nc.no_echo_guard proves the assertion can fail | closed |
| T-02-04 | Tampering | weakening the state table (PASS without run) by editing data | high | mitigate | CheckContract pins the nine names and the locked rules as an oracle (`contract: bad-state-rule`, `contract: state-set`), with controls | closed |
| T-02-05 | Tampering | a seed entry silently dropped or renamed | high | mitigate | seed ids pinned in CheckRegistry (`registry: seed-missing`), control removes each seed in turn | closed |
| T-02-06 | Repudiation | a community or emulator claim presented as hardware fact | medium | mitigate | every source carries one vocabulary label; issue and errata hardware claims recorded as UNK in notes; status limited to `open`; label controls | closed |
| T-02-07 | Information disclosure (rights) | quoting GBATEK prose or GPL/MPL source into the repository | medium | mitigate | registry cites section names, paths, commits, and issue numbers only; reviewed in the task acceptance and by the hygiene/signature scans for byte-level content | closed |
| T-02-08 | Repudiation | profile claiming fidelity it lacks (unlabeled constant, hard-coded HBlank edge, dropped unmodeled item) | high | mitigate | per-constant source and cite (`missing-source`, `bad-source-label`), null HBlank edge (`hblank-edge-hardcoded`), pinned unmodeled ids (`unmodeled-missing`), each with a control | closed |
| T-02-09 | Tampering | doc and JSON drifting apart | medium | mitigate | doc-sync codes `doc-missing-id`, `doc-extra-id`, `doc-version-mismatch`, `doc-no-section` with controls; the empty-section case fails rather than passing | closed |
| T-02-10 | Tampering | BIOS provider grammar bypass (embedded newline, empty `hle:` list, short digest, retired M0 label) | high | mitigate | whole-string anchored regexes in gbl_bios_provider_valid (anchors probed against newline variants in RESEARCH); bad_bios_provider control loops seven shapes | closed |
| T-02-11 | Tampering | git source pinned to a branch, `HEAD`, tag, or short SHA | high | mitigate | 40-lowercase-hex commit required (`manifest: unpinned-revision`), control loops six shapes; tag accepted only as an extra field | closed |
| T-02-12 | Tampering | committed pins row using `http://` or `file://` (downgrade or local read) | high | mitigate | `pins: bad-url` and `manifest: bad-url` require `https://`; controls for http, file, and quote shapes | closed |
| T-02-13 | Tampering, Elevation | path traversal through pins dest or manifest path (`../`, absolute) | high | mitigate | allow-list charset plus per-component `.`/`..`/empty rejection (`pins: bad-dest`, `manifest: bad-path`), controls; plan 05 repeats the check at fetch time | closed |
| T-02-14 | Repudiation | zero-row manifest or empty pins file passing vacuously | medium | mitigate | linter prints the row count; manifest.pos.complete_row and manifest.pos.source_kinds prove rows are accepted; every row-level code has a rejecting control (R-4); an empty manifest file fails in the loader (`json: not-json`); a zero-byte or header-less pins.txt fails with `pins: missing-header` (pins.nc.missing_header, W7) | closed |
| T-02-15 | Repudiation | a disputed expected value recorded under a non-disputed class | high | mitigate | `dispute-class-mismatch` both directions and `dispute-unknown`, with controls | closed |
| T-02-16 | Tampering | unstable GitHub archive bytes under a pin (Pitfall 9) | medium | mitigate | git rows with pin_dest must have the commit in the pinned URL (`manifest: pin-mismatch`); pins.txt header states the single-raw-file or release-asset rule | closed |
| T-02-17 | Tampering, Elevation | path traversal through `<dest>` (`../../.git/hooks/pre-commit`, absolute path) | high | mitigate | check_dest allow-list and per-component rejection on the requested and row dest (`bad-dest`), `pwd -P` containment and symlink refusal (`symlink`), `dest-is-dir`; controls for traversal, absolute, `.` component, space, symlinked parent, symlink at destination, directory | closed |
| T-02-18 | Tampering | command or option injection via URL or dest | high | mitigate | scheme prefix required (so a value cannot start with `-`), whitespace, quote, and backslash rejected, URL passed with `--url`, `set -f`, quoted expansions, no evaluation of row text; bad_url control loops four shapes | closed |
| T-02-19 | Tampering | substituted or tampered download | high | mitigate | digest compared before `mv`; mismatch exits non-zero loudly with nothing placed (fetch.nc.digest_mismatch asserts no destination and no temp dir) | closed |
| T-02-20 | Tampering, Information disclosure | downgrade or redirect to http or file | high | mitigate | `--proto` and `--proto-redir` restricted to the row's single scheme; `file://` only with GBL_FETCH_ALLOW_FILE=1 (control proves refusal without it); certificate checks left on | closed |
| T-02-21 | Tampering, Denial of service | partial download or signalled run leaving a file or printing ok | medium | mitigate | temp dir inside the root and rename only after hashing; Phase 1 trap idiom enforced by traps.static | closed |
| T-02-22 | Denial of service | oversized or hanging download | low | mitigate | `--max-filesize 268435456`, `--max-time 600`, `--connect-timeout 20`, `--max-redirs 5` | closed |
| T-02-23 | Information disclosure (rights) | a fetched ROM or BIOS committed | high | mitigate | `/.fixtures/` ignored; fetch.untracked gate with not-ignored, tracked, and git-failed controls; Phase 1 signature scan still catches a forced add elsewhere | closed |
| T-02-24 | Tampering | tests reaching the network | medium | mitigate | every control uses a synthesized file under the build dir through `file://`; no https row exists in any test sandbox (D-16) | closed |
| T-02-25 | Repudiation | a fixture that did not run, or ran without a digest, reported as PASS | high | mitigate | `pass-without-run`, `pass-without-digest`, `ran-state-mismatch` driven by contract rows, each with a control; the contract oracle (plan 01) prevents weakening the rows | closed |
| T-02-26 | Repudiation | a disputed expected value presented as settled | high | mitigate | `undisclosed-dispute`, `label-without-dispute`, `unknown-dispute` with controls; result.pos.disputed proves the honest form passes | closed |
| T-02-27 | Tampering | claim tuple disagreeing with the fixture (different ROM digest, foreign evidence class) | medium | mitigate | `claim-mismatch` and `bad-evidence` cross-checks against the fixture block and the manifest row, with controls | closed |
| T-02-28 | Spoofing | retired or free-form BIOS labels (M0/M1, `none ` with whitespace) presented as a provider | medium | mitigate | gbl_bios_provider_valid anchored grammar; five-variant control and five-form positive | closed |
| T-02-29 | Tampering | substituted floor CMake tarball | high | mitigate | SHA-256 compared against Kitware's published list and the Phase 1 recorded digest before extraction; extracted outside the repository; stop on mismatch; never PyPI | closed |
| T-02-30 | Information disclosure | local paths, scratch locations, or addresses in 02-EVIDENCE.md, docs, or the SUMMARY | medium | mitigate | commands written with placeholders; `hygiene.sh --dir` on the phase directory and `--tree` on the repository in the verify commands | closed |
| T-02-31 | Denial of service | the larger suite exceeding the 3-minute CI job timeouts and failing the required check | medium | mitigate | local workflow and slowest-test times recorded against the timeout with a 90-second flag, a stated reduction step, and the escalation that the orchestrator may raise `timeout-minutes` in the phase PR (I5); no plan edits ci.yml | closed |
| T-02-32 | Repudiation | docs drifting from validator reason codes | low | mitigate | Task 1 verify greps representative codes from every family; the executor diffs every header list against docs/verify.md | closed |
| T-02-SC-01 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages; CMake and POSIX sh are already present | closed |
| T-02-SC-02 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages | closed |
| T-02-SC-03 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages | closed |
| T-02-SC-04 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages | closed |
| T-02-SC-05 | Tampering | curl added to the Linux-lane container | high | mitigate | installed from the Ubuntu archive inside the digest-pinned ubuntu:26.04 image, like the existing packages; no npm/pip/cargo install; no new host dependency (curl already present on macOS and GitHub runners, RESEARCH A1 checked on the phase PR run) | closed |
| T-02-SC-06 | Tampering | npm/pip/cargo installs | high | mitigate | This plan installs no registry packages | closed |
| T-02-SC-07 | Tampering | npm/pip/cargo installs | high | mitigate | No registry packages; the only download is the SHA-256-verified Kitware tarball used locally once | closed |

*Status: all 39 threats closed (32 STRIDE ids T-02-01..T-02-32 and 7 supply-chain rows T-02-SC-01..SC-07).*
*Verified by the security auditor on 2026-11-10 against the branch tip including the review fixes (CR-01, WR-01..WR-09, IN-04), with ci 263/261/2 and hygiene 193/191/2 re-run in a scratch clone.*

---

## Accepted Risks Log

No accepted risks.

### Residual notes (not register items)

- **RESEARCH A1 pending:** whether curl is present on the GitHub `ubuntu-26.04` runner is unverified. `scripts/fetch-pin.sh` exits with `fetch-pin: no-curl` if it is absent, so the gap fails loudly. Owner: orchestrator, checked on the phase PR's first hosted run.
- **Redirects are not followed (WR-01):** stricter than the plan text. Release-asset URLs that redirect cannot be pinned; documented in `pins.txt` and `docs/evidence-contract.md`.
- **SC5 producer half:** the validator rule for disclosing disputes is delivered; results are produced by the runner in Phase 4.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-11-10 | 39 | 39 | 0 | gsd-security-auditor |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-11-10
