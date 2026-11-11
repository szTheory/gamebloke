---
phase: "03"
slug: "bus-api-memory-map-and-rom-loading"
status: verified
threats_open: 0
asvs_level: 1
block_on: high
created: "2026-11-10"
---

# Phase 03 — Security

## Trust Boundaries

| Boundary | Description | Data Crossing |
|-----------|-------------|---------------|
| Host descriptor to core | The host supplies native allocator callbacks and their context. They are trusted same-process code; this API does not sandbox an incorrect or malicious host. | Callback pointers, context, caller-owned storage |
| ROM image to core | The caller supplies a readable byte span. The core checks declared bounds before reading or allocating and copies the bytes into owned storage. Guest bytes do not supply callback pointers or context. | Untrusted image bytes and declared length |
| Core to caller | Refusal status and bounded diagnostic enums are returned without retaining title or game-code strings. | Status, timing results, bounded diagnostics |

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation / scope evidence | Status |
|-----------|----------|-----------|----------|-------------|----------------------------|--------|
| T-03-01 | Tampering | bus requests and clock | high | mitigate | `src/bus.c` validates width, class, address span and cycle overflow before effects; `tests/bus_test.c` compares canonical state after refusal and overflow. | closed |
| T-03-02 | Tampering | machine initialization and reset | high | mitigate | `src/machine.c` explicitly zeroes deterministic state and keeps no mutable production globals; state and interleaving tests verify independent machines and reset. | closed |
| T-03-03 | Denial of service | bounded bus access | medium | mitigate | Invalid arguments return bounded error results without guest-controlled allocation; timing, decode and mutation tests exercise rejection paths. | closed |
| T-03-04 | Tampering | profile policy | high | mitigate | `cmake/CheckProfile.cmake` validates policy fields and citations; semantic profile mutations and clean controls run in CI/hygiene. | closed |
| T-03-05 | Repudiation | documented machine policy | medium | mitigate | Profile policy and `docs/profile-agb.md` distinguish project timing/fill/latch choices from hardware facts. | closed |
| T-03-06 | Tampering | ROM length validation and copy | high | mitigate | `src/loader.c` validates ordered bounds before allocation or image reads; loader tests check boundaries and canaries. | closed |
| T-03-07 | Denial of service | allocation and replacement | high | mitigate | Candidate allocation is bounded, resource failure is distinct, and successful replacement installs/reset the candidate before releasing old storage. | closed |
| T-03-08 | Tampering | refusal atomicity | high | mitigate | Canonical-state, diagnostics, ROM-content and caller-buffer checks cover refusal and allocation failure on fresh and loaded machines. | closed |
| T-03-09 | Elevation of privilege | host allocator contract | low | scope assumption | Callback pointers/context are copied only from the host descriptor (`src/machine.c`); guest ROM supplies bytes and bounded diagnostics only (`src/loader.c`). Initialization checks callback pairing/storage placement, loading validates inputs and callbacks before allocation, and docs/tests establish trusted, disjoint, non-reentrant host obligations. An incorrect native host can still violate its own process state; this core is not a sandbox. This refines the plan-time `accept` label based on audited provenance and does not claim owner risk acceptance. | closed — boundary verified |
| T-03-10 | Tampering | memory-map aliases | high | mitigate | `src/bus.c` maps bounded ROM extents without image-size modulo; bus tests and mirror mutation controls cover boundaries. | closed |
| T-03-11 | Tampering | video memory translation | high | mitigate | `src/bus.c` translates mirrored VRAM before mode thresholds and bounds byte duplication; mode tests and semantic mutations exercise the behavior. | closed |
| T-03-12 | Information disclosure | open-bus and fallback reads | medium | mitigate | `src/machine.c` initializes the latch; `src/bus.c` bounds fallback lanes and reports distinct outcomes without raw-input logging. | closed |
| T-03-13 | Tampering | fixed-header parsing | high | mitigate | `src/loader.c` reads fixed offsets only after minimum-length validation and commits candidate diagnostics transactionally. | closed |
| T-03-14 | Information disclosure | loader diagnostics | medium | mitigate | Diagnostics are bounded enums/by-value fields; caller-controlled title/game-code strings and output pointers are not retained. | closed |
| T-03-15 | Repudiation | canonical-logo claims | high | mitigate | Real-logo result remains `UNCHECKED`/`OWNER_PENDING`; synthetic checks are labeled as mechanics only, with explicit partial acceptance in docs. | closed |
| T-03-16 | Repudiation | phase evidence and test inventories | high | mitigate | The linked integration test, inventory post-checks and `03-EVIDENCE.md` record current commands, results, skips and claim limits. | closed |
| T-03-17 | Tampering | verification lanes and controls | medium | mitigate | Sanitizer flags and no-tests-error policy are configured; mutation controls distinguish semantic failures from infrastructure errors and have clean companions. | closed |
| T-03-SC | Tampering | dependency and package boundary | high | mitigate | No dependency, install rule, external acquisition or CI/preset compiler mapping was added; the in-tree C header makes no stable embedding ABI promise. | closed |
| T-03-18 | Tampering | candidate digest and commit | high | mitigate | `src/loader.c` validates bounds/callbacks and copies the candidate before fixed-window hashing and diagnostic commit; `src/logo_sha256.h` uses unsigned operations, bytewise assembly, fixed 156-byte input and 1248-bit padding; independent vectors and integrated boundary/refusal tests pass. | closed |
| T-03-19 | Tampering | digest configure metadata | high | mitigate | Fixed authority path and configure dependency in CMakeLists; `cmake/ReadLogoDigest.cmake` validates one exact pending/64-hex row without echo; malformed and disabled-parser controls pass. | closed |
| T-03-20 | Repudiation | synthetic/canonical distinction | high | mitigate | Shared source/options for both core targets; integrated synthetic witness and bypass/forced-match mutants with clean companions; docs preserve actual owner-pending acceptance and no rights/identity inference. | closed |
| T-03-21 | Repudiation | current sanitizer membership | high | mitigate | Fresh installed-Clang build, retained compile commands/JUnit and current-inventory reconciliation prove 42 ran with zero skips, including both ownership and digest controls. | closed |

## Accepted Risks Log

No risks have been accepted by the owner. T-03-09 is recorded as a verified trusted-host scope assumption, not an accepted risk. The remaining host preconditions are explicit: callback validity, readable input extent, sufficient fresh disjoint storage, and callback non-reentrancy. Violations can corrupt or mismanage the host's own process state; the core does not promise to contain them.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open at/above high threshold | Run By |
|------------|---------------|--------|------------------------------|--------|
| 2026-11-10 | 18 | 18 | 0 | gsd-security-auditor; orchestrator scope review |

## Sign-Off

- [x] All threats have an evidence-supported disposition.
- [x] No owner-accepted risks were invented.
- [x] `threats_open: 0` at the configured high block threshold.
- [x] ASVS L1 threat register was checked against implementation, tests, documentation and current verification evidence.

**Approval:** verified 2026-11-10. The remaining T-03-09 host-misuse limitation is below the blocking threshold and outside the guest-controlled boundary; no sandbox claim is made.

## Plan 07 security follow-up

An independent gsd-security-auditor reviewed the digest delta after implementation
and found all four added high-severity mitigations closed, with no regression in
the prior 18 boundaries. Total verified threat rows: 22; open high/critical: zero.
The auditor independently ran existing instrumented logo-digest, transaction and
length binaries, core-purity checking, current ASan inventory/JUnit reconciliation
and both owned digest vectors. No source changes or owner acceptance were inferred.
The production authority remains unchanged/pending; trusted native-host/build
preconditions and all prior qualification limits remain in force.

## Owner-authorized authority activation

The owner explicitly supplied two local files and delegated local copying,
hashing and verification. This replaces the prior missing authority input, not
the parser or loader trust boundary. The fixed-path parser, candidate extent
checks, copied-window hashing and advisory commit path are unchanged. The real
scan detects both local files; tree scans exclude the ignored private copies.
Only digest metadata and byte-free reports are proposed for the phase PR.
The focused activation review is clean; no new runtime callback, dependency,
public fixture or publication right is introduced. Existing 22 threat rows and
zero open high/critical findings are retained; no new owner risk acceptance is
inferred from the successful comparisons.
