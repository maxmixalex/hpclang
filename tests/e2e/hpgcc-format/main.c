#include <hpstdio.h>
#include <hpclang/stdio.h>
#include <hpclang/display.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

volatile uint32_t hpclang_e2e_result;
int legacy_render(char *, size_t, const char *, ...);
int legacy_stream(FILE *, const char *, ...);
int legacy_stdout(const char *, ...);
unsigned modern_formats(void);
unsigned legacy_cpp_formats(void);

struct capture
{
    char bytes[128];
    size_t used;
    unsigned fail_stderr;
    unsigned streams;
};
static int write_byte(void *context, unsigned stream, unsigned char byte)
{
    struct capture *capture = context;
    capture->streams |= 1u << stream;
    if ((stream == 2 && capture->fail_stderr) || capture->used == sizeof(capture->bytes)) {
        errno = EIO;
        return -1;
    }
    capture->bytes[capture->used++] = (char)byte;
    return 0;
}

static unsigned verify(void)
{
    char text[352];
    set_decimal_digits(2);
    set_decimal_separator(',');
    if (snprintf(text, sizeof(text), "%f", 1.25) != 8 || strcmp(text, "1.250000"))
        return 1;
    if (modern_formats() || legacy_cpp_formats())
        return 2;
    if (legacy_render(text, sizeof(text), "%010f/%g/%.4f", -1.25, 12500.0, 1.25) != 27 ||
        strcmp(text, "00000-1,25/1,25E+004/1,2500"))
        return 3;
    if (get_decimal_digits() != 2)
        return 4;
    if (legacy_render(text, sizeof(text), "%L/%U/%b/%-5s", LLONG_MIN, ULLONG_MAX, 5u, "hi") != 51 ||
        strcmp(text, "-9223372036854775808/18446744073709551615/101/   hi"))
        return 5;
    if (legacy_render(text, 3, "%f", 1.25) != 4 || strcmp(text, "1,"))
        return 6;
    if (legacy_render(NULL, 0, "%2147483647d", 1) != INT_MAX)
        return 7;
    if (legacy_render(text, sizeof(text), "ok%q") != -1 || errno != EINVAL || strcmp(text, "ok"))
        return 8;
    if (legacy_render(text, sizeof(text), "%f", 0.125) != 4 || strcmp(text, "0,13"))
        return 9;
    if (legacy_render(text, sizeof(text), "%.15f", DBL_MAX) != 325)
        return 10;
    double boundary = 0x1.0c6f7a0b5ed8dp+1004;
    if (legacy_render(text, sizeof(text), "%.6e", boundary) != 13 || strcmp(text, "1,797693e+302"))
        return 19;
    if (legacy_render(text, sizeof(text), "%.6f", boundary) != 310 || strstr(text, "inf"))
        return 20;
    if (legacy_render(text, sizeof(text), "%.2e", 0x1.8b0a3d70a3d7p+3) != 9 ||
        strcmp(text, "1,24e+001"))
        return 21;
    if (legacy_render(text, sizeof(text), "%.2e", 0x1.8ffae147ae147p+6) != 9 ||
        strcmp(text, "1,00e+002"))
        return 22;
    if (legacy_render(text, sizeof(text), "%.0e", 0.005) != 6 || strcmp(text, "0e-001"))
        return 23;
    struct capture capture = {{0}, 0, 0, 0};
    struct hpclang_stdio_callbacks callbacks = {write_byte, NULL, NULL, &capture};
    if (hpclang_stdio_bind(&callbacks))
        return 11;
    unsigned failure = 0;
    if (printf("std:%f/", 1.25) != 13 || legacy_stdout("old:%f", 1.25) != 8 || fflush(stdout) ||
        capture.used != 21 || memcmp(capture.bytes, "std:1.250000/old:1,25", 21))
        failure = 12;
    capture.used = 0;
    if (!failure && (legacy_stream(stdout, "%c/%010f", 0, -1.25) != 12 || fflush(stdout) ||
                     capture.used != 12 || memcmp(capture.bytes, "\0/00000-1,25", 12)))
        failure = 13;
    capture.fail_stderr = 1;
    if (!failure) {
        int result = legacy_stream(stderr, "%f", 1.25);
        int write_errno = errno;
        if (result != -1)
            failure = 14;
        else if (!ferror(stderr))
            failure = 17;
        else if (write_errno != EIO)
            failure = 18;
    }
    clearerr(stderr);
    capture.fail_stderr = 0;
    capture.used = 0;
    if (!failure && (legacy_stream(stderr, "%f", 1.25) != 4 || fflush(stderr) ||
                     capture.used != 4 || memcmp(capture.bytes, "1,25", 4) || capture.streams != 6))
        failure = 15;
    if (hpclang_stdio_unbind() || ferror(stdout) || ferror(stderr))
        failure = 16;
    return failure;
}

int main(void)
{
    unsigned failure = verify();
    /* The return verifier requires a visible transition from the application
     * to Stock. Keep the same borrowed-display marker as other runtime suites. */
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0);
    hpclang_surface_fill_rect(surface, 20, 28, 90, 8, 255);
    hpclang_e2e_result = failure ? UINT32_C(0x46417f00) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
    return 0;
}
