#include "hpgcc-format.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int decimal_digits = 6;
static char decimal_separator[2] = ".";

int get_decimal_digits(void)
{
    return decimal_digits;
}
void set_decimal_digits(int value)
{
    if (value < 0) {
        errno = EINVAL;
        return;
    }
    decimal_digits = value > 15 ? 15 : value;
}
char *get_decimal_separator(void)
{
    return decimal_separator;
}
void set_decimal_separator(char value)
{
    if (!value) {
        errno = EINVAL;
        return;
    }
    decimal_separator[0] = value;
    decimal_separator[1] = 0;
}

/* Avoid manufacturing an overflowing power for subnormal values. */
static double scale10(double value, int exponent)
{
    while (exponent > 100) {
        value *= 1e100;
        exponent -= 100;
    }
    while (exponent < -100) {
        value /= 1e100;
        exponent += 100;
    }
    return exponent < 0 ? value / pow(10.0, -exponent) : value * pow(10.0, exponent);
}

static int legacy_exponent(double value)
{
    int binary_exponent;
    (void)frexp(value, &binary_exponent);
    /* Historical truncation toward zero, deliberately not floor(log10(x)). */
    return (int)(binary_exponent * 0.3010299956639812);
}

static double legacy_round(double value, int precision)
{
    int exponent = legacy_exponent(value);
    double magnitude = fabs(value);
    double factor = pow(10.0, precision);
    if (exponent < 0)
        magnitude = scale10(magnitude, -exponent);
    /* The rounded quotient can be slightly above the exact overflow boundary.
     * Equality must skip multiplication; values this large have no fraction. */
    if (magnitude < DBL_MAX / factor)
        magnitude = round(magnitude * factor) / factor;
    if (exponent < 0)
        magnitude = scale10(magnitude, exponent);
    return value < 0.0 ? -magnitude : magnitude;
}

int hpclang_hpgcc_format_double(char *buffer, size_t capacity, double value, char conversion,
                                int precision)
{
    if ((!buffer && capacity) || precision < -1 ||
        (conversion != 'f' && conversion != 'e' && conversion != 'E' && conversion != 'g')) {
        if (buffer && capacity)
            buffer[0] = 0;
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    int places = precision < 0 ? decimal_digits : precision > 15 ? 15 : precision;
    char point = decimal_separator[0];
    /* Largest finite binary64 fixed result: sign + 309 digits + dot + 15
     * digits. Exponential suffixes are smaller. Keep scratch bounded. */
    char text[352];
    int length;
    if (!isfinite(value)) {
        length = snprintf(text, sizeof(text), "%s",
                          isnan(value) ? "nan"
                          : value < 0  ? "-inf"
                                       : "inf");
    } else {
        value = legacy_round(value, places);
        int exponent = value == 0.0 ? 0 : legacy_exponent(value);
        double magnitude = fabs(value);
        int scientific =
            value != 0.0 && (conversion == 'e' || conversion == 'E' ||
                             (conversion == 'g' && (magnitude >= 1e15 || magnitude <= 1e-15 ||
                                                    exponent > places || exponent < -places)));
        if (scientific) {
            /* Preserve the original multiply order in the finite-power range.
             * Division by the reciprocal changes halfway rounding (12.345).
             * Extreme subnormals still require overflow-safe staged scaling. */
            int shift = -exponent;
            value =
                shift >= -308 && shift <= 308 ? value * pow(10.0, shift) : scale10(value, shift);
        }
        /* The historical comparison discarded the sign of zero. */
        if (value == 0.0)
            value = 0.0;
        /* dtoa performed a second half-away rounding of fractional digits.
         * Standard printf alone would round exact ties to even. Propagate a
         * carry into the integer part before writing the bounded result. */
        double factor = pow(10.0, places);
        if (fabs(value) < DBL_MAX / factor)
            value = round(value * factor) / factor;
        length = snprintf(text, sizeof(text), "%.*f", places, value);
        if (length >= 0 && (size_t)length < sizeof(text)) {
            char *decimal = strchr(text, '.');
            if (decimal)
                *decimal = point;
            if (scientific) {
                int count = snprintf(text + length, sizeof(text) - (size_t)length, "%c%c%03u",
                                     conversion == 'e' ? 'e' : 'E', exponent < 0 ? '-' : '+',
                                     (unsigned)(exponent < 0 ? -exponent : exponent));
                if (count < 0)
                    length = -1;
                else
                    length += count;
            }
        }
    }
    if (length < 0 || (size_t)length >= sizeof(text)) {
        if (buffer && capacity)
            buffer[0] = 0;
        errno = EOVERFLOW;
        return -1;
    }
    if (capacity) {
        size_t copied = (size_t)length < capacity - 1 ? (size_t)length : capacity - 1;
        memcpy(buffer, text, copied);
        buffer[copied] = 0;
    }
    errno = saved_errno;
    return length;
}
