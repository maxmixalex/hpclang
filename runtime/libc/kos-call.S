.syntax unified
.arm

.section .text.hpclang_kos_swi3,"ax",%progbits
.global hpclang_kos_swi3
.type hpclang_kos_swi3,%function
hpclang_kos_swi3:
    swi 0
    bx lr
.size hpclang_kos_swi3, .-hpclang_kos_swi3
