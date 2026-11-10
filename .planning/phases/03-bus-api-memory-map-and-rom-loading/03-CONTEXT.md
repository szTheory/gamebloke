# Phase 3: Bus API, Memory Map, and ROM Loading - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver BUS-01 through BUS-03 and LOAD-01 through LOAD-03: the timing-bearing
bus contract, GBA region decode and width rules, deterministic initialization,
and safe copy-on-load ROM handling. CPU execution and pipeline integration
belong to Phase 4; configurable WAITCNT, prefetch and BIOS read protection
belong to the later bus-timing milestone. No hardware-accuracy or release claim.

</domain>

<decisions>
## Implementation Decisions

### Standing owner instructions and discussion disposition
- The owner invoked autonomous execution from Phase 3 and instructed routine,
  reversible decisions to proceed without acknowledgments. Research found no
  new material owner decision. The defaults below are agent decisions under
  that standing authority, not newly accepted owner judgments.
- Preserve Codex runtime routing and both runtime model mappings in config.
- Preserve one PR per phase, squash merge only after required CI is green;
  stop on red CI, with no ruleset bypass or repository-setting change.

### Bus and deterministic state
- Keep every memory access and internal-cycle advancement behind one bus-owned
  time authority, using a uint64_t master clock. Results expose cycle cost
  and sequential/non-sequential classification; make overflow behavior explicit.
- Use documented placeholder costs where timing is not yet implemented, labeled
  as project policy in the profile. No claim of WAITCNT or prefetch timing.
- Keep the open-bus latch explicit and testable. Phase 3 tests its simple
  unmapped-read contract; CPU-prefetch-driven latch semantics need Phase 4.
  A universal last-data-access latch is not a GBATEK hardware claim.
- Decode the documented region windows, mirrors and byte/halfword/word behavior.
  Explicitly label unimplemented I/O and device semantics; do not imply a flat
  writable I/O array models hardware. CPU misaligned load rotation is Phase 4.
- Zero-fill fresh mutable backing memory and register storage, with zero clock
  and initial latch. This is a project determinism policy; hardware power-on RAM
  contents remain unknown. CPU direct-start registers are a Phase 4 decision.
- Cite GBATEK 3.06 sections per decode case. Do not reuse 2.9b-derived claims.

### Loader ownership and refusal behavior
- Validate lengths before allocation, reads or mutation: zero, below 0xC0,
  over 32 MiB, odd. Give distinct stable reasons; define overlap precedence.
- Copy the caller's ROM. On a successful second load, atomically replace the
  ROM and reset machine state to documented cold-start defaults. Prepare the
  new image before releasing the old one. Refusal and allocation failure leave
  the existing ROM, state, and caller input unchanged.
- Keep resource failure distinct from malformed input. Use bounded result
  fields, avoiding caller diagnostic-buffer mutation on refusal.
- Preserve the freestanding core gate. Allocation or storage provisioning must
  stay explicit at the host boundary; do not introduce host heap/file APIs.
- Compare canonical state fields and arrays, excluding padding and pointer
  identities, for full-state refusal evidence. Prove refusal on already-loaded,
  non-default state as well as fresh state.

### Header findings and the existing logo gate
- Under direct-start, logo, checksum and unusual game-code findings are advisory;
  they never refuse an otherwise size-valid image. Do not embed Nintendo bytes.
- The real logo digest remains owner-pending. Do not obtain or manufacture a
  canonical replacement. Report an explicit unchecked/unverified reason when
  canonical comparison cannot run. An all-zero window may be labeled missing
  by documented project convention; other content is not thereby a match.
- Synthetic test-only expected windows may prove comparison paths, including
  mismatch. They cannot establish comparison against the real Nintendo logo.
- LOAD-02 remains partial for canonical logo mismatch detection until the owner
  supplies the digest. Keep that acceptance gap visible in verification; do not
  silently narrow the requirement or mark all phase criteria passed.
- Header checksum and bounded game-code diagnostics proceed independently.

### The agent's discretion
Exact internal layouts, minimal lifecycle API shape without a stable ABI promise,
allocation/storage mechanism consistent with the purity gate, function and reason
names, test decomposition, placeholder table values and explicit I/O limits.
Do not add dependencies or expose the final consumer/package API prematurely.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- C17 static core and runner, include/gamebloke/gamebloke.h, tests/check.h.
- CTest registration in tests/core.cmake and tests/gates.cmake; sorted lane
  inventories; ExpectFail helpers and synthesized mutation drivers.
- profiles/agb.json and the profile validator/doc-sync checks from Phase 2.
- Core purity gate allows only freestanding headers; no malloc or host headers.

### Established Patterns
- One canonical local/CI command: cmake --workflow --preset ci.
- ASan/UBSan lane, floor CMake 3.25, hygiene/signature gates with visible skips.
- No third-party binaries, copyrighted logo/BIOS bytes or host paths in tree.

### Integration Points
- Add core sources to gamebloke_core and behavior tests to ci and asan lanes.
- Bump profile_version for claim-affecting changes and update narrative/validator.
- Keep Phase 2 result vocabulary for later runner status mapping; no JSON runner
  or CPU execution implementation in Phase 3.

</code_context>

<specifics>
## Specific Ideas

Research consulted the GBATEK 3.06 extraction at https://mgba-emu.github.io/gbatek/
(the original site timed out). Relevant sections: GBA Memory Map; Memory Mirrors;
Address Bus Width and CPU Read/Write Access Widths; Writing 8bit Data to Video
Memory; Reading from Unused Memory; Reading from BIOS Memory; GBA Cartridge Header.
These are community-reference observations, not physical qualification. Treat
cross-reference ambiguity and reference errata explicitly in planning research.

</specifics>

<deferred>
## Deferred Ideas

- Real-logo canonical comparison: existing owner-pending digest gate; owner acts.
- CPU execution, banked registers, pipeline fetch/latch integration: Phase 4.
- WAITCNT, prefetch, BIOS protection and active devices: later milestone work.
- Stable embedding API, packaging and installed consumers: later roadmap.
- Phase 1 verification is reported stale after later source changes; refresh
  before milestone close rather than treating the historical pass as current.

</deferred>
