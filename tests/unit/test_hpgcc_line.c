#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/stdio.h>
#include "hpgcc-line.h"

static const unsigned char *input;
static char output[64];
static unsigned offset, out, erases, fulls;
static int end_result, fail_echo, fail_erase, fail_full;
static int read_byte(void *context)
{
    assert(context == output);
    if (input[offset])
        return input[offset++];
    if (end_result == HPCLANG_STDIO_ERROR)
        errno = ECANCELED;
    return end_result;
}
static int echo(void *context, unsigned char byte)
{
    assert(context == output);
    if (fail_echo)
        return -1;
    assert(out < sizeof output - 1);
    output[out++] = (char)byte;
    output[out] = '\0';
    return 0;
}
static int erase(void *context)
{
    assert(context == output);
    ++erases;
    if (fail_erase) {
        errno = ENODEV;
        return -1;
    }
    return 0;
}
static int full(void *context)
{
    assert(context == output);
    ++fulls;
    return fail_full ? -1 : 0;
}
static void start(const char *text)
{
    input = (const unsigned char *)text;
    offset = out = erases = fulls = 0;
    output[0] = '\0';
    end_result = HPCLANG_STDIO_EOF;
    fail_echo = fail_erase = fail_full = 0;
    errno = EDOM;
}
int main(void)
{
    const struct hpclang_hpgcc_line_io io = {read_byte, echo, erase, full, output};
    struct
    {
        char before;
        char text[3];
        char after;
    } guarded = {'x', "OK", 'y'};
    char *text = guarded.text;
    start("\bABX\bC\n");
    assert(hpclang_hpgcc_read_line(text, 3, &io) == text && errno == EDOM);
    assert(strcmp(text, "AC") == 0 && strcmp(output, "ABC\n") == 0 && erases == 1 && fulls == 1);
    assert(guarded.before == 'x' && guarded.after == 'y');
    start("A\r");
    assert(hpclang_hpgcc_read_line(text, 1, &io) == text && !*text && fulls == 1);
    start("\x87\xff\n");
    assert(hpclang_hpgcc_read_line(text, 3, &io) == text);
    assert((unsigned char)text[0] == 0x87 && (unsigned char)text[1] == 0xff);
    start("AB");
    end_result = HPCLANG_STDIO_ERROR;
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && errno == ECANCELED && !strcmp(text, "AB"));
    start("A");
    assert(hpclang_hpgcc_read_line(text, 3, &io) == text && errno == EDOM);
    start("");
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && !*text && errno == EDOM);
    start("A");
    fail_echo = 1;
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && errno == EIO && !*text);
    start("A\b");
    fail_erase = 1;
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && errno == ENODEV && !strcmp(text, "A"));
    start("ABC");
    fail_full = 1;
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && errno == EIO && !strcmp(text, "AB"));
    start("");
    end_result = 256;
    assert(!hpclang_hpgcc_read_line(text, 3, &io) && errno == EIO);
    strcpy(text, "OK");
    assert(!hpclang_hpgcc_read_line(text, 0, &io) && errno == EINVAL && !strcmp(text, "OK"));
    assert(!hpclang_hpgcc_read_line(text, 3, NULL) && !strcmp(text, "OK"));
    assert(!hpclang_hpgcc_read_line(NULL, 3, &io));
    return 0;
}
