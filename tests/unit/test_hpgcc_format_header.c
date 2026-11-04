#define HPCLANG_HPCONIO_NO_AUTOINIT
#define HPCLANG_HPCONIO_NO_GETS_ALIASES
#include <hpstdio.h>
#include <hpconio.h>
#include <assert.h>
#include <string.h>

#if defined(HPCLANG_HPGCC_LEGACY_FORMAT) && HPCLANG_HPGCC_LEGACY_FORMAT
#ifndef snprintf
#error Legacy call aliases missing
#endif
#define EXPECTED "1,25"
#else
#ifdef snprintf
#error Compatibility include changed standard names without opt-in
#endif
#define EXPECTED "1.250000"
#endif

static int via_va(char *buffer, size_t size, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}

int main(void)
{
    set_decimal_digits(2);
    set_decimal_separator(',');
    char buffer[32];
    assert(snprintf(buffer, sizeof(buffer), "%f", 1.25) == (int)strlen(EXPECTED));
    assert(!strcmp(buffer, EXPECTED));
    assert(via_va(buffer, sizeof(buffer), "%f", 1.25) == (int)strlen(EXPECTED));
    assert(!strcmp(buffer, EXPECTED));
    assert(sprintf(buffer, "%f", 1.25) == (int)strlen(EXPECTED));
    assert(!strcmp(buffer, EXPECTED));
    /* Explicit names work regardless of aliases; addresses remain standard. */
    assert(hpclang_hpgcc_snprintf(buffer, sizeof(buffer), "%f", 1.25) == 4);
    assert(!strcmp(buffer, "1,25"));
    int (*standard)(char *, size_t, const char *, ...) = &snprintf;
    assert(standard(buffer, sizeof(buffer), "%f", 1.25) == 8);
    assert(!strcmp(buffer, "1.250000"));
#ifdef __cplusplus
    assert((std::snprintf)(buffer, sizeof(buffer), "%f", 1.25) == 8);
    assert(!strcmp(buffer, "1.250000"));
#endif
    return 0;
}
