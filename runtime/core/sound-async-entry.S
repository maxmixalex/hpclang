.syntax unified
.arm
.section .text.hpclang_sound_irq_entry,"ax",%progbits
.align 2
.global hpclang_sound_irq_entry
.type hpclang_sound_irq_entry,%function
hpclang_sound_irq_entry:
    push {r0-r3, ip, lr}
    ldr r0, =0x07100000
    ldr r1, [r0, #16]
    tst r1, #0x200
    beq .Lfault
    ldr ip, =hpclang_sound_irq_dispatch
.Ldispatch:
    mov lr, pc
    bx ip
    b .Lreturn
.Lfault:
    ldr ip, =hpclang_sound_irq_fault
    b .Ldispatch
.Lreturn:
    pop {r0-r3, ip, lr}
    subs pc, lr, #4
.size hpclang_sound_irq_entry, .-hpclang_sound_irq_entry
