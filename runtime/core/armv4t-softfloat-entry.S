.syntax unified
.arm
.text
.align 2

/*
 * ARMv4T-safe entry veneers for the generic double-precision soft-float core.
 * The core itself is compiled in ARM state with -march=armv4t, so all of its
 * compiler helper calls remain ARM-to-ARM. These veneers are the only
 * Thumb-to-ARM boundary and always return through BX LR.
 */

.macro SOFT_ENTRY public, internal
.global \public
.type \public,%function
\public:
    stmfd   sp!, {r4, lr}
    bl      \internal
    ldmfd   sp!, {r4, lr}
    bx      lr
.size \public, .-\public
.endm

SOFT_ENTRY __aeabi_dmul, hpclang_soft_dmul
SOFT_ENTRY __aeabi_dadd, hpclang_soft_dadd
SOFT_ENTRY __aeabi_dsub, hpclang_soft_dsub
SOFT_ENTRY __aeabi_dcmpeq, hpclang_soft_dcmpeq
SOFT_ENTRY __aeabi_dcmplt, hpclang_soft_dcmplt
SOFT_ENTRY __aeabi_dcmple, hpclang_soft_dcmple
SOFT_ENTRY __aeabi_dcmpge, hpclang_soft_dcmpge
SOFT_ENTRY __aeabi_dcmpgt, hpclang_soft_dcmpgt
SOFT_ENTRY __aeabi_dcmpun, hpclang_soft_dcmpun
