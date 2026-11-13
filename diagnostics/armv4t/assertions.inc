/*
 * Original ARM7TDMI assertions shared by every selectable source case.
 * GBL_BROKEN changes only the expected operand; the guest executes the same
 * instruction and reaches this same compare in both members of a pair.
 */
.macro GBL_ASSERT_EQ reg, expected, broken_expected
    ldr r12, =\expected
.if GBL_BROKEN
    ldr r12, =\broken_expected
.endif
    cmp \reg, r12
    bne .Ldiagnostic_assert_fail
.endm

.macro GBL_ASSERT_MASKED reg, mask, expected, broken_expected
    ldr r12, =\mask
    and r11, \reg, r12
    ldr r12, =\expected
.if GBL_BROKEN
    ldr r12, =\broken_expected
.endif
    cmp r11, r12
    bne .Ldiagnostic_assert_fail
.endm
