.syntax unified
.arm

.equ SAT_B_LO,           0x914
.equ SAT_C_HI,           0x920
.equ L1_SLOT,            0x07f00280
.equ SWI_VECTOR,         0x08000008
.equ FLASH_ALIAS,        0x0a000000
.equ FLASH_SECTION_DESC, 0x00000c12
.equ OUTPUT_BYTES,       0x00004010
.equ DUMP_BYTES,         0x00004000
.equ PASS_WORD,          0x50415353
.equ MAGIC_MASK,         0xa5a5a5a5
.equ MAGIC0_ENC,         0xe79c96ed
.equ MAGIC1_ENC,         0x94f1eaea

.section .text.boot_dump,"ax",%progbits
.global hp39_boot_dump
.type hp39_boot_dump,%function
hp39_boot_dump:
    stmfd   sp!, {r0-r12, lr}
    mov     r10, r1

    /* Old PrRUN puts allocated output size in Saturn B.LO and its ARM
       pointer in Saturn C.HI before ARMSYS. */
    ldr     r5, [r10, #SAT_C_HI]
    ldr     r6, [r10, #SAT_B_LO]
    cmp     r5, #0
    beq     .Lreturn
    ldr     r0, =OUTPUT_BYTES
    cmp     r6, r0
    blo     .Lreturn

    /* H39BOOT1 + payload length + incomplete status. Keep the literal pool
       free of the raw magic so host extraction cannot match the embedded
       program image by mistake. */
    ldr     r4, =MAGIC_MASK
    ldr     r0, =MAGIC0_ENC
    eor     r0, r0, r4
    mov     r1, r5
    bl      .Lstore_u32
    ldr     r0, =MAGIC1_ENC
    eor     r0, r0, r4
    bl      .Lstore_u32
    ldr     r0, =DUMP_BYTES
    bl      .Lstore_u32
    mov     r0, #0
    bl      .Lstore_u32

    /* Borrow one high L1 section slot. Physical flash remains read-only:
       the descriptor only makes physical 0 visible at 0x0a000000. */
    ldr     r7, =L1_SLOT
    ldr     r8, [r7]
    /* Never borrow a section already owned by the stock environment. */
    cmp     r8, #0
    bne     .Lreturn
    ldr     r0, =FLASH_SECTION_DESC
    str     r0, [r7]
    bl      .Lpublish_map

    ldr     r0, =FLASH_ALIAS
    add     r1, r5, #16
    ldr     r2, =DUMP_BYTES
.Lcopy:
    ldrb    r3, [r0], #1
    strb    r3, [r1], #1
    subs    r2, r2, #1
    bne     .Lcopy

    /* Restore the exact original mapping before advertising completion. */
    str     r8, [r7]
    bl      .Lpublish_map

    ldr     r0, =PASS_WORD
    add     r1, r5, #12
    bl      .Lstore_u32

    /* The old launcher returns this RAM to Saturn immediately. Make every
       byte visible outside the ARM D-cache before returning to PrRUN. */
    mov     r0, r5
    ldr     r1, =OUTPUT_BYTES
    bl      .Lpublish_output

.Lreturn:
    ldmfd   sp!, {r0-r12, pc}
.size hp39_boot_dump, .-hp39_boot_dump

/* Store one little-endian word without assuming ARM word alignment.
   r1 advances by four bytes. */
.type .Lstore_u32,%function
.Lstore_u32:
    strb    r0, [r1], #1
    mov     r0, r0, lsr #8
    strb    r0, [r1], #1
    mov     r0, r0, lsr #8
    strb    r0, [r1], #1
    mov     r0, r0, lsr #8
    strb    r0, [r1], #1
    bx      lr

.type .Lpublish_map,%function
.Lpublish_map:
    stmfd   sp!, {r0-r4, lr}
    ldr     r2, =SWI_VECTOR
    adr     r3, .Lmap_supervisor
    swp     r4, r3, [r2]
    swi     0
    str     r4, [r2]
    ldmfd   sp!, {r0-r4, pc}

.align 2
.Lmap_supervisor:
    stmfd   sp!, {r0, lr}
    ldr     r0, =L1_SLOT
    bic     r0, r0, #31
    mcr     p15, 0, r0, c7, c10, 1
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c8, c7, 0
    nop
    nop
    ldmfd   sp!, {r0, lr}
    movs    pc, lr

.type .Lpublish_output,%function
.Lpublish_output:
    stmfd   sp!, {r0-r5, lr}
    add     r3, r0, r1
    bic     r0, r0, #31
    add     r3, r3, #31
    bic     r3, r3, #31

    ldr     r2, =SWI_VECTOR
    adr     r1, .Loutput_supervisor
    swp     r4, r1, [r2]
    swi     0
    str     r4, [r2]
    ldmfd   sp!, {r0-r5, pc}

.align 2
.Loutput_supervisor:
    stmfd   sp!, {r0-r2, lr}
.Lclean_output:
    cmp     r0, r3
    bhs     .Lclean_output_done
    mcr     p15, 0, r0, c7, c10, 1
    add     r0, r0, #32
    b       .Lclean_output
.Lclean_output_done:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    nop
    nop
    ldmfd   sp!, {r0-r2, lr}
    movs    pc, lr
