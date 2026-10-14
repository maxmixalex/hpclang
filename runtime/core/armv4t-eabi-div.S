.syntax unified
.arm
.text
.align 2

/*
 * ARMv4T-safe AAPCS/EABI divmod wrappers.
 *
 * LLVM compiler-rt's generic ARM divmod wrappers restore PC from the stack.
 * That return sequence cannot interwork from ARM state back
 * to a Thumb caller on ARMv4T.  HPC1 applications are Thumb-1 by default, so
 * keep compiler-rt's actual division implementations but provide wrappers
 * whose epilogue restores LR explicitly and returns through BX.
 */

.section .text.__aeabi_uidivmod,"ax",%progbits
.p2align 2
.global __aeabi_uidivmod
.type __aeabi_uidivmod,%function
__aeabi_uidivmod:
    stmfd   sp!, {r4, lr}
    sub     sp, sp, #8
    mov     r2, sp
    bl      __udivmodsi4
    ldr     r1, [sp]
    add     sp, sp, #8
    ldmfd   sp!, {r4, lr}
    bx      lr
.size __aeabi_uidivmod, .-__aeabi_uidivmod

.section .text.__aeabi_idivmod,"ax",%progbits
.p2align 2
.global __aeabi_idivmod
.type __aeabi_idivmod,%function
__aeabi_idivmod:
    stmfd   sp!, {r4, lr}
    sub     sp, sp, #8
    mov     r2, sp
    bl      __divmodsi4
    ldr     r1, [sp]
    add     sp, sp, #8
    ldmfd   sp!, {r4, lr}
    bx      lr
.size __aeabi_idivmod, .-__aeabi_idivmod

.section .text.__aeabi_uldivmod,"ax",%progbits
.p2align 2
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


.section .text.__aeabi_ldivmod,"ax",%progbits
.p2align 2
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
