#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpstdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int readint(void)
{
    char line[32];
    if (!hpclang_hpgcc_gets(line, sizeof line))
        return 0;
    char *end;
    int saved = errno;
    errno = 0;
    long value = strtol(line, &end, 10);
    if (end == line) {
        errno = EINVAL;
        return 0;
    }
    if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
        errno = ERANGE;
        return value < 0 ? INT_MIN : INT_MAX;
    }
    errno = saved;
    return (int)value;
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
