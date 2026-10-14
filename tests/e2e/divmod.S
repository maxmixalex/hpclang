.syntax unified
.thumb

/* Force the EABI's r0/r1 quotient/remainder calls from Thumb. C division may
 * instead lower to quotient-only calls plus multiplication and miss this edge. */
.section .text.hpclang_test_unsigned_divmod,"ax",%progbits
.p2align 1
.global hpclang_test_unsigned_divmod
.type hpclang_test_unsigned_divmod,%function
.thumb_func
hpclang_test_unsigned_divmod:
    push {r4, lr}
    movs r4, r2
    bl __aeabi_uidivmod
    str r1, [r4]
    pop {r4}
    pop {r2}
    bx r2
.size hpclang_test_unsigned_divmod, .-hpclang_test_unsigned_divmod

.section .text.hpclang_test_signed_divmod,"ax",%progbits
.p2align 1
.global hpclang_test_signed_divmod
.type hpclang_test_signed_divmod,%function
.thumb_func
hpclang_test_signed_divmod:
    push {r4, lr}
    movs r4, r2
    bl __aeabi_idivmod
    str r1, [r4]
    pop {r4}
    pop {r2}
    bx r2
.size hpclang_test_signed_divmod, .-hpclang_test_signed_divmod
