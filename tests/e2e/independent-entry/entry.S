.syntax unified
.arm
.section .entry,"ax",%progbits
.global entry
entry:
 stmdb sp!, {r0-r12,lr}
 mov r4, sp
 adr r6, entry
 ldr r2, control_offset
 add r7, r6, r2
 ldr r8, swi_address
 ldr r2, [r8]
 str r2, [r7, #12]
 bl hpclang_irq_save_disable
 mov r5, r0
 str r5, [r7]
 ldr r2, stack_offset
 add sp, r6, r2
 bic sp, sp, #7
 ldr r2, workspace_offset
 add r0, r6, r2
 mov r1, r7
 bl exercise
 mov r0, r5
 bl hpclang_irq_restore
 mrs r2, cpsr
 str r2, [r7, #8]
 ldr r2, [r8]
 str r2, [r7, #16]
 mov sp, r4
 ldmia sp!, {r0-r12,lr}
 bx lr
control_offset: .word control-entry
swi_address: .word 0x08000008
stack_offset: .word stack_end-entry
workspace_offset: .word workspace-entry
.balign 8
.global control
control: .space 20
.global workspace
workspace: .space 536
.global guard_low
guard_low: .word 0x13579bdf, 0x2468ace0
.global stack_start
stack_start: .space 4096
.global stack_end
stack_end:
.global guard_high
guard_high: .word 0x89abcdef, 0xfedcba98
