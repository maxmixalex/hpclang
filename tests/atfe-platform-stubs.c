#include <stdio.h>

static int discard_putc(char c, FILE *stream)
{
    (void)stream;
    return (unsigned char)c;
}

static int eof_getc(FILE *stream)
{
    (void)stream;
    return EOF;
}

static FILE test_stdio =
    FDEV_SETUP_STREAM(discard_putc, eof_getc, NULL, _FDEV_SETUP_RW);

FILE *const stdin = &test_stdio;
FILE *const stdout = &test_stdio;
FILE *const stderr = &test_stdio;

__attribute__((noreturn))
void _exit(int status)
{
    (void)status;
    for (;;)
        ;
}
