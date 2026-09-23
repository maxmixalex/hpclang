.syntax unified
.arm

/*
 * HP39g+ ROM stream experiment entry.
 *
 * Map the physical NOR flash and the four S3C2410 peripheral sections used by
 * the polling Raw HID transport into otherwise-unused high virtual sections.
 * The stock HP39 MMU table lives at 0x07f00000.  All borrowed entries must be
 * invalid before we touch them; otherwise we return without changing anything.
 *
 * The retained 16 KiB probe established L1_FLASH=0x07f00280 for virtual
 * section 0x0a. L1 descriptors are 4 bytes each, so the adjacent 0x0b..0x0e
 * aliases are +4 bytes per section.
 */
.equ L1_FLASH,       0x07f00280  /* VA 0x0a000000 */
.equ L1_USBD,        0x07f00284  /* VA 0x0b000000 */
.equ L1_CLK,         0x07f00288  /* VA 0x0c000000 */
.equ L1_GPIO,        0x07f0028c  /* VA 0x0d000000 */
.equ L1_INT,         0x07f00290  /* VA 0x0e000000 */
.equ SWI_VECTOR,     0x08000008

.equ DESC_FLASH,     0x00000c12
.equ DESC_USBD,      0x52000c12
.equ DESC_CLK,       0x4c000c12
.equ DESC_GPIO,      0x56000c12
.equ DESC_INT,       0x4a000c12

.section .text.rom_stream_entry,"ax",%progbits
.global hp39_rom_stream
.type hp39_rom_stream,%function
hp39_rom_stream:
    stmfd   sp!, {r0-r12, lr}

    /* Refuse to borrow any virtual section that the stock OS already uses. */
    ldr     r4, =L1_FLASH
    ldr     r5, =L1_USBD
    ldr     r6, =L1_CLK
    ldr     r7, =L1_GPIO
    ldr     r8, =L1_INT
    ldr     r0, [r4]
    cmp     r0, #0
    bne     .Lreturn
    ldr     r0, [r5]
    cmp     r0, #0
    bne     .Lreturn
    ldr     r0, [r6]
    cmp     r0, #0
    bne     .Lreturn
    ldr     r0, [r7]
    cmp     r0, #0
    bne     .Lreturn
    ldr     r0, [r8]
    cmp     r0, #0
    bne     .Lreturn

    ldr     r0, =DESC_FLASH
    str     r0, [r4]
    ldr     r0, =DESC_USBD
    str     r0, [r5]
    ldr     r0, =DESC_CLK
    str     r0, [r6]
    ldr     r0, =DESC_GPIO
    str     r0, [r7]
    ldr     r0, =DESC_INT
    str     r0, [r8]
    bl      .Lpublish_maps

    bl      hp39_rom_stream_main

    /* Exact precondition was five invalid descriptors, so restore five zeros. */
    mov     r0, #0
    str     r0, [r4]
    str     r0, [r5]
    str     r0, [r6]
    str     r0, [r7]
    str     r0, [r8]
    bl      .Lpublish_maps

.Lreturn:
    ldmfd   sp!, {r0-r12, pc}
.size hp39_rom_stream, .-hp39_rom_stream

/* Publish the five modified L1 descriptors and invalidate the unified TLB. */
.type .Lpublish_maps,%function
.Lpublish_maps:
    stmfd   sp!, {r0-r4, lr}
    ldr     r2, =SWI_VECTOR
    adr     r3, .Lmap_supervisor
    swp     r4, r3, [r2]
    swi     0
    str     r4, [r2]
    ldmfd   sp!, {r0-r4, pc}

.align 2
.Lmap_supervisor:
    stmfd   sp!, {r0-r2, lr}
    ldr     r0, =L1_FLASH
    bic     r0, r0, #31
    ldr     r1, =L1_INT
    add     r1, r1, #32
    bic     r1, r1, #31
.Lclean_l1:
    cmp     r0, r1
    bhs     .Lclean_l1_done
    mcr     p15, 0, r0, c7, c10, 1
    add     r0, r0, #32
    b       .Lclean_l1
.Lclean_l1_done:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c8, c7, 0
    nop
    nop
    ldmfd   sp!, {r0-r2, lr}
    movs    pc, lr
