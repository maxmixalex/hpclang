.syntax unified
.arm

/*
 * hpclang Launcher V3 simple/MMU ARM-side loader
 *
 * This is a new implementation of the common ARM-side contract consumed by
 * HPGCC 1.x MMU programs. Historical ARMToolbox launchers for HP39, HP48gII
 * and HP49/50 normalize their family-specific Saturn memory handling into the
 * same ARMSYS register block before entering ARM code.
 *
 * Entry contract:
 *   r1 -> ARMSYS/Saturn register block
 *
 * Fields used here (also used by historical MMUloader/SYSMEM helpers):
 *   +0x914  number of 4 KiB MMU pages
 *   +0x918  optional OS/KOS RAM allocation handle (B.HI)
 *   +0x920  physical address of coarse page table
 *   +0x91c  launcher jump field (not required by this loader)
 *
 * hpclang application ABI:
 *   code + read-only data: ROPI
 *   writable data:        RWPI, r9/sb = 0x09000400
 *
 * The Saturn-side launcher remains family-specific. This file deliberately
 * contains no HP39 aplet, HP48 mapping or HP49 Port1 logic.
 */

.equ HPCLANG_RWPI_BASE,      0x09000400
.equ HPCLANG_MMU_L1_SLOT,    0x07f00240
.equ HPCLANG_VECTOR_SWI,     0x08000008

.section .text.entry,"ax",%progbits
.global _loader
.type _loader,%function
_loader:
    stmfd sp!, {r0-r12, lr}

    /*
     * Map the launcher-generated coarse page table into the common
     * 0x09000000 virtual window.
     */
    ldr   r2, =HPCLANG_MMU_L1_SLOT
    ldr   r0, [r1, #0x920]
    orr   r0, r0, #0x11
    str   r0, [r2]
    bl    hpclang_flush_cache

    /*
     * Relocate the stack to the top of the mapped page list and preserve the
     * original stack pointer in the last mapped word.
     */
    ldr   r2, [r1, #0x914]
    mov   r2, r2, lsl #12
    add   r2, r2, #0x09000000
    sub   r2, r2, #4
    str   sp, [r2]
    mov   sp, r2

    /*
     * _loader is linked at VMA zero. ADR therefore gives the actual packed
     * image base after the calculator-side launcher places the payload.
     */
    adr   r8, _loader

    /* Copy initialized RWPI data from packed LMA to the virtual data window. */
    ldr   r0, =__data_load
    add   r0, r8, r0
    ldr   r2, =__data_size
    ldr   r3, =HPCLANG_RWPI_BASE
1:
    cmp   r2, #0
    beq   2f
    ldrb  r4, [r0], #1
    strb  r4, [r3], #1
    subs  r2, r2, #1
    bne   1b

    /* Clear BSS immediately after initialized writable data. */
2:
    ldr   r2, =__bss_size
    mov   r4, #0
3:
    cmp   r2, #0
    beq   4f
    strb  r4, [r3], #1
    subs  r2, r2, #1
    bne   3b

4:
    /* Standard ARM RWPI static base. */
    ldr   r9, =HPCLANG_RWPI_BASE

    /* Pass the common launcher register block to the higher-level runtime. */
    mov   r0, r1
    bl    hpclang_start

    /* Restore the original stack before tearing down the virtual mapping. */
    ldr   sp, [sp]

    mov   r0, #0
    ldr   r2, =HPCLANG_MMU_L1_SLOT
    str   r0, [r2]
    bl    hpclang_flush_cache

    /*
     * Launcher V3 R-mode may have allocated an OS/KOS block. Historical
     * MMUloader frees the handle found at +0x918 through KOS syscall 101.
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


.type hpclang_flush_cache,%function
hpclang_flush_cache:
    stmfd sp!, {r0-r1, lr}
    ldr   r0, =HPCLANG_VECTOR_SWI
    adr   r1, hpclang_flush_handler
    swp   r1, r1, [r0]
    add   lr, pc, #0
    swi   0
    str   r1, [r0]
    ldmfd sp!, {r0-r1, pc}
.size hpclang_flush_cache, .-hpclang_flush_cache


.type hpclang_flush_handler,%function
hpclang_flush_handler:
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
.size hpclang_flush_handler, .-hpclang_flush_handler
