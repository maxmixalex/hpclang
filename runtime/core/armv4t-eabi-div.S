.syntax unified
.arm
.text
.align 2

/*
 * ARMv4T-safe AAPCS/EABI 64-bit divmod wrappers.
 *
 * LLVM compiler-rt's generic ARM __aeabi_{u,l}ldivmod wrappers return with
 * "pop {..., pc}".  That return sequence cannot interwork from ARM state back
 * to a Thumb caller on ARMv4T.  HPC1 applications are Thumb-1 by default, so
 * keep compiler-rt's actual __{u,}divmoddi4 implementation but provide wrappers
 * whose epilogue restores LR explicitly and returns through BX.
 */

.global __aeabi_uldivmod
.type __aeabi_uldivmod,%function
__aeabi_uldivmod:
    stmfd   sp!, {r6, lr}
    sub     sp, sp, #16
    add     r6, sp, #8
    str     r6, [sp]
    bl      __udivmoddi4
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    add     sp, sp, #16
    ldmfd   sp!, {r6, lr}
    bx      lr
.size __aeabi_uldivmod, .-__aeabi_uldivmod


.global __aeabi_ldivmod
.type __aeabi_ldivmod,%function
__aeabi_ldivmod:
    stmfd   sp!, {r6, lr}
    sub     sp, sp, #16
    add     r6, sp, #8
    str     r6, [sp]
    bl      __divmoddi4
    ldr     r2, [sp, #8]
    ldr     r3, [sp, #12]
    add     sp, sp, #16
    ldmfd   sp!, {r6, lr}
    bx      lr
.size __aeabi_ldivmod, .-__aeabi_ldivmod
