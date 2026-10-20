.syntax unified
.arm
.section .text.tp,"ax",%progbits
.global __aeabi_read_tp
__aeabi_read_tp:
 ldr r0, .Ltpoff
.Ltpadd:
 add r0, pc, r0
 bx lr
.Ltpoff: .word __tls_base-8-(.Ltpadd+8)

