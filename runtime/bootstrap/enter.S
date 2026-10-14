.syntax unified
.arm
.text
.align 2

/*
 * int hpclang_enter(uintptr_t entry, uintptr_t stack_top)
 *
 * Keep the platform launcher's stack completely outside the HPC1 writable
 * mapping. The application gets an 8-byte aligned stack at the top of its
 * mapped arena; on return we restore the launcher's original stack.
 */
.global hpclang_enter
.type hpclang_enter,%function
hpclang_enter:
    stmfd   sp!, {r4-r11, lr}
    mov     r4, sp

    bic     r1, r1, #7
    mov     sp, r1

    mov     lr, pc
    bx      r0

    mov     sp, r4
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
.size hpclang_enter, .-hpclang_enter
