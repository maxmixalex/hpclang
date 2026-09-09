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

static FILE null_stdio =
    FDEV_SETUP_STREAM(discard_putc, eof_getc, NULL, _FDEV_SETUP_RW);

FILE *const stdin = &null_stdio;
FILE *const stdout = &null_stdio;
FILE *const stderr = &null_stdio;

__attribute__((noreturn))
void _exit(int status)
{
    (void)status;
    for (;;)
        ;
}
