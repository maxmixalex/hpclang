#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpstdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static unsigned long long read_integer(unsigned base, unsigned long long maximum, int *negative)
{
    char line[32];
    if (negative)
        *negative = 0;
    if (!hpclang_hpgcc_gets(line, sizeof line))
        return 0;
    const unsigned char *cursor = (const unsigned char *)line;
    if (negative && *cursor == '-') {
        *negative = 1;
        ++maximum;
        ++cursor;
    }
    unsigned long long value = 0;
    unsigned digits = 0;
    int overflow = 0;
    for (; *cursor; ++cursor) {
        unsigned character = *cursor;
        unsigned digit = character >= '0' && character <= '9'   ? character - '0'
                         : character >= 'a' && character <= 'f' ? character - 'a' + 10
                         : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                                                : base;
        if (digit >= base)
            break;
        ++digits;
        if (value > (maximum - digit) / base) {
            overflow = 1;
            value = maximum;
        } else if (!overflow)
            value = value * base + digit;
    }
    if (!digits) {
        errno = EINVAL;
        return 0;
    }
    if (overflow)
        errno = ERANGE;
    return value;
}

int readint(void)
{
    int negative;
    unsigned long long value = read_integer(10, INT_MAX, &negative);
    return negative ? (value ? -(int)(value - 1) - 1 : 0) : (int)value;
}

unsigned readuint(void)
{
    return (unsigned)read_integer(10, UINT_MAX, NULL);
}

unsigned readhex(void)
{
    return (unsigned)read_integer(16, UINT_MAX, NULL);
}

long long readlonglong(void)
{
    int negative;
    unsigned long long value = read_integer(10, LLONG_MAX, &negative);
    return negative ? (value ? -(long long)(value - 1) - 1 : 0) : (long long)value;
}

unsigned long long readulonglong(void)
{
    return read_integer(10, ULLONG_MAX, NULL);
}

double readdouble(void)
{
    char line[32];
    if (!hpclang_hpgcc_gets(line, sizeof line))
        return 0;
    char *end;
    int saved = errno;
    errno = 0;
    double value = strtod(line, &end);
    if (end == line) {
        errno = EINVAL;
        return 0;
    }
    if (!errno)
        errno = saved;
    return value;
}
