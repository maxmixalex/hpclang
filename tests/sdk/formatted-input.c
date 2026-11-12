#include <hpstdio.h>
#include <hpclang/stdio.h>
#include <string.h>

static const unsigned char *input;
static int read_byte(void *context)
{
    (void)context;
    return *input ? *input++ : HPCLANG_STDIO_EOF;
}

static int scan_args(const char *text, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = _vsscanf(text, format, args);
    va_end(args);
    return count;
}

int main(void)
{
    int year, month, day, hour, minute, second;
    /* Format used by the original HPGCC 2.0 scanf example. */
    if (_sscanf("2026-11-12 13:45:09", "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute,
                &second) != 6 ||
        year != 2026 || month != 11 || day != 12 || hour != 13 || minute != 45 || second != 9)
        return 1;
    char word[4] = {0};
    unsigned value = 0;
    int consumed = -1;
    if (scan_args("skip abc 0x2a!", "%*s %3[a-z] %x%n", word, &value, &consumed) != 2 ||
        strcmp(word, "abc") || value != 42 || consumed != 13)
        return 2;
    struct hpclang_stdio_callbacks callbacks = {0};
    callbacks.read = read_byte;
    input = (const unsigned char *)"-17 42\n12 no\n";
    if (hpclang_stdio_bind(&callbacks))
        return 3;
    int a = 0, b = 0;
    int valid = _scanf("%d %d", &a, &b) == 2 && a == -17 && b == 42;
    valid = valid && _scanf("%d %d", &a, &b) == 1 && a == 12 && b == 42;
    valid = valid && _scanf("%d", &a) == EOF && a == 12;
    int detached = hpclang_stdio_unbind();
    /* Including the compatibility header keeps the standard parser available. */
    valid = valid && sscanf("8", "%d", &a) == 1 && a == 8;
    return valid && !detached ? 0 : 4;
}
