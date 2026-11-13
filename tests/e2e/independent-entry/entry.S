.syntax unified
.arm
.section .entry,"ax",%progbits
.global entry
entry:
 stmdb sp!, {r0-r12,lr}
 mov r4, sp
 mrs r5, cpsr
 orr r2, r5, #0xc0
 msr cpsr_c, r2
 adr r6, entry
 ldr r2, stack_offset
 add sp, r6, r2
 bic sp, sp, #7
 ldr r2, workspace_offset
 add r0, r6, r2
 bl exercise
 mov sp, r4
 msr cpsr_c, r5
 ldmia sp!, {r0-r12,lr}
 bx lr
stack_offset: .word stack_end-entry
workspace_offset: .word workspace-entry
.balign 8
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
