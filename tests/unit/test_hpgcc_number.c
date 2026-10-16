#include <hpstdio.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <string.h>

static const char *input;
static int input_error;

char *hpclang_hpgcc_gets(char *buffer, size_t capacity)
{
    assert(capacity == 32);
    if (input_error) {
        errno = input_error;
        return NULL;
    }
    if (!input)
        return NULL;
    assert(strlen(input) < capacity);
    strcpy(buffer, input);
    return buffer;
}

int main(void)
{
    input = " \t-123tail";
    errno = EDOM;
    assert(readint() == -123 && errno == EDOM);
    input = "0x10";
    assert(readint() == 0 && errno == EDOM);
    char text[32];
    snprintf(text, sizeof text, "%d", INT_MIN);
    input = text;
    assert(readint() == INT_MIN && errno == EDOM);
    snprintf(text, sizeof text, "%d", INT_MAX);
    assert(readint() == INT_MAX && errno == EDOM);
    input = "9999999999999999999999999999999";
    assert(readint() == INT_MAX && errno == ERANGE);
    input = "-999999999999999999999999999999";
    assert(readint() == INT_MIN && errno == ERANGE);
    /* Exercise the narrower int boundary even on hosts with 64-bit long. */
    snprintf(text, sizeof text, "%llu", (unsigned long long)INT_MAX + 1);
    input = text;
    assert(readint() == INT_MAX && errno == ERANGE);
    input = " \t-1.25e2tail";
    errno = EDOM;
    assert(readdouble() == -125.0 && errno == EDOM);
    input = "-0";
    assert(signbit(readdouble()) && errno == EDOM);
    input = "1e9999";
    assert(isinf(readdouble()) && errno == ERANGE);
    input = "1e-9999";
    assert(readdouble() == 0 && errno == ERANGE);
    const char *invalid[] = {"", " ", "+", "word"};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        input = invalid[i];
        errno = 0;
        assert(readint() == 0 && errno == EINVAL);
        errno = 0;
        assert(readdouble() == 0 && errno == EINVAL);
    }
    input = NULL;
    errno = EDOM;
    assert(readint() == 0 && errno == EDOM);
    assert(readdouble() == 0 && errno == EDOM);
    input = "123";
    const int errors[] = {ECANCELED, EIO, ENOMEM};
    for (unsigned i = 0; i < sizeof errors / sizeof errors[0]; ++i) {
        input_error = errors[i];
        assert(readint() == 0 && errno == input_error);
        assert(readdouble() == 0 && errno == input_error);
    }
    return 0;
}
