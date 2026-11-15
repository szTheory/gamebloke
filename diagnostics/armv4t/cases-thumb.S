/* Original selectable ARM7TDMI Thumb diagnostics. Direct-start ARM startup
 * owns the final mailbox; this body explicitly establishes its stack and
 * returns semantic state only after assertions. r10 holds the ARM caller. */
.syntax unified
.cpu arm7tdmi
.arch armv4t
.set GBL_THUMB, 1
.include "diagnostics/armv4t/assertions.inc"

/* Thumb has no MRS/MSR. These original inline interworking helpers preserve
 * tested low registers, change only the requested flags or capture r3, and
 * use BX to return to the next halfword. No exception/BIOS service is used. */
.macro GBL_SEED_FLAGS value
    .balign 4
    bx pc
    nop
    .arm
    ldr r12, =\value
    msr cpsr_f, r12
    add r12, pc, #1
    bx r12
    .thumb
.endm
.macro GBL_CAPTURE_FLAGS
    /* Branch over alignment padding: zero fill would execute MOVS r0,r0 and
     * destroy the flags being measured after an explicitly encoded opcode. */
    b .Lcapture_aligned\@
    .balign 4
.Lcapture_aligned\@:
    bx pc
    nop
    .arm
    mrs r3, cpsr
    add r12, pc, #1
    bx r12
    .thumb
.endm

.section .text.smoke,"ax",%progbits
.align 2
.arm
.global diagnostic_main
.type diagnostic_main,%function
diagnostic_main:
    mov r10, lr
    ldr sp, =0x03007F00
    adr r12, .Lthumb_entry+1
    bx r12
.thumb
.Lthumb_entry:
    .include "thumb-case.inc"
    movs r0, #2
    b .Lthumb_return
.Ldiagnostic_assert_fail:
    movs r0, #3
.Lthumb_return:
    .balign 4
    bx pc
    nop
.arm
    ldr sp, =0x03007F00
    mov lr, r10
    bx lr
    .ltorg
.size diagnostic_main, .-diagnostic_main
