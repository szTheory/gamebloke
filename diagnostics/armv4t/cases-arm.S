/*
 * Original, independently selected ARM7TDMI diagnostic source.
 * BuildArmDiagnostics.cmake supplies the selected literal ledger ID, a
 * generated case body copied from that ledger's source_test, and GBL_BROKEN.
 */
.syntax unified
.cpu arm7tdmi
.arch armv4t
.arm

.include "diagnostics/armv4t/assertions.inc"

.section .text.smoke,"ax",%progbits
.align 2
.global diagnostic_main
.type diagnostic_main,%function
diagnostic_main:
    /* Preserve the startup caller's return address across BL-family cases. */
    stmdb sp!, {lr}
    .include "arm-case.inc"
    mov r0, #2
    ldmia sp!, {lr}
    bx lr

.Ldiagnostic_assert_fail:
    mov r0, #3
    ldmia sp!, {lr}
    bx lr

.size diagnostic_main, .-diagnostic_main
