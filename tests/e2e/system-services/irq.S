.syntax unified
.arm
.text
.align 2

/* Independent read-only oracle for the runtime's IRQ-bank exchange. */
.global hpclang_e2e_irq_snapshot
.type hpclang_e2e_irq_snapshot,%function
hpclang_e2e_irq_snapshot:
    push {r1-r6, lr}
    ldr r4, =0x08000008
    adr r5, .Lsnapshot
    swp r5, r5, [r4]
    swi 0
    pop {r1-r6, lr}
    bx lr
.Lsnapshot:
    str r5, [r4]
    mrs r2, cpsr
    bic r3, r2, #31
    orr r3, r3, #0xd2
    msr cpsr_c, r3
    str sp, [r0]
    str lr, [r0, #4]
    mrs r3, spsr
    str r3, [r0, #8]
    msr cpsr_c, r2
    movs pc, lr
.size hpclang_e2e_irq_snapshot, .-hpclang_e2e_irq_snapshot

/* No SDK polling or callback: only the actual exception can set the witness. */
.global hpclang_e2e_irq_handler
.type hpclang_e2e_irq_handler,%function
hpclang_e2e_irq_handler:
    push {r0-r3, ip, lr}
    ldr r0, =0x07100000
    ldr r1, [r0, #16]
    tst r1, #0x4000
    beq .Lmask
    ldr r1, =hpclang_e2e_irq_hits
    ldr r2, [r1]
    add r2, r2, #1
    str r2, [r1]
    mov r1, #0x4000
    str r1, [r0]
    str r1, [r0, #16]
.Lmask:
    mvn r1, #0
    str r1, [r0, #8]
    pop {r0-r3, ip, lr}
    subs pc, lr, #4
.size hpclang_e2e_irq_handler, .-hpclang_e2e_irq_handler
