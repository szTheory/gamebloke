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
| `thumb/thumb.asm` | `090a6bb08b3813e2da9adef9705368d5c292bd563aeada82189f93b5b16e0f6e` |
| `memory/memory.asm` | `03c62d6ac9d72e65698f1f444780657998510c1c63ec8fe62905b20419a9559c` |
| `lib/macros.inc` | `25b0ff553c4ca99d76ec82c8c9db874e25d6eed58e2e69fa27f1a0fe7a8c46d4` |
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
  branches to the `eval` labels, declares Thumb `eval` in `code32`, and expands
  `m_vsync` before `m_test_eval`.
- **Byte observation:** the fetched files match the three pinned SHA-256 values;
  disassembly at offsets `0x1D4C`, `0x0934`, and `0x0350` matches the first ARM
  instructions of `m_vsync`.
- **Inference:** the result register at `eval` is final because the source's
  failure exits store the test number and the successful fall-through preserves
  the initialized zero. The local runner must still observe zero at the entry
  to qualify each run.

The upstream BIOS ROM hash recorded in the manifest is
`9d7b369fa1aa661ff03692b3d79c6f644b623d72983d0fc890e6d87a0409a3c9`. It is not
a fetch pin and was not downloaded or executed.
