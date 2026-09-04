#include <stdbool.h>
#include <stdint.h>

/*
 * ARMv4T double-precision soft-float core.
 *
 * The algorithms follow LLVM compiler-rt's generic fp_{mul,add,compare}
 * implementations, but are kept here as a narrow target adaptation so they are
 * compiled with hpclang's actual -march=armv4t constraints instead of using
 * ATfE's newer-ISA optimized ARM assembly builtins.
 *
 * This file contains no floating-point arithmetic. double values are only
 * moved to/from their IEEE-754 binary64 representation.
 */

#define D_SIGN UINT64_C(0x8000000000000000)
#define D_ABS  UINT64_C(0x7fffffffffffffff)
#define D_INF  UINT64_C(0x7ff0000000000000)
#define D_QNAN UINT64_C(0x7ff8000000000000)
#define D_QUIET UINT64_C(0x0008000000000000)
#define D_SIG_MASK UINT64_C(0x000fffffffffffff)
#define D_IMPLICIT UINT64_C(0x0010000000000000)
#define D_MAX_EXP 0x7ff
#define D_BIAS 0x3ff

static uint64_t to_bits(double value)
{
    union {
        double d;
        uint64_t u;
    } rep = { .d = value };
    return rep.u;
}

static double from_bits(uint64_t bits)
{
    union {
        double d;
        uint64_t u;
    } rep = { .u = bits };
    return rep.d;
}

static int clz64_nonzero(uint64_t value)
{
    uint32_t high = (uint32_t)(value >> 32);
    if (high != 0)
        return __builtin_clz(high);
    return 32 + __builtin_clz((uint32_t)value);
}

static int normalize(uint64_t *significand)
{
    const int shift = clz64_nonzero(*significand) - 11;
    *significand <<= shift;
    return 1 - shift;
}

static void wide_multiply(
    uint64_t a,
    uint64_t b,
    uint64_t *high,
    uint64_t *low)
{
    const uint32_t a0 = (uint32_t)a;
    const uint32_t a1 = (uint32_t)(a >> 32);
    const uint32_t b0 = (uint32_t)b;
    const uint32_t b1 = (uint32_t)(b >> 32);

    const uint64_t p00 = (uint64_t)a0 * b0;
    const uint64_t p01 = (uint64_t)a0 * b1;
    const uint64_t p10 = (uint64_t)a1 * b0;
    const uint64_t p11 = (uint64_t)a1 * b1;

    const uint64_t r0 = (uint32_t)p00;
    const uint64_t r1 =
        (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;

    *low = r0 + (r1 << 32);
    *high = (p01 >> 32) + (p10 >> 32) + (r1 >> 32) + p11;
}

double hpclang_soft_dmul(double a, double b)
{
    const uint64_t a_rep = to_bits(a);
    const uint64_t b_rep = to_bits(b);
    const unsigned a_exp = (unsigned)((a_rep >> 52) & D_MAX_EXP);
    const unsigned b_exp = (unsigned)((b_rep >> 52) & D_MAX_EXP);
    const uint64_t sign = (a_rep ^ b_rep) & D_SIGN;

    uint64_t a_sig = a_rep & D_SIG_MASK;
    uint64_t b_sig = b_rep & D_SIG_MASK;
    int scale = 0;

    if (a_exp - 1u >= D_MAX_EXP - 1u ||
        b_exp - 1u >= D_MAX_EXP - 1u) {
        const uint64_t a_abs = a_rep & D_ABS;
        const uint64_t b_abs = b_rep & D_ABS;

        if (a_abs > D_INF)
            return from_bits(a_rep | D_QUIET);
        if (b_abs > D_INF)
            return from_bits(b_rep | D_QUIET);

        if (a_abs == D_INF)
            return b_abs ? from_bits(D_INF | sign) : from_bits(D_QNAN);
        if (b_abs == D_INF)
            return a_abs ? from_bits(D_INF | sign) : from_bits(D_QNAN);

        if (a_abs == 0 || b_abs == 0)
            return from_bits(sign);

        if (a_abs < D_IMPLICIT)
            scale += normalize(&a_sig);
        if (b_abs < D_IMPLICIT)
            scale += normalize(&b_sig);
    }

    a_sig |= D_IMPLICIT;
    b_sig |= D_IMPLICIT;

    uint64_t product_hi;
    uint64_t product_lo;
    wide_multiply(a_sig, b_sig << 11, &product_hi, &product_lo);

    int product_exp = (int)a_exp + (int)b_exp - D_BIAS + scale;

    if (product_hi & D_IMPLICIT) {
        ++product_exp;
    } else {
        product_hi = (product_hi << 1) | (product_lo >> 63);
        product_lo <<= 1;
    }

    if (product_exp >= D_MAX_EXP)
        return from_bits(D_INF | sign);

    if (product_exp <= 0) {
        const unsigned shift = 1u - (unsigned)product_exp;
        if (shift >= 64)
            return from_bits(sign);

        const bool sticky = (product_lo << (64 - shift)) != 0;
        product_lo =
            (product_hi << (64 - shift)) | (product_lo >> shift) | sticky;
        product_hi >>= shift;
    } else {
        product_hi &= D_SIG_MASK;
        product_hi |= (uint64_t)product_exp << 52;
    }

    product_hi |= sign;

    if (product_lo > D_SIGN)
        ++product_hi;
    else if (product_lo == D_SIGN)
        product_hi += product_hi & 1u;

    return from_bits(product_hi);
}

double hpclang_soft_dadd(double a, double b)
{
    uint64_t a_rep = to_bits(a);
    uint64_t b_rep = to_bits(b);
    uint64_t a_abs = a_rep & D_ABS;
    uint64_t b_abs = b_rep & D_ABS;

    if (a_abs - 1u >= D_INF - 1u ||
        b_abs - 1u >= D_INF - 1u) {
        if (a_abs > D_INF)
            return from_bits(a_rep | D_QUIET);
        if (b_abs > D_INF)
            return from_bits(b_rep | D_QUIET);

        if (a_abs == D_INF) {
            if ((a_rep ^ b_rep) == D_SIGN)
                return from_bits(D_QNAN);
            return a;
        }
        if (b_abs == D_INF)
            return b;

        if (a_abs == 0) {
            if (b_abs == 0)
                return from_bits(a_rep & b_rep);
            return b;
        }
        if (b_abs == 0)
            return a;
    }

    if (b_abs > a_abs) {
        uint64_t tmp = a_rep;
        a_rep = b_rep;
        b_rep = tmp;
        tmp = a_abs;
        a_abs = b_abs;
        b_abs = tmp;
    }

    int a_exp = (int)((a_rep >> 52) & D_MAX_EXP);
    int b_exp = (int)((b_rep >> 52) & D_MAX_EXP);
    uint64_t a_sig = a_rep & D_SIG_MASK;
    uint64_t b_sig = b_rep & D_SIG_MASK;

    if (a_exp == 0)
        a_exp = normalize(&a_sig);
    if (b_exp == 0)
        b_exp = normalize(&b_sig);

    const uint64_t result_sign = a_rep & D_SIGN;
    const bool subtraction = ((a_rep ^ b_rep) & D_SIGN) != 0;

    a_sig = (a_sig | D_IMPLICIT) << 3;
    b_sig = (b_sig | D_IMPLICIT) << 3;

    const unsigned align = (unsigned)(a_exp - b_exp);
    if (align != 0) {
        if (align < 64) {
            const bool sticky = (b_sig << (64 - align)) != 0;
            b_sig = (b_sig >> align) | sticky;
        } else {
            b_sig = 1;
        }
    }

    if (subtraction) {
        a_sig -= b_sig;
        if (a_sig == 0)
            return from_bits(0);

        if (a_sig < (D_IMPLICIT << 3)) {
            const int shift =
                clz64_nonzero(a_sig) - clz64_nonzero(D_IMPLICIT << 3);
            a_sig <<= shift;
            a_exp -= shift;
        }
    } else {
        a_sig += b_sig;
        if (a_sig & (D_IMPLICIT << 4)) {
            const bool sticky = (a_sig & 1u) != 0;
            a_sig = (a_sig >> 1) | sticky;
            ++a_exp;
        }
    }

    if (a_exp >= D_MAX_EXP)
        return from_bits(D_INF | result_sign);

    if (a_exp <= 0) {
        const unsigned shift = (unsigned)(1 - a_exp);
        if (shift >= 64) {
            a_sig = a_sig != 0;
        } else {
            const bool sticky = (a_sig << (64 - shift)) != 0;
            a_sig = (a_sig >> shift) | sticky;
        }
        a_exp = 0;
    }

    const unsigned rgs = (unsigned)(a_sig & 7u);
    uint64_t result = (a_sig >> 3) & D_SIG_MASK;
    result |= (uint64_t)a_exp << 52;
    result |= result_sign;

    if (rgs > 4u || (rgs == 4u && (result & 1u)))
        ++result;

    return from_bits(result);
}

double hpclang_soft_dsub(double a, double b)
{
    return hpclang_soft_dadd(a, from_bits(to_bits(b) ^ D_SIGN));
}

static int ordered_compare(double a, double b, bool *unordered)
{
    const int64_t a_int = (int64_t)to_bits(a);
    const int64_t b_int = (int64_t)to_bits(b);
    const uint64_t a_abs = (uint64_t)a_int & D_ABS;
    const uint64_t b_abs = (uint64_t)b_int & D_ABS;

    if (a_abs > D_INF || b_abs > D_INF) {
        *unordered = true;
        return 0;
    }

    *unordered = false;

    if ((a_abs | b_abs) == 0)
        return 0;

    if ((a_int & b_int) >= 0) {
        if (a_int < b_int)
            return -1;
        if (a_int > b_int)
            return 1;
        return 0;
    }

    if (a_int > b_int)
        return -1;
    if (a_int < b_int)
        return 1;
    return 0;
}

int hpclang_soft_dcmpeq(double a, double b)
{
    bool unordered;
    return !ordered_compare(a, b, &unordered) && !unordered;
}

int hpclang_soft_dcmplt(double a, double b)
{
    bool unordered;
    return ordered_compare(a, b, &unordered) < 0 && !unordered;
}

int hpclang_soft_dcmple(double a, double b)
{
    bool unordered;
    return ordered_compare(a, b, &unordered) <= 0 && !unordered;
}

int hpclang_soft_dcmpge(double a, double b)
{
    bool unordered;
    return ordered_compare(a, b, &unordered) >= 0 && !unordered;
}

int hpclang_soft_dcmpgt(double a, double b)
{
    bool unordered;
    return ordered_compare(a, b, &unordered) > 0 && !unordered;
}

int hpclang_soft_dcmpun(double a, double b)
{
    bool unordered;
    (void)ordered_compare(a, b, &unordered);
    return unordered;
}
