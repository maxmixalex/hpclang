#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpstdio.h>

int _vsscanf(const char *text, const char *format, va_list args)
{
    return vsscanf(text, format, args);
}

int _sscanf(const char *text, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = _vsscanf(text, format, args);
    va_end(args);
    return result;
}

int _scanf(const char *format, ...)
{
    char line[1024];
    if (!hpclang_hpgcc_gets(line, sizeof line))
        return EOF;
    va_list args;
    va_start(args, format);
    int result = _vsscanf(line, format, args);
    va_end(args);
    return result;
}
