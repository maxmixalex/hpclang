.syntax unified
.arm

#include "hp39-portal-private.h"

.text
.align 2

.global hpclang_hp39_portal_enter
.type hpclang_hp39_portal_enter,%function
hpclang_hp39_portal_enter:
    ldr     r12, =hpclang_hp39_portal_context
    str     r0, [r12, #HP39P_APP_RESULT_OBJ]
    str     r1, [r12, #HP39P_APP_RESULT_PRO]
    str     r2, [r12, #HP39P_REQUEST_KIND]
    ldr     r1, [r12, #HP39P_MAGIC]
    ldr     r2, =HPCLANG_HP39_PORTAL_MAGIC_VALUE
    cmp     r1, r2
    bne     .Lportal_unavailable
    ldr     r1, [r12, #HP39P_ACTIVE]
    cmp     r1, #0
    bne     .Lportal_busy

    mov     r1, #1
    str     r1, [r12, #HP39P_ACTIVE]
    str     sp, [r12, #HP39P_APP_SP]
    str     lr, [r12, #HP39P_APP_LR]
    add     r1, r12, #HP39P_APP_REGS
    stmia   r1, {r4-r11}

    add     sp, r12, #HP39P_STACK_END
    bl      hpclang_hp39_portal_prepare_stock
    cmp     r0, #0
    bne     .Lportal_prepare_failed

    ldr     r12, =hpclang_hp39_portal_context
    add     r1, r12, #HP39P_EMULATOR_REGS
    ldmia   r1, {r4-r11}
    ldr     sp, [r12, #HP39P_STOCK_SP]
    ldr     r0, [r12, #HP39P_SATURN_ENTRY]
    ldr     r1, [r12, #HP39P_SATURN_STATE]
    ldr     r2, [r12, #HP39P_EMULATOR_LR]
    bx      r2

.Lportal_prepare_failed:
    mov     r3, r0
    ldr     r12, =hpclang_hp39_portal_context
    mov     r1, #0
    str     r1, [r12, #HP39P_ACTIVE]
    add     r1, r12, #HP39P_APP_REGS
    ldmia   r1, {r4-r11}
    ldr     sp, [r12, #HP39P_APP_SP]
    ldr     lr, [r12, #HP39P_APP_LR]
    mov     r0, r3
    bx      lr

.Lportal_busy:
    mvn     r0, #1
    bx      lr
.Lportal_unavailable:
    mvn     r0, #0
    bx      lr
.size hpclang_hp39_portal_enter, .-hpclang_hp39_portal_enter

.global hpclang_hp39_portal_resume
.type hpclang_hp39_portal_resume,%function
hpclang_hp39_portal_resume:
    ldr     r12, =hpclang_hp39_portal_context
    str     r1, [r12, #HP39P_SATURN_STATE]
    str     lr, [r12, #HP39P_EMULATOR_LR]
    str     sp, [r12, #HP39P_STOCK_SP]
    add     r2, r12, #HP39P_EMULATOR_REGS
    stmia   r2, {r4-r11}

    add     sp, r12, #HP39P_STACK_END
    bl      hpclang_hp39_portal_restore_hpc
    mov     r3, r0

    ldr     r12, =hpclang_hp39_portal_context
    cmp     r3, #0
    bne     .Lportal_result_done
    ldr     r1, [r12, #HP39P_APP_RESULT_OBJ]
    cmp     r1, #0
    ldrne   r0, [r12, #HP39P_RESULT_OBJECT]
    strne   r0, [r1]
    ldr     r1, [r12, #HP39P_APP_RESULT_PRO]
    cmp     r1, #0
    ldrne   r0, [r12, #HP39P_RESULT_PROLOGUE]
    strne   r0, [r1]
.Lportal_result_done:
    add     r1, r12, #HP39P_APP_REGS
    ldmia   r1, {r4-r11}
    ldr     sp, [r12, #HP39P_APP_SP]
    ldr     lr, [r12, #HP39P_APP_LR]
    mov     r0, r3
    bx      lr
.size hpclang_hp39_portal_resume, .-hpclang_hp39_portal_resume
