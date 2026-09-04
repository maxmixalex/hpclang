.syntax unified
.arm

.section .text.stage1.entry,"ax",%progbits
.global hpclang_armtoolbox_stage1_entry
.type hpclang_armtoolbox_stage1_entry,%function
hpclang_armtoolbox_stage1_entry:
    b hpclang_armtoolbox_stage1
.size hpclang_armtoolbox_stage1_entry, .-hpclang_armtoolbox_stage1_entry
