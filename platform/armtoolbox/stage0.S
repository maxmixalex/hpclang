.syntax unified
.arm

#ifndef HPCLANG_STAGE0_MODEL
#error HPCLANG_STAGE0_MODEL must identify the packaged calculator model
#endif

.equ STAGE_WINDOW_BASE, 0x09000000
.equ STAGE1_BASE,       0x09000400
.equ STAGE_L1_SLOT,     0x07f00240
.equ SWI_VECTOR,        0x08000008

.section .text.stage0.entry,"ax",%progbits
.global hpclang_armtoolbox_stage0
.type hpclang_armtoolbox_stage0,%function
hpclang_armtoolbox_stage0:
#if defined(HPCLANG_STAGE0_ARMSYS_PROBE) && \
    (HPCLANG_STAGE0_MODEL == 2 || HPCLANG_STAGE0_MODEL == 3)
    /*
     * Stock HP39gs/40gs enters stage0 through Saturn ARMSYS. Its ABI passes
     * the post-ARMSYS Saturn PC in r0, SAT_CPU/ARMSYS state in r1 and the
     * emulator continuation in lr. Direct E2E has no live Read[] map and
     * therefore skips this path.
     *
     * Prove the reverse edge without touching RPL state: redirect C.LO to a
     * local resume label, return r0-5 so the just-executed five-nibble ARMSYS
     * instruction is replayed, then continue through the ordinary stage0.
     * The normal SDK/hardware/storage launchers never define this probe.
     */
    ldr     r2, [r1, #0x1f0]        /* Read[0x7c]: HP39 main-font page */
    cmp     r2, #0
    beq     .Larmsys_probe_skip
    ldr     r2, [r1, #0x1f8]        /* Read[0x7e]: HP39 mini-font page */
    cmp     r2, #0
    beq     .Larmsys_probe_skip
    adr     r2, .Larmsys_probe_resume
    str     r2, [r1, #0x91c]        /* Saturn C low 32 bits / ARMSYS target */
    sub     r0, r0, #5              /* replay NIBHEX 80BFE */
    bx      lr

.Larmsys_probe_resume:
    /*
     * Save the second ARMSYS call exactly as the ordinary entry would, then
     * restore C.LO to the stage0 primary entry before any bootstrap work.
     */
    stmfd   sp!, {r0-r12, lr}
    adr     r2, hpclang_armtoolbox_stage0
    str     r2, [r1, #0x91c]
    b       .Lstage0_entry_saved

.Larmsys_probe_skip:
#endif
    stmfd   sp!, {r0-r12, lr}

.Lstage0_entry_saved:
    mov     r10, r1                  /* ARMSYS/Saturn state */
    adr     r8, hpclang_armtoolbox_stage0

    /*
     * The temporary 0x09000000 mapping and stage stack belong to this stage0
     * invocation. Keep stock IRQ/FIQ masked for their entire lifetime. The
     * common bootstrap has its own nested save/disable/restore pair; because
     * it now observes this already-masked CPSR, its restore leaves interrupts
     * masked until stage0 has removed the temporary alias below.
     *
     * HP48gII in particular can otherwise take a stock interrupt immediately
     * after the common bootstrap restores L1[0x11]/L1[0x12]. That stock path
     * reuses L1[0x90], replacing this launcher's coarse table while stage1 is
     * still executing from 0x09000000. Preserve the original caller CPSR in
     * callee-saved r9; stage1 already follows AAPCS and stage0 relies on the
     * same rule for r8/r10.
     */
    bl      .Lirq_save_disable
    mov     r9, r0

    /* Install the stock launcher-provided coarse table at 0x09000000. */
    ldr     r0, [r10, #0x920]
    orr     r0, r0, #0x11
    ldr     r2, =STAGE_L1_SLOT
    str     r0, [r2]
    bl      .Lpublish_stage_map

    /* Move to the top of the temporary page pool. */
    ldr     r2, [r10, #0x914]
    mov     r2, r2, lsl #12
    ldr     r3, =STAGE_WINDOW_BASE
    add     r2, r2, r3
    sub     r2, r2, #4
    str     sp, [r2]
    mov     sp, r2

    /* Resolve the two embedded blobs relative to the actual packed image. */
    ldr     r5, =__stage1_blob_start
    add     r5, r8, r5
    ldr     r6, =__stage1_blob_size

    ldr     r7, =__hpc1_blob_start
    add     r7, r8, r7

    /* Copy the fixed-address stage1 image into its temporary execution slot. */
    ldr     r4, =STAGE1_BASE
    mov     r0, r5
    mov     r1, r4
    mov     r2, r6
1:
    cmp     r2, #0
    beq     2f
    ldrb    r3, [r0], #1
    strb    r3, [r1], #1
    subs    r2, r2, #1
    bne     1b

2:
    /* The aligned zero-initialized tail occupies pages, not transfer bytes. */
    ldr     r2, =__stage1_memory_size
    sub     r2, r2, r6
    mov     r3, #0
.Lclear_stage1:
    cmp     r2, #0
    beq     .Lstage1_ready
    strb    r3, [r1], #1
    subs    r2, r2, #1
    bne     .Lclear_stage1
.Lstage1_ready:
    mov     r0, r4
    ldr     r1, =__stage1_memory_size
    bl      .Lpublish_stage_code

    /*
     * stage1(saturn_state, hpc1, stage1_size, model,
     *        saturn_pc, emulator_lr, stock_cpsr, stock_frame,
     *        stage0_code, stage0_size)
     *
     * Copy the HP39 portal seed out of the original ARMSYS call frame before
     * stage1 can temporarily hand execution back to the Stock emulator.
     */
    ldr     r11, [sp]                 /* saved r0-r12/lr Stock frame */
    ldr     r5, [r11, #0]            /* post-ARMSYS Saturn PC */
    ldr     r12, [r11, #52]          /* emulator continuation LR */
    sub     sp, sp, #24
    str     r5, [sp, #0]
    str     r12, [sp, #4]
    str     r9, [sp, #8]             /* caller CPSR before stage0 masking */
    str     r11, [sp, #12]
    str     r8, [sp, #16]            /* original PIC stage0 code */
    ldr     r5, =__stage0_code_size
    str     r5, [sp, #20]

    mov     r0, r10
    mov     r1, r7
    ldr     r2, =__stage1_memory_size
    ldr     r3, =HPCLANG_STAGE0_MODEL
    ldr     r4, =STAGE1_BASE
    mov     lr, pc
    bx      r4
    add     sp, sp, #24

    /* Restore the original stock-launcher stack. */
    ldr     sp, [sp]

    /* Remove only our temporary 0x09000000 alias while IRQ/FIQ stay masked. */
    mov     r0, #0
    ldr     r2, =STAGE_L1_SLOT
    str     r0, [r2]
    bl      .Lpublish_stage_map

    /*
     * The stage window and temporary stack are gone. Restore the exact caller
     * CPSR before invoking any further stock service so pending interrupts see
     * only the original stock address-space state.
     */
    mov     r0, r9
    bl      .Lirq_restore

    /* Release the optional KOS block allocated by the stock launcher. */
    ldr     r1, [r10, #0x918]
    tst     r1, r1
    beq     3f
    mov     r0, #101
    add     lr, pc, #0
    swi     0

3:
    ldmfd   sp!, {r0-r12, pc}
.size hpclang_armtoolbox_stage0, .-hpclang_armtoolbox_stage0


/*
 * Save the caller CPSR and return to the same mode with IRQ/FIQ masked.
 * stage0 can be entered from an unprivileged stock context, so use the same
 * one-shot SWI supervisor mechanism as the common bootstrap helpers.
 */
.type .Lirq_save_disable,%function
.Lirq_save_disable:
    stmfd   sp!, {r1-r5, lr}
    ldr     r2, =SWI_VECTOR
    adr     r3, .Lirq_disable_supervisor
    swp     r4, r3, [r2]
    swi     0
    ldmfd   sp!, {r1-r5, pc}

.align 2
.Lirq_disable_supervisor:
    /* Restore the stock SWI vector before exception-return changes IRQ state. */
    str     r4, [r2]
    mrs     r0, spsr
    orr     r1, r0, #0xc0
    msr     spsr_c, r1
    movs    pc, lr


/* Restore the exact CPSR returned by .Lirq_save_disable. */
.type .Lirq_restore,%function
.Lirq_restore:
    stmfd   sp!, {r1-r5, lr}
    mov     r5, r0
    ldr     r2, =SWI_VECTOR
    adr     r3, .Lirq_restore_supervisor
    swp     r4, r3, [r2]
    mov     r0, r5
    swi     0
    ldmfd   sp!, {r1-r5, pc}

.align 2
.Lirq_restore_supervisor:
    /* A pending IRQ/FIQ may fire on movs. Restore the stock vector first. */
    str     r4, [r2]
    msr     spsr_cxsf, r0
    movs    pc, lr


/*
 * Publish the L1[0x90] install/remove. This is deliberately local to the
 * position-independent stage0; the normal common bootstrap uses sync.S.
 * Restore the stock SWI vector inside the supervisor handler, before
 * exception-return can expose a temporary vector to a pending interrupt.
 */
.type .Lpublish_stage_map,%function
.Lpublish_stage_map:
    stmfd   sp!, {r0-r4, lr}
    ldr     r2, =SWI_VECTOR
    adr     r3, .Lstage_map_supervisor
    swp     r4, r3, [r2]
    swi     0
    ldmfd   sp!, {r0-r4, pc}

.align 2
.Lstage_map_supervisor:
    str     r4, [r2]
    stmfd   sp!, {r0, lr}
    ldr     r0, =STAGE_L1_SLOT
    bic     r0, r0, #31
    mcr     p15, 0, r0, c7, c10, 1
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c8, c7, 0
    nop
    nop
    ldmfd   sp!, {r0, lr}
    movs    pc, lr


/* Clean copied stage1 bytes and invalidate the instruction cache. */
.type .Lpublish_stage_code,%function
.Lpublish_stage_code:
    stmfd   sp!, {r0-r5, lr}
    add     r3, r0, r1
    bic     r0, r0, #31
    add     r3, r3, #31
    bic     r3, r3, #31

    ldr     r2, =SWI_VECTOR
    adr     r1, .Lstage_code_supervisor
    swp     r4, r1, [r2]
    swi     0

    ldmfd   sp!, {r0-r5, pc}

.align 2
.Lstage_code_supervisor:
    str     r4, [r2]
    stmfd   sp!, {r0-r2, lr}
4:
    cmp     r0, r3
    bhs     5f
    mcr     p15, 0, r0, c7, c10, 1
    add     r0, r0, #32
    b       4b
5:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 0
    nop
    nop
    ldmfd   sp!, {r0-r2, lr}
    movs    pc, lr
