.syntax unified
.arm

.section .text.stage1.entry,"ax",%progbits
.global hpclang_armtoolbox_stage1_entry
.type hpclang_armtoolbox_stage1_entry,%function
hpclang_armtoolbox_stage1_entry:
    /* C may move USEROB, including the original stage0. Its return target
       is rebound to a protected PIC copy before mutation is enabled. */
    ldr r12, =hpclang_stage0_resume_target
    str lr, [r12]
    adr lr, .Lstage1_return
    b hpclang_armtoolbox_stage1
.Lstage1_return:
    ldr r12, =hpclang_stage0_resume_target
    ldr pc, [r12]
.size hpclang_armtoolbox_stage1_entry, .-hpclang_armtoolbox_stage1_entry
