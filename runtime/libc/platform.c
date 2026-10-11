#include <stdio.h>
#include <hpclang/display.h>
#include <hpclang/stdio.h>
#include "../core/stdio-routing.h"

static int console_putc(char c, FILE *stream)
{
    return hpclang_stdio_route_write(stream == stderr ? 2u : 1u, (unsigned char)c) == 0
               ? (unsigned char)c
               : _FDEV_ERR;
}

static int console_getc(FILE *stream)
{
    (void)stream;
    int result = hpclang_stdio_route_read();
    return result == HPCLANG_STDIO_EOF     ? _FDEV_EOF
           : result == HPCLANG_STDIO_ERROR ? _FDEV_ERR
                                           : result;
}

static int console_flush(FILE *stream)
{
    return hpclang_stdio_route_flush(stream == stderr ? 2u : 1u);
}

static FILE input_stream = FDEV_SETUP_STREAM(NULL, console_getc, NULL, _FDEV_SETUP_READ);
static FILE output_stream = FDEV_SETUP_STREAM(console_putc, NULL, console_flush, _FDEV_SETUP_WRITE);
static FILE error_stream = FDEV_SETUP_STREAM(console_putc, NULL, console_flush, _FDEV_SETUP_WRITE);

FILE *const stdin = &input_stream;
FILE *const stdout = &output_stream;
FILE *const stderr = &error_stream;

void hpclang_stdio_clear_streams(void)
{
    /* These runtime-owned, unbuffered streams have no outstanding I/O here.
     * Reset pushback/orientation as well as EOF/error on ownership changes. */
    input_stream = (FILE)FDEV_SETUP_STREAM(NULL, console_getc, NULL, _FDEV_SETUP_READ);
    output_stream = (FILE)FDEV_SETUP_STREAM(console_putc, NULL, console_flush, _FDEV_SETUP_WRITE);
    error_stream = (FILE)FDEV_SETUP_STREAM(console_putc, NULL, console_flush, _FDEV_SETUP_WRITE);
}

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    hpclang_display_end();
    for (;;)
        ;
}
