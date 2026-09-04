.syntax unified
.arm
.text
.align 2

/*
 * Thumb-1 application code cannot encode CPSR MRS/MSR directly. Keep these
 * two privileged instructions in a tiny ARM interworking helper so the visible
 * smoke remains an ordinary default-Thumb HPC1 application.
 */

.global hp39_irq_save_disable
.type hp39_irq_save_disable,%function
hp39_irq_save_disable:
    mrs     r0, cpsr
    orr     r1, r0, #0xc0
    msr     cpsr_c, r1
    bx      lr
.size hp39_irq_save_disable, .-hp39_irq_save_disable

.global hp39_irq_restore
.type hp39_irq_restore,%function
hp39_irq_restore:
    msr     cpsr_c, r0
    bx      lr
.size hp39_irq_restore, .-hp39_irq_restore
