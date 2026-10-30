#include "hpgcc-format.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void check(double value, char conversion, int precision, const char *expected)
{
    char output[352];
    errno = EDOM;
    int size = hpclang_hpgcc_format_double(output, sizeof(output), value, conversion, precision);
    assert(size == (int)strlen(expected) && !strcmp(output, expected) && errno == EDOM);
    assert(hpclang_hpgcc_format_double(NULL, 0, value, conversion, precision) == size);
    for (size_t capacity = 1; capacity <= (size_t)size + 1; ++capacity) {
        memset(output, 0x5a, sizeof(output));
        assert(hpclang_hpgcc_format_double(output, capacity, value, conversion, precision) == size);
        size_t copied = capacity - 1 < (size_t)size ? capacity - 1 : (size_t)size;
        assert(!memcmp(output, expected, copied) && output[copied] == 0);
        assert(output[capacity] == 0x5a);
    }
}

int main(void)
{
    assert(get_decimal_digits() == 6 && !strcmp(get_decimal_separator(), "."));
    check(1.25, 'f', -1, "1.250000");
    set_decimal_digits(2);
    check(1.25, 'f', -1, "1.25");
    set_decimal_digits(3);
    set_decimal_separator(',');
    check(-1.25, 'f', -1, "-1,250");
    set_decimal_digits(2);
    check(1.25, 'f', 4, "1,2500");
    assert(get_decimal_digits() == 2);
    check(1.25, 'f', 0, "1");
    set_decimal_separator('.');
    check(12.5, 'g', -1, "12.50");
    check(125.0, 'g', -1, "125.00");
    set_decimal_separator(',');
    check(12500, 'g', -1, "1,25E+004");
    set_decimal_separator('.');
    check(125, 'e', -1, "1.25e+002");
    check(125, 'E', -1, "1.25E+002");
    set_decimal_separator(',');
    check(0.0, 'f', -1, "0,00");
    check(-0.0, 'f', -1, "0,00");
    set_decimal_separator('.');
    check(1.0, 'f', 17, "1.000000000000000");
    check(0.125, 'f', 2, "0.13");
    check(-0.125, 'f', 2, "-0.13");
    check(0.995, 'f', 2, "1.00");
    check(-0.004, 'f', 2, "-0.00");
    check(0x1.8b0a3d70a3d7p+3, 'e', 2, "1.24e+001");
    check(-0x1.8b0a3d70a3d7p+3, 'E', 2, "-1.24E+001");
    /* Correct the old overlong fractional carry and zero-precision suffix. */
    check(0x1.8ffae147ae147p+6, 'e', 2, "1.00e+002");
    check(999.999999999999, 'e', 12, "1.000000000000e+003");
    check(0.005, 'e', 0, "0e-001");
    /* Rounded DBL_MAX / 10^6 is just above the exact multiply-safe boundary. */
    double boundary = 0x1.0c6f7a0b5ed8dp+1004;
    check(boundary, 'e', 6, "1.797693e+302");
    check(boundary, 'g', 6, "1.797693E+302");
    check(-boundary, 'E', 6, "-1.797693E+302");
    char fixed[352];
    assert(snprintf(fixed, sizeof(fixed), "%.6f", boundary) == 310);
    check(boundary, 'f', 6, fixed);
    check(INFINITY, 'g', -1, "inf");
    check(-INFINITY, 'e', -1, "-inf");
    check(NAN, 'f', -1, "nan");
    set_decimal_digits(INT_MAX);
    assert(get_decimal_digits() == 15);
    set_decimal_digits(-1);
    assert(errno == EINVAL && get_decimal_digits() == 15);
    set_decimal_separator(0);
    assert(errno == EINVAL && !strcmp(get_decimal_separator(), "."));
    char output[352];
    assert(hpclang_hpgcc_format_double(output, sizeof(output), DBL_MAX, 'f', 15) == 325);
    assert(strncmp(output, "17976931348623157", 17) == 0);
    assert(hpclang_hpgcc_format_double(output, sizeof(output), DBL_TRUE_MIN, 'e', 15) > 0);
    assert(strstr(output, "e-323") && !strstr(output, "inf") && !strstr(output, "nan"));
    assert(hpclang_hpgcc_format_double(output, sizeof(output), 1, 'q', 2) == -1);
    assert(errno == EINVAL && output[0] == 0);
    assert(hpclang_hpgcc_format_double(NULL, 1, 1, 'f', 2) == -1 && errno == EINVAL);
    assert(hpclang_hpgcc_format_double(output, sizeof(output), 1, 'f', -2) == -1);
    /* A setting change must never affect the standard formatter. */
    set_decimal_digits(1);
    set_decimal_separator(',');
    assert(snprintf(output, sizeof(output), "%f", 1.25) == 8);
    assert(!strcmp(output, "1.250000"));
    return 0;
}
