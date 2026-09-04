.syntax unified
.arm
.text
.align 2

/*
 * Publish hpclang page-table changes on the ARM920T.
 *
 * r0 = start of the 4 KiB L2 workspace (virtual address)
 * r1 = active L1 table (virtual address)
 *
 * The stock calculator environment exposes the SWI vector at 0x08000008.
 * HP39/40, HP48gII and HP49/50 launcher sources all use SWI 0 for privileged
 * ARM system work; HP39 real hardware and the later 49/50 launcher both use
 * this vector-patching mechanism for cache/MMU maintenance.
 */
.global hpclang_mmu_sync
.type hpclang_mmu_sync,%function
hpclang_mmu_sync:
    stmfd   sp!, {r2-r5, lr}

    ldr     r2, =0x08000008
    adr     r3, .Lsupervisor
    swp     r4, r3, [r2]

    /*
     * Preserve the two MVAs as supervisor arguments:
     * r0 = L2 workspace, r1 = L1 table.
     */
    swi     0

    str     r4, [r2]
    ldmfd   sp!, {r2-r5, pc}

.align 2
.Lsupervisor:
    stmfd   sp!, {r0-r3, lr}

    /*
     * Clean the first 2 KiB of the workspace, which contains both coarse
     * tables. ARM920T D-cache lines are 32 bytes.
     */
    mov     r2, r0
    add     r3, r0, #2048
1:
    mcr     p15, 0, r2, c7, c10, 1
    add     r2, r2, #32
    cmp     r2, r3
    blo     1b

    /*
     * L1[0x11] and L1[0x12] are adjacent and share a cache line. Clean that
     * line after either install or restore.
     */
    add     r2, r1, #(0x11 * 4)
    bic     r2, r2, #31
    mcr     p15, 0, r2, c7, c10, 1

    mov     r2, #0
    mcr     p15, 0, r2, c7, c10, 4  /* drain write buffer */
    mcr     p15, 0, r2, c8, c7, 0   /* invalidate unified TLB */
    mcr     p15, 0, r2, c7, c5, 0   /* invalidate I-cache */

    nop
    nop
    ldmfd   sp!, {r0-r3, lr}
    movs    pc, lr
.size hpclang_mmu_sync, .-hpclang_mmu_sync


/*
 * void hpclang_cache_clean(const void *start, uint32_t size)
 *
 * Clean staged payload bytes through their current virtual alias before those
 * physical pages are exposed at HPCLANG_CODE_BASE.
 */
.global hpclang_cache_clean
.type hpclang_cache_clean,%function
hpclang_cache_clean:
    stmfd   sp!, {r2-r5, lr}

    add     r3, r0, r1
    bic     r0, r0, #31
    add     r3, r3, #31
    bic     r3, r3, #31

    ldr     r2, =0x08000008
    adr     r1, .Lclean_supervisor
    swp     r4, r1, [r2]
    swi     0
    str     r4, [r2]

    ldmfd   sp!, {r2-r5, pc}

.align 2
.Lclean_supervisor:
    stmfd   sp!, {r0-r2, lr}
1:
    cmp     r0, r3
    bhs     2f
    mcr     p15, 0, r0, c7, c10, 1
    add     r0, r0, #32
    b       1b
2:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 0
    nop
    nop
    ldmfd   sp!, {r0-r2, lr}
    movs    pc, lr
.size hpclang_cache_clean, .-hpclang_cache_clean
