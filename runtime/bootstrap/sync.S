.syntax unified
.arm
.text
.align 2

/*
 * Publish hpclang page-table changes on the ARM920T.
 *
 * r0 = start of the 4 KiB L2 workspace (virtual address)
 * r1 = active L1 table (virtual address)
 * r2 = optional section-alias L1 index, or zero when disabled
 *
 * The stock calculator environment exposes the SWI vector at 0x08000008.
 * HP39/40, HP48gII and HP49/50 launcher sources all use SWI 0 for privileged
 * ARM system work; HP39 real hardware and the later 49/50 launcher both use
 * this vector-patching mechanism for cache/MMU maintenance.
 *
 * Every temporary SWI handler restores the original vector while still in
 * supervisor exception context, before exception-return can re-enable the
 * caller's IRQ/FIQ state. Restoring it in the caller after SWI return leaves a
 * race where a pending interrupt can observe the temporary handler.
 */
.global hpclang_mmu_sync
.type hpclang_mmu_sync,%function
hpclang_mmu_sync:
    stmfd   sp!, {r2-r5, lr}
    mov     r5, r2

    ldr     r2, =0x08000008
    adr     r3, .Lsupervisor
    swp     r4, r3, [r2]

    /*
     * Preserve the MMU arguments across SWI entry:
     * r0 = L2 workspace, r1 = L1 table, r5 = optional alias L1 index.
     */
    swi     0

    ldmfd   sp!, {r2-r5, pc}

.align 2
.Lsupervisor:
    str     r4, [r2]
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
     * L1[0x11] and L1[0x12] are adjacent and share a cache line. The HP39/40
     * physical-NOR alias is selected dynamically, so clean the cache line that
     * contains the actual selected descriptor after both install and restore.
     */
    cmp     r5, #0
    beq     .Lalias_clean_done
    add     r2, r1, r5, lsl #2
    bic     r2, r2, #31
    mcr     p15, 0, r2, c7, c10, 1
.Lalias_clean_done:

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

    ldmfd   sp!, {r2-r5, pc}

.align 2
.Lclean_supervisor:
    str     r4, [r2]
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


/*
 * Save the caller CPSR and return with IRQ/FIQ masked.
 *
 * Do this through the same temporary SWI-0 supervisor trampoline used by the
 * MMU/cache helpers. hpclang_bootstrap_run may be entered from an unprivileged
 * stock-launcher context, where a direct MSR CPSR_c cannot be relied upon.
 *
 * Return value (r0): caller CPSR before masking.
 */
.global hpclang_irq_save_disable
.type hpclang_irq_save_disable,%function
hpclang_irq_save_disable:
    stmfd   sp!, {r1-r5, lr}

    ldr     r2, =0x08000008
    adr     r3, .Lirq_disable_supervisor
    swp     r4, r3, [r2]
    swi     0

    ldmfd   sp!, {r1-r5, pc}

.align 2
.Lirq_disable_supervisor:
    str     r4, [r2]
    /*
     * SPSR_svc is the exact caller CPSR captured by SWI entry. Return it in
     * r0, but modify the exception-return SPSR so the caller resumes with both
     * IRQ and FIQ masked.
     */
    mrs     r0, spsr
    orr     r1, r0, #0xc0
    msr     spsr_c, r1
    movs    pc, lr
.size hpclang_irq_save_disable, .-hpclang_irq_save_disable


/*
 * Restore the caller CPSR saved by hpclang_irq_save_disable().
 *
 * r0 = original caller CPSR
 */
.global hpclang_irq_restore
.type hpclang_irq_restore,%function
hpclang_irq_restore:
    stmfd   sp!, {r1-r5, lr}
    mov     r5, r0

    ldr     r2, =0x08000008
    adr     r3, .Lirq_restore_supervisor
    swp     r4, r3, [r2]
    mov     r0, r5
    swi     0

    ldmfd   sp!, {r1-r5, pc}

.align 2
.Lirq_restore_supervisor:
    /*
     * The saved CPSR may re-enable IRQ/FIQ on exception return. Restore the
     * stock SWI vector first so a pending keyboard/timer interrupt can never
     * observe this one-shot handler after movs pc,lr.
     */
    str     r4, [r2]
    msr     spsr_cxsf, r0
    movs    pc, lr
.size hpclang_irq_restore, .-hpclang_irq_restore
