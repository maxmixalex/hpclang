.syntax unified
.arm

/*
 * hp39gs clean ARM loader
 *
 * This is a new implementation of the calculator-side entry contract used by
 * the HP39 ARM launcher. It intentionally contains no historical HPGCC object
 * code and has no r10/GOT dependency.
 *
 * Entry contract:
 *   r1 -> launcher/Saturn register block
 *
 * New application ABI:
 *   code + read-only data: ROPI
 *   writable data:        RWPI, r9/sb = 0x09000400
 */

.equ HP39_RAM_BASE,        0x09000400
.equ HP39_MMU_L1_SLOT,     0x07f00240
.equ HP39_VECTOR_SWI,      0x08000008

.section .text.entry,"ax",%progbits
.global _loader
.type _loader,%function
_loader:
    stmfd sp!, {r0-r12, lr}

    /*
     * The launcher supplies the physical coarse page-table address at +0x920.
     * Install it into the L1 slot for the 0x09000000 virtual window.
     */
    ldr   r2, =HP39_MMU_L1_SLOT
    ldr   r0, [r1, #0x920]
    orr   r0, r0, #0x11
    str   r0, [r2]
    bl    hp39_flush_cache

    /*
     * Relocate the stack to the top of the mapped RAM window. Keep the old SP
     * in the top word so it can be restored without writable globals.
     */
    ldr   r2, [r1, #0x914]
    mov   r2, r2, lsl #12
    add   r2, r2, #0x09000000
    sub   r2, r2, #4
    str   sp, [r2]
    mov   sp, r2

    /*
     * _loader is linked at VMA 0. ADR therefore gives the actual image base
     * after the calculator has placed the ARM payload in memory.
     */
    adr   r8, _loader

    /*
     * Copy initialized writable data from its packed load image into the RWPI
     * data window. There is deliberately no GOT and no runtime GOT patch loop.
     */
    ldr   r0, =__data_load
    add   r0, r8, r0
    ldr   r2, =__data_size
    ldr   r3, =HP39_RAM_BASE
1:
    cmp   r2, #0
    beq   2f
    ldrb  r4, [r0], #1
    strb  r4, [r3], #1
    subs  r2, r2, #1
    bne   1b

    /* Clear BSS immediately after initialized data. */
2:
    ldr   r2, =__bss_size
    mov   r4, #0
3:
    cmp   r2, #0
    beq   4f
    strb  r4, [r3], #1
    subs  r2, r2, #1
    bne   3b

    /* Standard ARM RWPI static base. */
4:
    ldr   r9, =HP39_RAM_BASE

    /* Pass the launcher context to the C runtime as an ordinary argument. */
    mov   r0, r1
    bl    hpclang_start

    /* Return to the original stack before removing the virtual mapping. */
    ldr   sp, [sp]

    mov   r0, #0
    ldr   r2, =HP39_MMU_L1_SLOT
    str   r0, [r2]
    bl    hp39_flush_cache

    /*
     * Preserve the historical launcher ownership convention for an allocated
     * RAM block: the saved launcher register block pointer is r1 at [sp,#4],
     * and its B.HI allocation handle is at +0x918.
     */
    ldr   r1, [sp, #4]
    ldr   r1, [r1, #0x918]
    tst   r1, r1
    beq   5f
    mov   r0, #101
    add   lr, pc, #0
    swi   0
5:
    ldmfd sp!, {r0-r12, pc}
.size _loader, .-_loader


/*
 * The cache/TLB operation must execute in a privileged exception context.
 * The launcher/runtime convention permits temporarily replacing the SWI
 * vector, entering through SWI 0, then restoring the original vector.
 */
.type hp39_flush_cache,%function
hp39_flush_cache:
    stmfd sp!, {r0-r1, lr}
    ldr   r0, =HP39_VECTOR_SWI
    adr   r1, hp39_flush_handler
    swp   r1, r1, [r0]
    add   lr, pc, #0
    swi   0
    str   r1, [r0]
    ldmfd sp!, {r0-r1, pc}
.size hp39_flush_cache, .-hp39_flush_cache


.type hp39_flush_handler,%function
hp39_flush_handler:
    stmfd sp!, {r0, lr}
    mov   r0, #0
    mcr   p15, 0, r0, c7, c6, 0
    nop
    nop
    mcr   p15, 0, r0, c8, c6, 0
    nop
    nop
    ldmfd sp!, {r0, lr}
    movs  pc, lr
.size hp39_flush_handler, .-hp39_flush_handler
