.syntax unified
.arm
.section .entry,"ax",%progbits
.global entry
entry:
 stmdb sp!,{r0-r12,lr}
 mov r4,sp
 mov r6,r1
 bl hpclang_irq_save_disable
 mov r5,r0
 adr r7,entry
 ldr r2,zero_offset
 add r2,r7,r2
 ldr r3,end_offset
 add r3,r7,r3
 mov r12,#0
1:
 cmp r2,r3
 strlo r12,[r2],#4
 blo 1b
 ldr r2,low_offset
 add r2,r7,r2
 ldr r3,guard
 str r3,[r2]
 str r3,[r2,#4]
 ldr r2,high_offset
 add r2,r7,r2
 str r3,[r2]
 str r3,[r2,#4]
 mov sp,r2
 mov r0,r6
 bl admission
 cmp r0,#0
 beq .Lfailed
 ldr r2,low_offset
 add r2,r7,r2
 ldr r3,guard
 ldr r0,[r2]
 cmp r0,r3
 ldreq r0,[r2,#4]
 cmpeq r0,r3
 bne .Lfailed
 ldr r2,high_offset
 add r2,r7,r2
 ldr r0,[r2]
 cmp r0,r3
 ldreq r0,[r2,#4]
 cmpeq r0,r3
 bne .Lfailed
 mov sp,r4
 mov r0,r5
 bl hpclang_irq_restore
 ldmia sp!,{r0-r12,lr}
 bx lr
.Lfailed:
 /* Never continue a condition without producing its result. */
 b .Lfailed
zero_offset: .word __zero_start-entry
end_offset: .word __image_end-entry
low_offset: .word admission_stack_low-entry
high_offset: .word admission_stack_high-entry
guard: .word 0x51a7b09d
.section .bss,"aw",%nobits
.balign 8
.global admission_stack_low, admission_stack_high
admission_stack_low: .space 8
.space 2048
admission_stack_high: .space 8
