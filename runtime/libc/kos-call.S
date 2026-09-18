.syntax unified
.arm

.section .text.hpclang_kos_swi3,"ax",%progbits
.global hpclang_kos_swi3
.type hpclang_kos_swi3,%function
hpclang_kos_swi3:
    swi 0
    bx lr
.size hpclang_kos_swi3, .-hpclang_kos_swi3

/*
 * Some Stock KOS module entry points are not part of hpclang's AAPCS ABI.
 * Keep the application/runtime callee-saved register contract intact across
 * zero-argument KOS calls instead of relying on undocumented module internals.
 */
.section .text.hpclang_kos_call0,"ax",%progbits
.global hpclang_kos_call0
.type hpclang_kos_call0,%function
hpclang_kos_call0:
    mov     r12, r0
    stmfd   sp!, {r4-r11, lr}
    sub     sp, sp, #4
    mov     lr, pc
    bx      r12
    add     sp, sp, #4
    ldmfd   sp!, {r4-r11, pc}
.size hpclang_kos_call0, .-hpclang_kos_call0

.section .text.hpclang_kos_call1,"ax",%progbits
.global hpclang_kos_call1
.type hpclang_kos_call1,%function
hpclang_kos_call1:
    mov     r12, r0
    mov     r0, r1
    stmfd   sp!, {r4-r11, lr}
    sub     sp, sp, #4
    mov     lr, pc
    bx      r12
    add     sp, sp, #4
    ldmfd   sp!, {r4-r11, pc}
.size hpclang_kos_call1, .-hpclang_kos_call1

.section .text.hpclang_kos_call2,"ax",%progbits
.global hpclang_kos_call2
.type hpclang_kos_call2,%function
hpclang_kos_call2:
    mov     r12, r0
    mov     r0, r1
    mov     r1, r2
    stmfd   sp!, {r4-r11, lr}
    sub     sp, sp, #4
    mov     lr, pc
    bx      r12
    add     sp, sp, #4
    ldmfd   sp!, {r4-r11, pc}
.size hpclang_kos_call2, .-hpclang_kos_call2

.section .text.hpclang_kos_call3,"ax",%progbits
.global hpclang_kos_call3
.type hpclang_kos_call3,%function
hpclang_kos_call3:
    mov     r12, r0
    mov     r0, r1
    mov     r1, r2
    mov     r2, r3
    stmfd   sp!, {r4-r11, lr}
    sub     sp, sp, #4
    mov     lr, pc
    bx      r12
    add     sp, sp, #4
    ldmfd   sp!, {r4-r11, pc}
.size hpclang_kos_call3, .-hpclang_kos_call3
