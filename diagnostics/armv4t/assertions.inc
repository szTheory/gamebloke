/*
 * Original ARM7TDMI assertions shared by every selectable source case.
 * GBL_BROKEN changes only the expected operand; the guest executes the same
 * instruction and reaches this same compare in both members of a pair.
 */
.ifndef GBL_THUMB
.set GBL_THUMB, 0
.endif
.macro GBL_ASSERT_EQ reg, expected, broken_expected
.if GBL_THUMB
    push {r0-r2}
    movs r0, \reg
    ldr r1, =\expected
.if GBL_BROKEN
    ldr r1, =\broken_expected
.endif
    cmp r0, r1
    pop {r0-r2}
    bne .Ldiagnostic_assert_fail
.else
    ldr r12, =\expected
.if GBL_BROKEN
    ldr r12, =\broken_expected
.endif
    cmp \reg, r12
    bne .Ldiagnostic_assert_fail
.endif
.endm

.macro GBL_ASSERT_MASKED reg, mask, expected, broken_expected
.if GBL_THUMB
    push {r0-r2}
    movs r0, \reg
    ldr r2, =\mask
    ands r0, r2
    ldr r1, =\expected
.if GBL_BROKEN
    ldr r1, =\broken_expected
.endif
    cmp r0, r1
    pop {r0-r2}
    bne .Ldiagnostic_assert_fail
.else
    ldr r12, =\mask
    and r11, \reg, r12
    ldr r12, =\expected
.if GBL_BROKEN
    ldr r12, =\broken_expected
.endif
    cmp r11, r12
    bne .Ldiagnostic_assert_fail
.endif
.endm
