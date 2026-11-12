---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "09"
subsystem: cpu
tags: [ARMv4T, ARM7TDMI, multiply, C, CTest, ASan, UBSan]

requires:
  - phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
    provides: "Plan08 canonical CPU decode, preflight, effect, and bus-owned timing path"
provides:
  - "Execution of all six ARM7TDMI ARM multiply forms through the canonical CPU unit"
  - "DDI0029G multiplier internal-cycle costs derived from original Rs"
  - "Inventoried multiply_unsigned and multiply_signed_timing CPU selectors"
affects: ["Phase05 remaining CPU plans", "CPU validation inventories"]

actuals:
  tokens: 8805
  tasks: 2
  commits: 7
commits: 7
plan_head_before: f3405b741a5d157deedc2094623e71f639826334
plan_head_after: ced8e0c577fea178d995bf422fae4462689d6d3d

tech-stack:
  added: []
  patterns:
    - "Represent signed long products with sign-extended uint64_t bit vectors and modulo-2^64 arithmetic."
    - "Derive multiplier work once from the captured original Rs before whole-unit preflight."

key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt

key-decisions:
  - "Use signed early termination for short MUL/MLA as an explicit deterministic project policy; DDI0100I leaves that choice implementation-defined. Long forms use their opcode's signed or unsigned termination rule."
  - "Update only N/Z for S forms; preserve incoming short C/V and long C/V under the project's documented deterministic policy."
  - "Model the DDI0029G internal-cycle table through the bus-owned clock without claiming Game Pak waits, prefetch behavior, or hardware accuracy."

patterns-established:
  - "All multiplier operands and accumulation words are captured before destination effects, including permitted source/destination overlap."
  - "Refusal tests compare named canonical state, identity, trace records, and buffer ownership instead of struct padding."

requirements-completed: []
coverage:
  - id: D1
    description: "MUL, MLA, UMULL, UMLAL, SMULL, and SMLAL execute with 32/64-bit results, captured operands, defined N/Z flags, and explicit preservation policy."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test multiply_unsigned (6/6) and multiply_signed_timing (85/85)"
        status: pass
      - kind: integration
        ref: "cmake --workflow --preset ci (336 passed, one optional skip; inventory post-check passed)"
        status: pass
    human_judgment: false
  - id: D2
    description: "All six forms reserve DDI0029G internal work from original Rs and preserve machine/trace state on one-short preflight refusals."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test multiply_signed_timing (85/85; signed and unsigned Rs boundaries plus exact-fit/one-short checks)"
        status: pass
      - kind: integration
        ref: "Native AppleClang 21 ASan/UBSan inventory (80/80, zero skips) and post check"
        status: pass
    human_judgment: false

duration: 43min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 09: ARM7TDMI Multiply Semantics and Timing Summary

**The canonical ARM CPU path now executes all six multiply forms with exact result bits and source-referenced internal-cycle costs.**

## Performance

- **Duration:** 42 minutes
- **Started:** 2026-11-12T13:42:52Z
- **Completed:** 2026-11-12T14:26:02Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Added canonical ARM decode and preflight support for MUL, MLA, UMULL, UMLAL, SMULL, and SMLAL. The implementation snapshots sources and accumulators before effects, uses defined unsigned arithmetic for low and long results, and implements signed long products without implementation-defined uint32-to-signed conversions.
- Added independent assertions for signed result edges, accumulator wrap, defined N/Z updates, chosen C/V preservation, exact ARM7TDMI operand restrictions, and allowed source/destination overlap.
- Applied DDI0029G §6.6 and §6.20/Table 6-23 timing to the original Rs value for all six forms. Tests cover positive and negative termination boundaries, exact cost, one-short cycle/counter/trace refusal, and unchanged canonical state and trace buffers.
- Registered both multiply selectors in CMake and the CI/ASan inventories.

## Task Commits

1. **Task 1 RED: unsigned multiply result vectors** — `e9d9943`
2. **Task 1 GREEN: MUL/MLA/UMULL/UMLAL** — `abc9fda`
3. **Task 2 RED: signed vectors and timing boundaries** — `44fdfab`
4. **Task 2 RED addendum: operand restrictions** — `3178886`
5. **Task 2 test correction: signed destinations, result shape, and refusal reasons** — `b9517e7`
6. **Task 2 GREEN: signed long multiply and DDI timing** — `5ba9a5e`
7. **Decode regression control: retain an unsupported ARM block transfer case** — `ced8e0c`

The corrected Task 2 RED run against the Task 1 CPU failed the named SMULL result assertion and passed both invalid-operand refusal controls. `gsd-tools check tdd-red-evidence` returned `RED_EVIDENCE_OK`; evidence is retained under `build/phase05-plan09/`. The GREEN selector then passed all 85 assertions.

## Files Modified

- `src/cpu.c` — six-form decode, signed bit-vector product, and source-derived internal cycles.
- `tests/cpu_test.c` — result, flags, restrictions, timing boundaries, and full preflight/refusal coverage.
- `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt` — selector and inventory registration.

## Decisions Made

- ARM DDI0100I leaves short MUL/MLA early-termination signedness implementation-defined. Gamebloke pins signed termination as its deterministic short-form policy; signed and unsigned long forms use the explicit opcode mode. The boundary vectors distinguish high all-one Rs values from unsigned high-zero values.
- DDI0029G Table 6-23 yields `m` internal cycles for MUL, `m+1` for MLA and long multiply, and `m+2` for long accumulate. The sequential fetch is accounted for by the existing CPU unit. This models internal work only; no Game Pak wait-state, prefetch, or physical-cycle claim is made.
- S forms update N/Z only. Incoming C is preserved for short forms under the chosen project policy, and C/V are preserved for long forms; non-S forms preserve NZCV.

## Deviations from Plan

### Auto-fixed Issues

**1. Rule 1 — Corrected multiply test oracles exposed by the first GREEN run**
- **Found during:** Task 2
- **Issue:** Two signed vectors targeted the wrong destination fields; short-form checks expected a long high word; trace exhaustion was expected to report the wrong runner termination/reason.
- **Fix:** Corrected the encodings and result expectations, and asserted the runner's trace-output-full preflight result. Re-ran the corrected RED suite against the Task 1 implementation before committing the test correction.
- **Files modified:** `tests/cpu_test.c`
- **Verification:** RED classifier passed; the implemented selector passed 85/85 assertions.
- **Commit:** `b9517e7`

**2. Rule 1 — Replaced a stale unsupported-MUL decode control**
- **Found during:** Full native CI
- **Issue:** Existing `cpu.decode` expected `MUL r0,r0,r0` to stop as unsupported, but this plan makes that valid ARMv4T instruction executable. Its clean-control dependents failed for the same reason.
- **Fix:** Kept the unsupported-instruction control using a valid unsupported `STMIA r0,{r1}` block-transfer encoding.
- **Files modified:** `tests/cpu_test.c`
- **Verification:** `build/ci/cpu_test decode` passed; the complete CI workflow passed on rerun.
- **Commit:** `ced8e0c`

**Total deviations:** 2 test-oracle corrections. No implementation scope was added.

## Verification

- `cmake --workflow --preset ci` — passed; 336 tests passed and one optional owner-local signature test was skipped because `GBL_OWNER_LOGO_FILE` was unset. The 337-case CI JUnit inventory post-check passed.
- `cmake --preset asan -B build/phase05-plan09-asan-macos -DCMAKE_C_COMPILER=/usr/bin/clang`, followed by build and CTest — passed with AppleClang 21 and ASan/UBSan; 80/80 main tests, zero skips, and the ASan inventory post-check passed.
- The ASan build emitted two CMake author warnings for generated smoke-ROM paths; fixture-index validation and the build completed successfully.
- No hosted CI job was started by this executor. The pinned Linux Clang 21 final sanitizer run remains with the root orchestrator.

## User Setup Required

None. No ROM or BIOS files were accessed.

## Next Phase Readiness

Plan 09 is complete. Phase-wide CPU-01 remains pending as specified; this plan's results make no game, BIOS, bus-wait, prefetch, physical-hardware, or broad compatibility qualification claim. The root orchestrator owns final pinned Linux verification before publication decisions.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary file exists.
- All seven plan task commits are ancestors of the current branch head.
- `.planning/state.json` still matches the exact task-start snapshot.
- `git diff --check` passes.
