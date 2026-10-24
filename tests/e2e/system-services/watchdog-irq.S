.syntax unified
.arm
.text
.align 2
/* IRQ-to-Thumb dispatch with an 8-byte aligned owned stack. Foreground code
 * cannot set the witness or advance the deadline. Preserve caller-saved ARM
 * registers and the exception return address across the C handler. */
.global hpclang_e2e_watchdog_handler
.type hpclang_e2e_watchdog_handler,%function
hpclang_e2e_watchdog_handler:
    push {r0-r3, ip, lr}
    ldr r0, =0x07100000
    ldr r1, [r0, #16]
    tst r1, #0x200
    beq .Lmask
    ldr ip, =hpclang_e2e_watchdog_dispatch
    mov lr, pc
    bx ip
    b .Lreturn
.Lmask:
    mvn r1, #0
    str r1, [r0, #8]
.Lreturn:
    pop {r0-r3, ip, lr}
    subs pc, lr, #4
.size hpclang_e2e_watchdog_handler, .-hpclang_e2e_watchdog_handler
