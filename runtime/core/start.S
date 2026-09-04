.syntax unified
.arm

/*
 * hpclang v1 application entry.
 *
 * The family launcher has already installed the common virtual mappings:
 *
 *   0x01220000  executable image
 *   0x01100000  boot page
 *   0x01101000  writable application arena
 *
 * The launcher also supplies an 8-byte aligned stack at the top of the
 * writable arena.  No historical HPGCC register or PIC convention is part of
 * this ABI.
 */

.section .text.start,"ax",%progbits
.global _start
.type _start,%function
_start:
    stmfd sp!, {r4-r7, lr}
    sub   sp, sp, #4

    /* Copy initialized writable data from the code image. */
    ldr   r4, =__data_load
    ldr   r5, =__data_start
    ldr   r6, =__data_end
1:
    cmp   r5, r6
    bhs   2f
    ldr   r7, [r4], #4
    str   r7, [r5], #4
    b     1b

    /* Clear BSS. */
2:
    ldr   r5, =__bss_start
    ldr   r6, =__bss_end
    mov   r7, #0
3:
    cmp   r5, r6
    bhs   4f
    str   r7, [r5], #4
    b     3b

    /*
     * C code is Thumb by default.  Use BX explicitly so the ABI does not
     * depend on a linker-generated interworking veneer.
     */
4:
    ldr   r0, =0x01100000
    ldr   r3, =hpclang_start
    mov   lr, pc
    bx    r3

    add   sp, sp, #4
    ldmfd sp!, {r4-r7, pc}
.size _start, .-_start
