.syntax unified
.cpu arm7tdmi
.arch armv4t

.section .text.smoke,"ax",%progbits
.align 2
.arm
.global diagnostic_main
.type diagnostic_main,%function
diagnostic_main:
    /* ARM7TDMI ARM arithmetic: 2 + 3 must equal the independently fixed 5. */
    mov r2, #2
    add r2, r2, #3
.if GBL_BROKEN
    cmp r2, #6
.else
    cmp r2, #5
.endif
    moveq r4, #0
    movne r4, #1

    /* Exercise the same ARM-to-Thumb exchange in both expectation variants. */
    adr r3, thumb_probe + 1
    bx r3

.thumb
.thumb_func
thumb_probe:
    /* Thumb arithmetic: 4 + 2 must equal the independently fixed 6. */
    movs r2, #4
    adds r2, #2
.if GBL_BROKEN
    cmp r2, #7
.else
    cmp r2, #6
.endif
    bne .Lthumb_fail
    cmp r4, #0
    bne .Lthumb_fail
    movs r0, #2
    bx lr

.Lthumb_fail:
    movs r0, #3
    bx lr

.size diagnostic_main, .-diagnostic_main
