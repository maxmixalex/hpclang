.syntax unified
.arm

#include "../../platform/armtoolbox/stage-layout.h"

.section .text.hpclang_kos_swi3,"ax",%progbits
.global hpclang_kos_swi3
.type hpclang_kos_swi3,%function
hpclang_kos_swi3:
    /* Protect the private stage slot across Stock SD services. The caller still
       owns that window for its return code and stack. Keep bookkeeping
       in preserved registers and the HPC1 stack, outside that window. */
    stmfd sp!, {r4-r8, lr}
    ldr r4, =HPCLANG_STAGE_L1_SLOT
    ldr r5, [r4]
    /* KOS returns through the caller LR, not the instruction after SWI. */
    adr lr, .Lkos_service_return
    swi 0
.Lkos_service_return:
    mov r6, r0
    ldr r7, [r4]
    cmp r7, r5
    beq .Lkos_done

    /* Publish the restored descriptor through the Stock SWI supervisor
       boundary, including the cache line and stale TLB translations. */
    ldr r8, =0x08000008
    adr r7, .Lkos_restore_stage
    swp r7, r7, [r8]
    swi 0
.Lkos_done:
    mov r0, r6
    ldmfd sp!, {r4-r8, lr}
    bx lr

.Lkos_restore_stage:
    str r7, [r8]
    str r5, [r4]
    bic r0, r4, #31
    mcr p15, 0, r0, c7, c10, 1
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c8, c7, 0
    nop
    nop
    movs pc, lr
.size hpclang_kos_swi3, .-hpclang_kos_swi3
