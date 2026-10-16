#include <hpstring.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int main(void)
{
    const unsigned values[] = {0, 1, 9, 10, 35, 36, 255, (unsigned)INT_MAX, UINT_MAX};
    for (int base = 2; base <= 36; ++base) {
        for (unsigned i = 0; i < sizeof values / sizeof values[0]; ++i) {
            errno = EDOM;
            const char *text = uints(values[i], base);
            assert(errno == EDOM && *text);
            char *end;
            assert(strtoul(text, &end, base) == values[i] && !*end);
            assert(text[0] != '0' || text[1] == '\0');
            for (const char *p = text; *p; ++p)
                assert((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z'));
        }
        const int signed_values[] = {INT_MIN, -1, 0, 1, INT_MAX};
        for (unsigned i = 0; i < sizeof signed_values / sizeof signed_values[0]; ++i) {
            char *end;
            errno = EDOM;
            const char *text = ints(signed_values[i], base);
            assert(errno == EDOM);
            assert(strtol(text, &end, base) == signed_values[i] && !*end);
        }
    }
    assert(strcmp(uints(255, 16), "ff") == 0);
    assert(strcmp(ints(-255, 16), "-ff") == 0);
    const int invalid[] = {INT_MIN, -1, 0, 1, 37, INT_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        errno = EDOM;
        assert(strcmp(uints(UINT_MAX, invalid[i]), "0") == 0);
        assert(strcmp(ints(INT_MIN, invalid[i]), "-0") == 0);
        assert(errno == EDOM);
    }
    char *negative = ints(-123, 10);
    char *positive = ints(456, 10);
    assert(strcmp(negative, "-123") == 0 && strcmp(positive, "456") == 0);
    assert(uints(9, 10) == positive && strcmp(positive, "9") == 0);
    assert(strcmp(negative, "-123") == 0);
    assert(ints(-7, 10) == negative && strcmp(negative, "-7") == 0);
    return 0;
}
