#include "hpgcc-format.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct output
{
    char *buffer;
    size_t capacity;
    int length;
    FILE *stream;
};

static int emit(struct output *out, const char *text, size_t length, char fill)
{
    if (length > (size_t)(INT_MAX - out->length)) {
        errno = EOVERFLOW;
        return -1;
    }
    size_t offset = (size_t)out->length;
    if (out->stream) {
        /* Target picolibc fwrite reports callback failure without setting the
         * FILE error flag. fputc preserves both the return and stream state. */
        for (size_t index = 0; index < length; ++index) {
            errno = 0;
            unsigned char byte = (unsigned char)(text ? text[index] : fill);
            if (fputc(byte, out->stream) == EOF) {
                if (!errno)
                    errno = EIO;
                return -1;
            }
        }
    }
    if (out->capacity && offset < out->capacity - 1) {
        size_t copied = out->capacity - 1 - offset;
        if (copied > length)
            copied = length;
        if (text)
            memcpy(out->buffer + offset, text, copied);
        else
            memset(out->buffer + offset, fill, copied);
        out->buffer[offset + copied] = 0;
    }
    out->length += (int)length;
    return 0;
}

static int decimal(const char **cursor, int *value)
{
    *value = 0;
    while (**cursor >= '0' && **cursor <= '9') {
        int digit = **cursor - '0';
        if (*value > (INT_MAX - digit) / 10) {
            errno = EOVERFLOW;
            return -1;
        }
        *value = *value * 10 + digit;
        ++*cursor;
    }
    return 0;
}

static size_t integer(char *buffer, unsigned long long value, unsigned base, int uppercase,
                      int negative)
{
    char reversed[64];
    size_t length = 0, written = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        reversed[length++] = digits[value % base];
        value /= base;
    } while (value);
    if (negative)
        buffer[written++] = '-';
    while (length)
        buffer[written++] = reversed[--length];
    buffer[written] = 0;
    return written;
}

static int parse(struct output *out, const char *format, va_list arguments)
{
    const char *cursor = format;
    while (*cursor) {
        if (*cursor != '%') {
            const char *start = cursor;
            while (*cursor && *cursor != '%')
                ++cursor;
            if (emit(out, start, (size_t)(cursor - start), 0))
                return -1;
            continue;
        }
        ++cursor;
        int minus = *cursor == '-';
        if (minus)
            ++cursor;
        int zero = *cursor == '0';
        int width, precision = -1;
        if (decimal(&cursor, &width))
            return -1;
        if (*cursor == '.') {
            ++cursor;
            if (decimal(&cursor, &precision))
                return -1;
        }
        char conversion = *cursor;
        if (!conversion || !strchr("scdiuxXpbULfeEg%", conversion)) {
            errno = EINVAL;
            return -1;
        }
        ++cursor;
        char scratch[352];
        const char *text = scratch;
        size_t length;
        if (conversion == 's') {
            text = va_arg(arguments, char *);
            if (!text)
                text = "<NULL>";
            length = strlen(text);
        } else if (conversion == 'c' || conversion == '%') {
            scratch[0] = conversion == '%' ? '%' : (char)va_arg(arguments, int);
            length = 1;
            width = 0; /* Historical character conversions ignore padding. */
        } else if (strchr("feEg", conversion)) {
            int count = hpclang_hpgcc_format_double(
                scratch, sizeof(scratch), va_arg(arguments, double), conversion, precision);
            if (count < 0)
                return -1;
            length = (size_t)count;
        } else {
            unsigned long long value;
            unsigned base = conversion == 'b' ? 2u : strchr("xXp", conversion) ? 16u : 10u;
            int negative = 0;
            if (conversion == 'd' || conversion == 'i' || conversion == 'L') {
                long long number =
                    conversion == 'L' ? va_arg(arguments, long long) : va_arg(arguments, int);
                negative = number < 0;
                value = negative ? 0ULL - (unsigned long long)number : (unsigned long long)number;
            } else if (conversion == 'U')
                value = va_arg(arguments, unsigned long long);
            else if (conversion == 'p')
                value = (uintptr_t)va_arg(arguments, void *);
            else
                value = va_arg(arguments, unsigned);
            length = integer(scratch, value, base, conversion == 'X', negative);
        }
        size_t padding = (size_t)width > length ? (size_t)width - length : 0;
        /* Original string alignment is the reverse of numeric alignment.
         * Numeric zero padding precedes the entire signed representation. */
        int before = conversion == 's' ? minus : !minus;
        char fill = conversion == 's' || !zero ? ' ' : '0';
        if ((before && emit(out, NULL, padding, fill)) || emit(out, text, length, 0) ||
            (!before && emit(out, NULL, padding, fill)))
            return -1;
    }
    return out->length;
}

int hpclang_hpgcc_vsnprintf(char *buffer, size_t capacity, const char *format, va_list arguments)
{
    if ((!buffer && capacity) || !format) {
        if (buffer && capacity)
            buffer[0] = 0;
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    struct output out = {buffer, capacity, 0, NULL};
    if (capacity)
        buffer[0] = 0;
    va_list copy;
    va_copy(copy, arguments);
    int result = parse(&out, format, copy);
    va_end(copy);
    if (result >= 0)
        errno = saved_errno;
    return result;
}

int hpclang_hpgcc_snprintf(char *buffer, size_t capacity, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = hpclang_hpgcc_vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);
    return result;
}

int hpclang_hpgcc_vsprintf(char *buffer, const char *format, va_list arguments)
{
    return hpclang_hpgcc_vsnprintf(buffer, SIZE_MAX, format, arguments);
}

int hpclang_hpgcc_sprintf(char *buffer, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = hpclang_hpgcc_vsprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}

int hpclang_hpgcc_vfprintf(FILE *stream, const char *format, va_list arguments)
{
    if (!stream || !format) {
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    struct output out = {NULL, 0, 0, stream};
    va_list copy;
    va_copy(copy, arguments);
    int result = parse(&out, format, copy);
    va_end(copy);
    if (result >= 0)
        errno = saved_errno;
    return result;
}

int hpclang_hpgcc_fprintf(FILE *stream, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = hpclang_hpgcc_vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int hpclang_hpgcc_vprintf(const char *format, va_list arguments)
{
    return hpclang_hpgcc_vfprintf(stdout, format, arguments);
}

int hpclang_hpgcc_printf(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = hpclang_hpgcc_vprintf(format, arguments);
    va_end(arguments);
    return result;
}
