# jsmolka CPU fixture source and binary audit

## Scope and rights

This audit binds three upstream prebuilt files from `jsmolka/gba-tests` at
revision `a7113b67e63f83a9b321696ddd7042ccfad6c881`. No upstream ROM was rebuilt.
Only the ARM, Thumb, and memory suite binaries are fetched, and only into the
ignored local `.fixtures/jsmolka/` directory. They remain local-only and must not
be committed, uploaded, or run in public CI. The `bios/bios.gba` source and digest
are metadata-only; its user-official-BIOS requirement skips it before any ROM
read. See [the fixture notice](../tests/fixtures/jsmolka-NOTICE.md).

The upstream `LICENSE` declares MIT for the source repository. The pinned
`lib/header.asm` contains Nintendo logo bytes. No separate logo-byte or ROM
redistribution grant was identified; the source license is not treated as that
grant. Digests below establish identity only.

## Pinned source evidence

All paths below were read as raw files at the pinned revision. SHA-256 is over
the exact raw file bytes served at those immutable paths.

| Source path | SHA-256 |
| --- | --- |
| `arm/arm.asm` | `eb708021f0bc77e022516e5770862c267a54513b45a4b0c81eae921b4a582283` |
| `arm/data_processing.asm` | `19fa7c26807bf83dababe46c76b120bc0f20adbb6c7fbf60fd900c6391698a0d` |
| `thumb/thumb.asm` | `090a6bb08b3813e2da9adef9705368d5c292bd563aeada82189f93b5b16e0f6e` |
| `thumb/memory.asm` | `250b5b0e4a616f2b836d8fe525df4092936a5992f8b2143c7784cdbe42a84e03` |
| `memory/memory.asm` | `03c62d6ac9d72e65698f1f444780657998510c1c63ec8fe62905b20419a9559c` |
| `lib/macros.inc` | `25b0ff553c4ca99d76ec82c8c9db874e25d6eed58e2e69fa27f1a0fe7a8c46d4` |
| `lib/text.asm` | `77e579d6cf0db182fd24e8d420c8c0a81592d78197e8fb06accc8bcfb43db17e` |
| `lib/header.asm` | `e9d1e8c7b6382eeb1b7059423ec365e32911f702eadfa51804a2119438a87357` |
| `LICENSE` | `b59a9ed235d76752c977f04116bc72155a14af48176bdbf50d7e5c29fca35aa7` |

## Source control flow

`arm/arm.asm` initializes R12 to zero. Its test groups execute before `eval`;
the suite's `m_exit test` macro writes the failure test number into R12 and
branches to `eval`. `memory/memory.asm` follows the same pattern, with its
`m_exit` macro writing R12 before branching. If those groups complete without a
failure exit, their initialized zero remains in R12.

`thumb/thumb.asm` initializes R7 to zero while `tmain` is in `code16`. A failed
test's `m_exit test` writes its test number to R7 and branches through
`tmain_end`. That label computes `eval` and uses `bx`; the target is even and
`eval` is declared under `code32`, so execution enters ARM state. A successful
fall-through reaches the same `tmain_end` with R7 still zero.

At each suite's `eval`, `m_vsync` is expanded before `m_test_eval`. The latter
checks the accumulator and prints a status only after display synchronization.
The runner observes the configured register at the first instruction boundary
of `eval`, before executing the first `m_vsync` instruction. This records the
suite's own result without waiting for display state. It makes no VBlank, PPU,
graphics, audio, peripheral, or hardware-accuracy claim.

## Direct-start bootstrap derivative

The pinned ARM, Thumb, and memory sources all expand `m_test_init` before their
test body. `m_test_init` calls `text_init`, whose first instruction pushes
`r0-r1,lr` through SP. That upstream startup assumes a BIOS-provided stack and
does not initialize SP before the call. With the accepted zero-filled
direct-start state, the first push targets unmapped `0xfffffff4`–`0xfffffffc`.

The local runner therefore creates a derivative from each separately pinned
upstream ROM input. It changes only the entry branch at file offset `0x0000`
and the reserved header words at `0x00B8`–`0x00BF`: the entry branches to a
two-instruction ARM bootstrap (`MOV SP, #0x03000000`; `ORR SP, SP, #0x7F00`),
then falls through to the unchanged upstream `main` at `0x080000C0`. The guest
sets its own SP to `0x03007F00`; the emulator and runner still start with every
CPU register zero. The ROM header logo and upstream suite/evaluator code remain
unchanged. This is a local-only fixture adaptation, not an upstream binary or
a BIOS handoff.

| Fixture | Pinned upstream SHA-256 | Derived execution SHA-256 |
| --- | --- | --- |
| `jsmolka-arm` | `77ee88662552bdc885c1080c0172ff119d54db791bd73b21808cf1ff1fe5b40e` | `09e9a33570f5461108ee6d1a8fcab89cdfbee518d8680ab9d3ded08574603e57` |
| `jsmolka-thumb` | `b5cb2291df4ab314b31c598acd9bff2ccfa0b38efff29daadfe97422ce369b67` | `1f9663c94b79a83213841dae656139c33c5a75193969defee08271ce2a97100b` |
| `jsmolka-memory` | `21024fb6aae6343f5f0466dd54e3149de1fbeb23f78e7d85a015c983684d2f87` | `f710599178269914ab0edf5b462df01a2d851f68eaa190378de636b4fa45dcb5` |

The deterministic patch recipe is `scripts/derive-local-jsmolka.py`. It checks
the pinned input digest before patching and verifies the output digest after
patching. The derived outputs live under ignored build storage and must remain
local-only. A 50-cycle startup check requires the three initial stack writes
to complete in IWRAM. A 2,000-cycle compatibility probe then runs each suite
before its 50,000,000-cycle budget is considered. Only a `TIMEOUT` with reason
`cycle-budget` is inconclusive and proceeds to the longer run; a terminal result
is preserved and reused. These probes are diagnostic evidence only; each suite
still needs an observed zero at its source-bound `eval` register to qualify.

## Compatibility stops under the current CPU profile

The short compatibility probe exposes two source/profile conflicts before a
long run. ARM test 234 in `arm/data_processing.asm` contains opcode
`0xE15FF000` (`cmp pc, pc, r0`); its CMP encoding has the unused Rd field set
to PC, which the current CPU profile stops as UNPREDICTABLE. Thumb memory test
227 in `thumb/memory.asm` contains opcode
`0xC800` (`ldm r0!, {}`), whose empty register list the profile also stops as
UNPREDICTABLE. The memory suite reaches its evaluator and reports zero.

These are actual suite outcomes, not timeout or runner failures. The current
profile deliberately refuses these encodings, so this phase does not change
those policies, edit upstream test bodies, omit either suite, or count a stop as
PASS. ARM passing test 225 after the register-shift PC fix is useful CPU evidence,
but it does not resolve test 234. DIAG-04 therefore remains incomplete while
the accepted profile and both pinned suite expectations disagree.

The PC-read fix follows the [ARM7DI Data Sheet §4.4.5](https://documentation-service.arm.com/static/5ed11a2dca06a95ce53f8f99?token=)
rule that R15 reads as the instruction address plus 12 when a register supplies
the shift amount. The focused regression covers this for both Rn and Rm. This
is reference-based behavior, not a claim of physical hardware measurement.

## Exact ROM identity and entry evidence

The `eval` labels were resolved against the exact SHA-256-verified ROM bytes.
The base mapping is `0x08000000 + file offset`. Each entry starts with the same
little-endian ARM word `E92D0003` (`push {r0, r1}`), followed by the safe
`m_vsync` sequence shown below. These are instruction bytes and control flow
only; no ROM or logo bytes are included in this document.

| Fixture | ROM source path | ROM SHA-256 | Eval address | File offset | Entry state | Register |
| --- | --- | --- | --- | --- | --- | --- |
| `jsmolka-arm` | `arm/arm.gba` | `77ee88662552bdc885c1080c0172ff119d54db791bd73b21808cf1ff1fe5b40e` | `0x08001D4C` | `0x1D4C` | ARM | R12 |
| `jsmolka-thumb` | `thumb/thumb.gba` | `b5cb2291df4ab314b31c598acd9bff2ccfa0b38efff29daadfe97422ce369b67` | `0x08000934` | `0x0934` | ARM | R7 |
| `jsmolka-memory` | `memory/memory.gba` | `21024fb6aae6343f5f0466dd54e3149de1fbeb23f78e7d85a015c983684d2f87` | `0x08000350` | `0x0350` | ARM | R12 |

The exact bytes at all three entries disassemble as:

```text
E92D0003  push {r0, r1}
E3A00301  mov r0, #0x04000000
E5901004  ldr r1, [r0, #4]
E3110001  tst r1, #1
1AFFFFFC  bne <m_vsync vblank-end poll>
E5901004  ldr r1, [r0, #4]
E3110001  tst r1, #1
0AFFFFFC  beq <m_vsync vblank-begin poll>
```

The matching ARM-state instruction words independently confirm that the Thumb
suite's `eval` entry is not in Thumb state. Its test body is Thumb code, but the
result channel is sampled after the source-controlled transition to ARM.

## Evidence classification

- **Source observation:** the pinned assembly initializes and updates R12/R7,
  branches to the `eval` labels, declares Thumb `eval` in `code32`, expands
  `m_vsync` before `m_test_eval`, and calls `text_init` before the test body.
  The pinned ARM source contains test 234's `0xE15FF000`; the pinned Thumb
  memory source contains test 227's `0xC800`.
- **Byte observation:** the fetched files match the three pinned SHA-256 values;
  disassembly at offsets `0x1D4C`, `0x0934`, and `0x0350` matches the first ARM
  instructions of `m_vsync`. Derived files match the patch recipe's hashes,
  retain the upstream logo bytes and evaluator offsets, and begin with the
  recorded guest stack bootstrap.
- **Inference:** the result register at `eval` is final because the source's
  failure exits store the test number and the successful fall-through preserves
  the initialized zero. The local runner must still observe zero at the entry
  to qualify each run.

The upstream BIOS ROM hash recorded in the manifest is
`9d7b369fa1aa661ff03692b3d79c6f644b623d72983d0fc890e6d87a0409a3c9`. It is not
a fetch pin and was not downloaded or executed.
