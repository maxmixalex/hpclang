#include <hpstdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

static const char *input;
static int input_error;
char *hpclang_hpgcc_gets(char *buffer, size_t capacity)
{
    assert(capacity == 1024);
    if (!input) {
        errno = input_error;
        return NULL;
    }
    assert(strlen(input) < capacity);
    return strcpy(buffer, input);
}
static int scan_args(const char *text, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = _vsscanf(text, format, args);
    va_end(args);
    return result;
}
int main(void)
{
    int y, m, d, h, minute, second;
    /* Conversion used by the original HPGCC 2.0 examples/scanf.c. */
    assert(_sscanf("2026-11-12 13:45:09", "%d-%d-%d %d:%d:%d", &y, &m, &d, &h, &minute, &second) ==
           6);
    assert(y == 2026 && m == 11 && d == 12 && h == 13 && minute == 45 && second == 9);
    char word[4] = {0};
    unsigned hex;
    int consumed = -1;
    assert(scan_args("skip abc 0x2a!", "%*s %3[a-z] %x%n", word, &hex, &consumed) == 2);
    assert(!strcmp(word, "abc") && hex == 42 && consumed == 13);
    int a = -1, b = -2;
    assert(_sscanf("12 no", "%d %d", &a, &b) == 1 && a == 12 && b == -2);
    assert(_sscanf("", "%d", &a) == EOF);
    input = "-17 42";
    assert(_scanf("%d %d", &a, &b) == 2 && a == -17 && b == 42);
    input = "";
    assert(_scanf("%d", &a) == EOF && a == -17);
    input = NULL;
    input_error = ECANCELED;
    assert(_scanf("%d", &a) == EOF && errno == ECANCELED && a == -17);
    input_error = EIO;
    assert(_scanf("%d", &a) == EOF && errno == EIO && a == -17);
    input_error = 0;
    assert(_scanf("%d", &a) == EOF && !errno && a == -17);
    /* hpstdio.h must not replace standard stream/string scanning. */
    assert(sscanf("8", "%d", &a) == 1 && a == 8);
    return 0;
}
