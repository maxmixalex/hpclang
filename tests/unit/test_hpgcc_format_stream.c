#define _POSIX_C_SOURCE 200809L
#define HPCLANG_HPGCC_LEGACY_FORMAT 1
#include <hpstdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static int stream_va(FILE *stream, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = vfprintf(stream, format, arguments);
    va_end(arguments);
    return result;
}
static int stdout_va(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = vprintf(format, arguments);
    va_end(arguments);
    return result;
}
static int buffer_va(char *buffer, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = vsprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}

int main(void)
{
    set_decimal_digits(2);
    set_decimal_separator(',');
    FILE *file = tmpfile();
    assert(file);
    errno = EDOM;
    assert(fprintf(file, "%010f/%L", -1.25, -123LL) == 15 && errno == EDOM);
    assert(stream_va(file, "/%s/%f", "x", 1.25) == 7);
    assert(fprintf(file, "/%c/", 0) == 3);
    assert(fprintf(file, "%130d", 7) == 130);
    assert(!ferror(file));
    rewind(file);
    char buffer[256];
    assert(fread(buffer, 1, 155, file) == 155);
    assert(!memcmp(buffer, "00000-1,25/-123/x/1,25/\0/", 25));
    for (size_t i = 25; i < 154; ++i)
        assert(buffer[i] == ' ');
    assert(buffer[154] == '7');
    assert(buffer_va(buffer, "%f/%d", 1.25, 7) == 6 && !strcmp(buffer, "1,25/7"));
    assert(hpclang_hpgcc_fprintf(NULL, "x") == -1 && errno == EINVAL);
    FILE *readonly = fopen("/dev/null", "r");
    assert(readonly);
    assert(setvbuf(readonly, NULL, _IONBF, 0) == 0);
    assert(fprintf(readonly, "%f", 1.25) == -1 && ferror(readonly));
    assert(fclose(readonly) == 0);
    assert(fclose(file) == 0);
    file = tmpfile();
    assert(file);
    assert(fflush(stdout) == 0);
    int saved = dup(STDOUT_FILENO);
    assert(saved >= 0);
    assert(dup2(fileno(file), STDOUT_FILENO) >= 0);
    int first = printf("%f", 1.25);
    int second = stdout_va("/%f", -1.25);
    assert(fflush(stdout) == 0);
    assert(dup2(saved, STDOUT_FILENO) >= 0);
    close(saved);
    assert(first == 4 && second == 6);
    rewind(file);
    assert(fread(buffer, 1, 10, file) == 10 && !memcmp(buffer, "1,25/-1,25", 10));
    assert(fclose(file) == 0);
    return 0;
}
