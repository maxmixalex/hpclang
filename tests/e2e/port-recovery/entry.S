/* Recovery witness ABI: main.c asserts workspace size and restored-CPSR offset.
 * All image offsets are relative to entry; never install an absolute RAM pointer.
 * The copied image owns TLS/BSS, workspace and a guarded 4 KiB C stack.
 */
.syntax unified
.arm
.section .entry,"ax",%progbits
.global entry
entry:
 stmdb sp!, {r0-r12,lr}
 mov r4, sp
 bl hpclang_irq_save_disable
 mov r5, r0
 adr r6, entry
 ldr r2, zero_start_offset
 add r2, r6, r2
 ldr r3, image_end_offset
 add r3, r6, r3
 mov r12, #0
.Lzero:
 cmp r2, r3
 strlo r12, [r2], #4
 blo .Lzero
 ldr r2, guard_low_offset
 add r2, r6, r2
 ldr r3, guard_values
 str r3, [r2]
 ldr r3, guard_values+4
 str r3, [r2, #4]
 ldr r2, guard_high_offset
 add r2, r6, r2
 ldr r3, guard_values+8
 str r3, [r2]
 ldr r3, guard_values+12
 str r3, [r2, #4]
 ldr r2, stack_offset
 add sp, r6, r2
 bic sp, sp, #7
 ldr r2, workspace_offset
 add r0, r6, r2
 mov r1, r5
 mov r7, r0
 bl exercise
 mov sp, r4
 mov r0, r5
 bl hpclang_irq_restore
 mrs r2, cpsr
 add r3, r7, #8192
 str r2, [r3, #108]
 ldmia sp!, {r0-r12,lr}
 bx lr
stack_offset: .word stack_end-entry
workspace_offset: .word workspace-entry
zero_start_offset: .word __zero_start-entry
image_end_offset: .word __image_end-entry
guard_low_offset: .word guard_low-entry
guard_high_offset: .word guard_high-entry
guard_values: .word 0x13579bdf, 0x2468ace0, 0x89abcdef, 0xfedcba98
.section .bss.owned,"aw",%nobits
.balign 8
.global workspace
workspace: .space 8400
.global guard_low
guard_low: .space 8
.global stack_start
stack_start: .space 4096
.global stack_end
stack_end:
.global guard_high
guard_high: .space 8

.section .text.tp,"ax",%progbits
.global __aeabi_read_tp
__aeabi_read_tp:
 ldr r0, .Ltpoff
.Ltpadd:
 add r0, pc, r0
 bx lr
.Ltpoff: .word __tls_base-8-(.Ltpadd+8)

.section .text.cpsr,"ax",%progbits
.arm
.global recovery_cpsr
.type recovery_cpsr,%function
recovery_cpsr:
 mrs r0, cpsr
 bx lr
