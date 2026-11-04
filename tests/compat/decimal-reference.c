#define HPCLANG_REFERENCE_SUPPORT
#include "decimal-reference.h"
#include <assert.h>

#ifdef HPCLANG_DECIMAL_CANDIDATE
#include "hpgcc-format.h"
#else
static int digits = 6;
static char separator[2] = ".";
int get_decimal_digits(void)
{
    return digits;
}
void set_decimal_digits(int value)
{
    digits = value;
}
char *get_decimal_separator(void)
{
    return separator;
}
void set_decimal_separator(char value)
{
    separator[0] = value;
}
double ipow(double value, int exponent)
{
    return pow(value, exponent);
}
double i10powx(int exponent)
{
    return pow(10.0, exponent);
}

char *ulltoa(ULONGLONG value, char *buffer, int base)
{
    char reversed[65];
    size_t size = 0;
    assert(base >= 2 && base <= 16);
    do {
        reversed[size++] = "0123456789abcdef"[value % (unsigned)base];
        value /= (unsigned)base;
    } while (value);
    for (size_t index = 0; index < size; ++index)
        buffer[index] = reversed[size - index - 1];
    buffer[size] = 0;
    return buffer;
}
char *lltoa(LONGLONG value, char *buffer, int base)
{
    if (value < 0) {
        buffer[0] = '-';
        ulltoa(0ULL - (ULONGLONG)value, buffer + 1, base);
    } else
        ulltoa((ULONGLONG)value, buffer, base);
    return buffer;
}
char *itoa(int value, char *buffer, int base)
{
    return lltoa(value, buffer, base);
}
char *utoa(unsigned value, char *buffer, int base)
{
    return ulltoa(value, buffer, base);
}
char *upper(char *buffer, char *source)
{
    size_t index = 0;
    do
        buffer[index] = (char)toupper((unsigned char)source[index]);
    while (source[index++]);
    return buffer;
}
char *xpad(char *buffer, char *source, int width, int left, char fill)
{
    size_t size = strlen(source);
    if ((size_t)width <= size)
        return strcpy(buffer, source);
    memset(buffer, fill, (size_t)width);
    memcpy(buffer + (left ? (size_t)width - size : 0), source, size);
    buffer[width] = 0;
    return buffer;
}
#endif
int legacy_sprintf(char *buffer, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
#ifdef HPCLANG_DECIMAL_CANDIDATE
    int result = hpclang_hpgcc_vsnprintf(buffer, 512, format, arguments);
#else
    int result = legacy_vsprintf(buffer, format, arguments);
#endif
    va_end(arguments);
    return result;
}

static void probe(const char *name, int places, char point, const char *format, double value)
{
    char output[512];
    set_decimal_digits(places);
    set_decimal_separator(point);
    int result = legacy_sprintf(output, format, value);
    /* Record the historical return-count defect; do not endorse it as an API. */
    printf("%s|%s|digits=%d|separator=%s|return=%d\n", name, output, get_decimal_digits(),
           get_decimal_separator(), result);
}
int main(void)
{
    probe("default", 6, '.', "%f", 1.25);
    probe("setting", 2, '.', "%f", 1.25);
    probe("separator", 3, ',', "%f", -1.25);
    probe("precision-restored", 2, ',', "%.4f", 1.25);
    probe("zero-precision", 4, ',', "%.0f", 1.25);
    probe("general-fractional", 2, '.', "%g", 12.5);
    probe("general-fixed", 2, '.', "%g", 125.0);
    probe("general-exponent", 2, ',', "%g", 12500.0);
    probe("exponent", 2, '.', "%e", 125.0);
    probe("upper-exponent", 2, '.', "%E", 125.0);
    probe("padded", 2, ',', "%010f", -1.25);
    probe("left", 2, ',', "%-10f", -1.25);
    probe("zero", 2, ',', "%f", 0.0);
    probe("negative-zero", 2, ',', "%f", -0.0);
    probe("precision-cap", 6, '.', "%.17f", 1.0);
    probe("half-away", 2, '.', "%f", 0.125);
    probe("negative-half-away", 2, '.', "%f", -0.125);
    probe("fraction-carry", 2, '.', "%f", 0.995);
    probe("negative-fraction-zero", 2, '.', "%f", -0.004);
    char mixed[512];
    set_decimal_separator(',');
    int result = legacy_sprintf(mixed, "%5s/%-5s/%010f/%c/%%/%b/%L/%U", "hi", "hi", -1.25, 'A', 5u,
                                -123LL, 456ULL);
    printf("mixed|%s|digits=%d|separator=%s|return=%d\n", mixed, get_decimal_digits(),
           get_decimal_separator(), result);
#ifdef HPCLANG_DECIMAL_SWEEP

    const double values[] = {0.005,  0.015,  0.025, 0.045, 0.125, 0.375,           0.625, 0.875,
                             1.005,  1.015,  1.025, 1.125, 1.375, 1.625,           1.875, 2.675,
                             12.345, 99.995, 1e-6,  1e-12, 1e-16, 999.999999999999};
    const int precisions[] = {0, 1, 2, 6, 12};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        for (int neighbor = -1; neighbor <= 1; ++neighbor)
            for (int sign = -1; sign <= 1; sign += 2)
                for (unsigned p = 0; p < sizeof(precisions) / sizeof(precisions[0]); ++p)
                    for (unsigned c = 0; c < 4; ++c)
                        for (unsigned explicit_precision = 0; explicit_precision < 2;
                             ++explicit_precision) {
                            double value =
                                sign * (neighbor == 0
                                            ? values[i]
                                            : nextafter(values[i], neighbor < 0 ? 0.0 : INFINITY));
                            char label[128], format[16];
                            snprintf(label, sizeof(label), "sweep_%a_p%d_%c_m%u", value,
                                     precisions[p], "feEg"[c], explicit_precision);
                            if (explicit_precision)
                                snprintf(format, sizeof(format), "%%.%d%c", precisions[p],
                                         "feEg"[c]);
                            else
                                snprintf(format, sizeof(format), "%%%c", "feEg"[c]);
                            probe(label, explicit_precision ? 3 : precisions[p], '.', format,
                                  value);
                        }
#endif
    return 0;
}
