#ifndef HP39GPLUS_DUMP_OFFSET
#define HP39GPLUS_DUMP_OFFSET 0
#endif

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
.equ DUMP_OFFSET,        HP39GPLUS_DUMP_OFFSET
.equ PASS_WORD,          0x50415353
.equ MAGIC_MASK,         0xa5a5a5a5
.equ MAGIC0_ENC,         0xf79c96ed
.equ MAGIC1_ENC,         0x9495e8ea

.section .text.rom_dump,"ax",%progbits
.global hp39_rom_dump
.type hp39_rom_dump,%function
hp39_rom_dump:
    stmfd   sp!, {r0-r12, lr}
    mov     r10, r1

    /* Old PrRUN provides the output byte count in Saturn B.LO and its
       byte-aligned ARM pointer in Saturn C.HI. */
    ldr     r5, [r10, #SAT_C_HI]
    ldr     r6, [r10, #SAT_B_LO]
    cmp     r5, #0
    beq     .Lreturn
    ldr     r0, =OUTPUT_BYTES
    cmp     r6, r0
    blo     .Lreturn

    /*
     * Header is exactly 16 bytes so each run keeps the already-proven
     * 0x4010-byte result size:
     *
     *   H39ROM01 | physical_offset | PASS xor byte_sum
     *
     * Keep the raw magic out of the embedded program image so host extraction
     * cannot match the payload itself.
     */
    ldr     r4, =MAGIC_MASK
    ldr     r0, =MAGIC0_ENC
    eor     r0, r0, r4
    mov     r1, r5
    bl      .Lstore_u32
    ldr     r0, =MAGIC1_ENC
    eor     r0, r0, r4
    bl      .Lstore_u32
    ldr     r0, =DUMP_OFFSET
    bl      .Lstore_u32
    mov     r0, #0
    bl      .Lstore_u32

    /* Borrow one unused high L1 section. Physical NOR stays read-only. */
    ldr     r7, =L1_SLOT
    ldr     r8, [r7]
    cmp     r8, #0
    bne     .Lreturn
    ldr     r0, =FLASH_SECTION_DESC
    str     r0, [r7]
    bl      .Lpublish_map

    ldr     r0, =FLASH_ALIAS
    ldr     r2, =DUMP_OFFSET
    add     r0, r0, r2
    add     r1, r5, #16
    ldr     r2, =DUMP_BYTES
    mov     r9, #0
.Lcopy:
    ldrb    r3, [r0], #1
    strb    r3, [r1], #1
    add     r9, r9, r3
    subs    r2, r2, #1
    bne     .Lcopy

    /* Restore the exact original map before advertising completion. */
    str     r8, [r7]
    bl      .Lpublish_map

    ldr     r0, =PASS_WORD
    eor     r0, r0, r9
    add     r1, r5, #12
    bl      .Lstore_u32

    /* Publish the complete result back to Saturn before returning to PrRUN. */
    mov     r0, r5
    ldr     r1, =OUTPUT_BYTES
    bl      .Lpublish_output

.Lreturn:
    ldmfd   sp!, {r0-r12, pc}
.size hp39_rom_dump, .-hp39_rom_dump

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
