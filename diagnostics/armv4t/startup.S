.syntax unified
.cpu arm7tdmi
.arch armv4t
.arm

.section .gba_header,"ax",%progbits
.global _start
.type _start,%function
_start:
    b .Lreset
    .space 0xBC, 0

.section .text.startup,"ax",%progbits
.align 2
.Lreset:
    ldr sp, =0x03007F00
    bl diagnostic_main

    /* Publish the five-word direct-start mailbox after the test has finished. */
    ldr r1, =0x03007FC0
    ldr r2, =0x47424C4B
    str r2, [r1, #0]
    mov r2, #1
    str r2, [r1, #4]
    str r0, [r1, #8]
    ldr r2, =0x00005001
    str r2, [r1, #12]
    ldr r2, =0x434F4D50
    str r2, [r1, #16]

    /* A valid terminal mailbox is published only after the final marker. */
.Lhalt:
    b .Lhalt
    .ltorg
