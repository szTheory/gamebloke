# Phase 4 ARM7TDMI Research

**Researched:** 2026-11-10
**Confidence:** High for cited ARM/C facts; medium for the project recommendations below.

## Recommendation

For Phase 4, use one interpreter path with explicit instruction-fetch/pipeline state, visible ARM/Thumb PC semantics, banked registers, and a centralized exception-entry transition. Implement only the execution slice needed to exercise those behaviors and the bounded runner; keep complete ARM and Thumb coverage in Phase 5 (CPU-01/02). Do not add a decode cache or fused-instruction path. The current `gbl_machine` struct explicitly lists `uint64_t master_cycles;`, `uint32_t open_bus;`, `uint16_t dispcnt;`, and memory/ROM fields, but no CPU register or pipeline state ([`gamebloke.h`](../../../include/gamebloke/gamebloke.h#L129-L143)); Phase 3 explicitly states “CPU direct-start registers are a Phase 4 decision” ([`03-CONTEXT.md`](../03-bus-api-memory-map-and-rom-loading/03-CONTEXT.md#L41-L43)).

For BIOS provider `none`, recommended policy: record the architectural exception transition (saved CPSR, exception mode/LR, ARM state, masks, vector PC), then stop with the named `GUEST_STOP` cause before fetching from the absent vector contents. The roadmap requires that “an IRQ entry, SWI, undefined instruction, or coprocessor opcode each ends the run as GUEST_STOP with the cause named, never as silent continuation” ([`ROADMAP.md`](../../ROADMAP.md#L186-L190)). Stopping after entry makes CPU-04 state observable while preventing zero-filled or open-bus bytes from masquerading as a BIOS handler. The stop edge is Gamebloke policy, not ARM behavior; discuss it in Phase 4.

## Phase requirements

| ID | Requirement | Research support |
|---|---|---|
| CPU-03 | “The core models the two-stage-visible pipeline (PC reads +8/+4) and the open-bus latch as explicit state, with no decode cache or fused-instruction shortcuts.” | ARM facts below plus Phase 3 bus-latch boundary. |
| CPU-04 | “The core switches modes and banks registers correctly for all ARM7TDMI modes. Exception entry saves the correct return address and SPSR.” | ARM mode/banking and exception tables below. |
| CPU-05 | “CPU arithmetic is free of C undefined behavior (shift ≥32, `ROR #0`/RRX, signed overflow), enforced by shift helpers and the UBSan lane.” | WG14 shift/unsigned arithmetic rules and ARM RRX semantics below. |

These requirement descriptions are quoted verbatim from [`REQUIREMENTS.md`](../../REQUIREMENTS.md#L38-L44).

## Source-derived facts and implications

| Topic | Sourced fact | Phase 4 implication |
|---|---|---|
| Version and pipeline | ARM7TDMI implements ARMv4T. Its TRM describes a three-stage fetch/decode/execute pipeline; the PC visible to an executing instruction is ahead of that instruction. The architecture reference specifies reads as instruction address `+8` in ARM and `+4` in Thumb. [ARM7TDMI TRM, §§1.1.1, 2.1, pp. 1-2, 2-2](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129); [ARM Architecture Reference Manual DDI0100I, §A2.4.3, pp. A2-9–A2-10](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a) | Preserve fetch/decode/execute visibility; derive PC reads from the current instruction and state. Branches, state changes, and exceptions must flush/refill pending instructions. Don't label the TRM's physical three stages as an architectural requirement beyond their observable effects. |
| Modes and banked registers | ARMv4T has User, FIQ, IRQ, Supervisor, Abort, Undefined, and System modes. FIQ banks `r8–r14`; IRQ/SVC/Abort/Undefined bank `r13–r14`; User and System share registers. Five exception modes have SPSRs; User and System do not. Thumb low registers map to the same ARM registers; SP/LR are mode-banked. [ARM7TDMI TRM, §§2.5–2.7, pp. 2-7–2-12](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) | Keep one physical register bank per architectural bank and centralize mode switching. Make User/System SPSR absence explicit; don't allocate fake SPSRs that make invalid returns appear valid. Test switching away and back to prove every bank is preserved. |
| Exception entry and returns | Entry saves CPSR to the destination SPSR, changes mode, forces ARM state, and loads the vector PC. LR/return adjustments differ: SWI/undefined `PC+4` ARM or `PC+2` Thumb, return `MOVS PC,LR`; prefetch abort `PC+4`, return `SUBS PC,LR,#4`; IRQ/FIQ `PC+4` in either state, return `SUBS PC,LR,#4`; data abort `PC+8` in either state, return `SUBS PC,LR,#8`. [ARM7TDMI TRM, §2.8 and Tables 2-3/2-4, pp. 2-16–2-22](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) | Use an exception-kind table/helper rather than a shared fixed LR offset. Save T in SPSR before entering ARM so exception return can restore the original state. Test each entry kind from ARM and Thumb where specified. |
| Vectors and masks | Reset vector is `0x00`; undefined `0x04`; SWI `0x08`; prefetch abort `0x0C`; data abort `0x10`; IRQ `0x18`; FIQ `0x1C`. Entry sets I for exceptions; F is set on reset and FIQ, unchanged on the other listed entries. IRQ is gated by I and FIQ by F. [ARM7TDMI TRM, §§2.8.4–2.8.9, Table 2-4, pp. 2-18–2-22](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) | Keep vector, destination mode, LR rule, and mask changes together in the exception definition. Preserve unchanged F on SWI/undefined/abort/IRQ; don't indiscriminately set both masks. |
| Reset vs direct start | ARM reset enters Supervisor/ARM with I and F set and PC at zero; other registers are indeterminate. [ARM7TDMI TRM, §2.10, p. 2-24](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) | Do not use ARM reset facts as a GBA post-BIOS/direct-start table. Pick and document deterministic CPU startup values as project policy; Phase 3 already zero-fills machine state as a determinism policy, not a hardware power-on claim ([`03-CONTEXT.md`](../03-bus-api-memory-map-and-rom-loading/03-CONTEXT.md#L41-L43)). The exact GBA startup values remain for the GBA-specific research/discussion. |
| `ROR #0` / RRX | In ARM data-processing operand encoding, rotate-right immediate field zero denotes RRX: shift Rm right one, insert old C at bit 31, and return old bit 0 as shifter carry. [ARM Architecture Reference Manual DDI0100I, §A5.1.11–A5.1.13, pp. A5-15–A5-17](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a) | Model RRX explicitly in a shift helper returning both value and carry. Decode the instruction's shift kind and immediate/register source first; never reduce all shift counts modulo 32. ARM's encoded-zero cases vary by operation. |
| C behavior | WG14 N1570 says shift counts that are negative or at least the promoted left operand's width are undefined; signed left shifts have additional undefined cases, signed right shift of a negative value is implementation-defined, and unsigned arithmetic wraps modulo its range. [WG14 N1570, §§6.2.5, 6.5, 6.5.7](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) | Perform register bit arithmetic with `uint32_t`; branch on decoded shift cases before any C shift. Compute signed overflow flags from operand/result sign bits or a wider checked method, not by overflowing a signed C expression. |

## Project constraints and boundaries

- The core remains deterministic and host-independent; host runner/adapters own clocks, files, and presentation concerns. Do not introduce BIOS bytes or copyrighted ROM inputs. [`AGENTS.md`](../../../AGENTS.md#L25-L43)
- CPU-03/04/05 are Phase 4; full ARM/Thumb behavior in CPU-01/02 is Phase 5. The Phase 4 goal allows bounded costs but expects explicit bus-only execution, all mode/exception evidence, and named no-BIOS stops. [`ROADMAP.md`](../../ROADMAP.md#L181-L209)
- Phase 3 owns one bus time authority and placeholder costs; CPU-prefetch-driven latch behavior and CPU alignment/load rotation were explicitly deferred to Phase 4. [`03-CONTEXT.md`](../03-bus-api-memory-map-and-rom-loading/03-CONTEXT.md#L29-L43)
- Do not infer CPU reset or post-BIOS state from zero-filled machine RAM. Document direct-start register state as project/profile policy, separate from sourced ARM reset semantics.

## Failure modes and negative witnesses

- **Sequential PC increment without refill:** branch to a known marker after placing a different word in the sequential slot; require the marker to execute and the stale slot not to execute. Test ARM and Thumb PC reads at `+8/+4`.
- **Flat or partially swapped banks:** write distinct values to User/System shared registers, FIQ `r8–r14`, and each exception mode's `r13/r14`; switch through every mode and verify values and SPSR isolation. Verify User/System have no exception SPSR.
- **One LR correction for every exception:** trigger SWI/undefined from Thumb, IRQ/FIQ from both states, and abort entries; assert exact saved LR, SPSR/T restoration, target mode and masks.
- **Absent BIOS becomes accidental execution:** for `none`, test IRQ, SWI, undefined, and coprocessor traps. Require named `GUEST_STOP` and unchanged guest progress after the stop; verify no fetch from the corresponding zero-filled vector path.
- **C shift UB or incorrect zero mapping:** exercise shift counts 0, 1, 31, 32, and greater than 32; test ARM immediate LSR/ASR `#0` and ROR `#0` separately. For RRX, vary C and Rm bit 0 and assert both result and carry. Run the existing UBSan lane.
- **Signed overflow hidden by host behavior:** test add/sub sign boundaries such as `0x7fffffff + 1` and `0x80000000 - 1`, and validate V independently from wrapped result bits.

## Material open decisions

1. **Direct-start CPU state:** choose the initial mode, T/mask bits, PC and general registers explicitly for `agb/direct-start`; ARM reset is not a substitute for post-BIOS GBA state.
2. **No-BIOS stop edge:** recommend complete architectural exception bookkeeping and vector-PC selection, then stop before the first handler fetch. This makes CPU-04 state observable while honoring PROF-03 and avoiding a fake BIOS path. Keep this labeled as a Gamebloke policy.
3. **Exception kinds implemented in Phase 4:** roadmap acceptance calls for every exception entry, while Phase 5 owns full opcode coverage. Prefer a centralized, table-driven entry operation with direct synthetic tests for all entries; only implement instruction-trigger paths required for Phase 4's runner and stop cases.

## Coverage and source identity

The full official ARM7TDMI TRM was read in bounded, untruncated chunks: cover and revision history, Chapters 1–7, Appendices A/B, glossary, and index (all 282 PDF pages; ARM DDI 0029G, ARM7TDMI Rev 3, Issue G, April 2001). Full PDF and extracted text are in ignored `build/phase04-research/arm/`; they are not tracked. The DDI0100I reference was consulted for ARMv4T-visible PC and shift/exception semantics, not read cover to cover. No ROM, BIOS image, save, or private content was read.
