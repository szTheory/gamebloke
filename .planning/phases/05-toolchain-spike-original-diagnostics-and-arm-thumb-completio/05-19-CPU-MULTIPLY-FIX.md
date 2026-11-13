# Plan 19 CPU Multiply Fix

Corrected long multiply signedness decoding in `src/cpu.c`: ARM bit 22 set selects signed SMULL/SMLAL. Audited the native multiply literals and fixed the UMULL/SMULL and UMLAL/SMLAL opcodes in arithmetic, flag, accumulation, timing, restriction, and preflight cases. Added paired UMULLS/SMULLS vectors for `0xFFFFFFFF * 3`, which must produce high words `2` and `0xFFFFFFFF` respectively.

Encoding basis: ARM DDI0100I A4.1.76, A4.1.87, A4.1.128, and A4.1.129 detailed instruction forms; GNU ARM assembler independently produced SMULLS `E0D42391`, UMULLS `E0942391`, SMLALS `E0F42391`, and UMLALS `E0B42391`. The overview's `Un` legend conflicts with the detailed forms; the detailed encodings match the assembler output.

## Verification

- **RED before the core fix:** both new contrasting vectors failed; signed product/accumulation cases and negative-value early-termination timing boundaries also failed against the old predicate.
- **GREEN, CI build root:** `build/phase05-plan19/cpu-fix/ci`; `cpu_test multiply_unsigned` passed 7 cases and `cpu_test multiply_signed_timing` passed 86 cases.
- **GREEN, native ASan/UBSan root:** `build/phase05-plan19/cpu-fix/asan`, built with `/usr/bin/clang`; the same 7 and 86 cases passed.
- Refusal/preflight, flags, accumulation, and Rs-based signed/unsigned timing cases are included in those groups.
- **Human review:** Confirm the signed/unsigned behavior during the existing phase acceptance review.

Commit: `1f83bcc` (`src/cpu.c`, `tests/cpu_test.c`).
