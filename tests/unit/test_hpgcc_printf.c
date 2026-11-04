#include "hpgcc-format.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

static void check(const char *expected, const char *format, ...)
{
    char buffer[256];
    va_list arguments;
    va_start(arguments, format);
    size_t length = strlen(expected);
    errno = EDOM;
    for (size_t capacity = 0; capacity <= length + 1; ++capacity) {
        memset(buffer, 0x5a, sizeof(buffer));
        assert(hpclang_hpgcc_vsnprintf(buffer, capacity, format, arguments) == (int)length);
        assert(errno == EDOM);
        if (capacity) {
            size_t copied = capacity - 1 < length ? capacity - 1 : length;
            assert(!memcmp(buffer, expected, copied) && buffer[copied] == 0);
        }
        assert(buffer[capacity] == 0x5a);
    }
    assert(hpclang_hpgcc_vsnprintf(NULL, 0, format, arguments) == (int)length);
    va_end(arguments);
}

int main(void)
{
    set_decimal_digits(2);
    set_decimal_separator(',');
    check("hello", "hello");
    check("", "");
    check("-12/34/567/ab/AB/101", "%d/%i/%u/%x/%X/%b", -12, 34, 567u, 171u, 171u, 5u);
    check("-9223372036854775808/18446744073709551615", "%L/%U", LLONG_MIN, ULLONG_MAX);
    check("-2147483648", "%d", INT_MIN);
    check("00000-1,25/-1,25     ", "%010f/%-10f", -1.25, -1.25);
    check("-1,2500000", "%-010f", -1.25);
    check("hi   /   hi/<NULL>", "%5s/%-5s/%s", "hi", "hi", (char *)0);
    check("hello/42", "%.2s/%.5d", "hello", 42);
    check("A%", "%8c%8%", 'A');
    check("1,2500/1,25/1,25E+004", "%.4f/%f/%g", 1.25, 1.25, 12500.0);
    assert(get_decimal_digits() == 2);
    check("1234", "%p", (void *)(uintptr_t)0x1234);
    check("000-12", "%06d", -12);
    char buffer[8];
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "a%cb", 0) == 3);
    assert(!memcmp(buffer, "a\0b\0", 4));
    assert(hpclang_hpgcc_snprintf(NULL, 0, "%2147483647d", 1) == INT_MAX);
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "%2147483647dX", 1) == -1);
    assert(errno == EOVERFLOW && buffer[7] == 0);
    const char *bad[] = {"%", "%q", "%ld", "%*d", "%n", "%+f", "%#x"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), bad[i]) == -1);
        assert(errno == EINVAL && buffer[0] == 0);
    }
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "ok%") == -1);
    assert(errno == EINVAL && !strcmp(buffer, "ok"));
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "%2147483648d", 1) == -1);
    assert(errno == EOVERFLOW && buffer[0] == 0);
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "%.2147483648f", 1.0) == -1);
    assert(errno == EOVERFLOW && buffer[0] == 0);
    assert(hpclang_hpgcc_snprintf(NULL, 1, "") == -1 && errno == EINVAL);
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), NULL) == -1 && errno == EINVAL);
    return 0;
}
