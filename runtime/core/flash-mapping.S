.syntax unified
.arm
.text
.p2align 2

/* Internal ARM920T Flash mapping exchange. All callers/code/data are in RAM,
 * with IRQ/FIQ masked. r0 points to two replacement L1 descriptors; the old
 * descriptors are returned in the same array for an exact restoration.
 *
 * ARM DDI 0151C section 2.3.11: 64 indices x 8 segments, 32-byte lines.
 * https://documentation-service.arm.com/static/5e8e2a5b88295d1e18d381bb
 * Clean+invalidate EVERY data-cache entry to cover Stock Flash aliases while
 * retaining dirty RAM. ARM920T has no ARM926 test-clean instruction.
 */
.global hpclang_flash_mapping_swap
.type hpclang_flash_mapping_swap,%function
hpclang_flash_mapping_swap:
    stmfd sp!, {r1-r5, lr}
    ldr r2, =0x08000008
    adr r3, .Lflash_supervisor
    swp r4, r3, [r2]
    swi 0
    ldmfd sp!, {r1-r5, lr}
    bx lr

.p2align 2
.Lflash_supervisor:
    str r4, [r2]
    stmfd sp!, {r0-r5, lr}
    mov r1, #0
.Lflash_index:
    mov r2, #0
.Lflash_segment:
    orr r3, r1, r2
    mcr p15, 0, r3, c7, c14, 2
    add r2, r2, #32
    cmp r2, #256
    bne .Lflash_segment
    adds r1, r1, #0x04000000
    bne .Lflash_index
    mov r1, #0
    mcr p15, 0, r1, c7, c10, 4

    ldr r1, =0x07f00000
    ldmia r0, {r2-r3}
    ldmia r1, {r4-r5}
    stmia r1, {r2-r3}
    stmia r0, {r4-r5}
    mcr p15, 0, r1, c7, c10, 1
    mov r1, #0
    mcr p15, 0, r1, c7, c10, 4
    /* Per-MVA invalidation also removes any locked translation. */
    mcr p15, 0, r1, c8, c5, 1
    mcr p15, 0, r1, c8, c6, 1
    mov r1, #0x00100000
    mcr p15, 0, r1, c8, c5, 1
    mcr p15, 0, r1, c8, c6, 1
    mov r1, #0
    mcr p15, 0, r1, c7, c5, 0
    nop
    nop
    ldmfd sp!, {r0-r5, lr}
    movs pc, lr
.size hpclang_flash_mapping_swap, .-hpclang_flash_mapping_swap
.ltorg

.p2align 2
.global hpclang_flash_read16
.type hpclang_flash_read16,%function
hpclang_flash_read16:
    ldrh r0, [r1]
    bx lr
.size hpclang_flash_read16, .-hpclang_flash_read16

.p2align 2
.global hpclang_flash_write16
.type hpclang_flash_write16,%function
hpclang_flash_write16:
    strh r2, [r1]
    bx lr
.size hpclang_flash_write16, .-hpclang_flash_write16
