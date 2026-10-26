#ifndef HPCLANG_HPGCC_FORMAT_H
#define HPCLANG_HPGCC_FORMAT_H
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
#include <cstdio>
extern "C"
{
#endif
    /* Process-local legacy settings: default six digits and '.', capped at 15.
     * Negative digits or a NUL separator leave the setting unchanged/EINVAL. */
    int get_decimal_digits(void);
    void set_decimal_digits(int value);
    char *get_decimal_separator(void);
    void set_decimal_separator(char value);
    /* Historical grammar, not the standard printf grammar; see compatibility guide.
     * Bounded calls return required length. Errors return -1 with errno and retain
     * terminated partial output. sprintf destinations must fit, as in standard C.
     * Stream calls use the supplied FILE and preserve its ordinary error state. */
    int hpclang_hpgcc_snprintf(char *buffer, size_t capacity, const char *format, ...);
    int hpclang_hpgcc_vsnprintf(char *buffer, size_t capacity, const char *format,
                                va_list arguments);
    int hpclang_hpgcc_sprintf(char *buffer, const char *format, ...);
    int hpclang_hpgcc_vsprintf(char *buffer, const char *format, va_list arguments);
    int hpclang_hpgcc_fprintf(FILE *stream, const char *format, ...);
    int hpclang_hpgcc_vfprintf(FILE *stream, const char *format, va_list arguments);
    int hpclang_hpgcc_printf(const char *format, ...);
    int hpclang_hpgcc_vprintf(const char *format, va_list arguments);
#ifdef __cplusplus
}
#endif
/* Define to 1 before the first compatibility include, in application TUs only.
 * Omitted/0 preserves standard calls. Named hpclang_hpgcc_* functions are always
 * available. Parenthesized standard names and function addresses stay standard;
 * C++ may use (std::snprintf)(...) to bypass these unqualified call aliases. */
#if defined(HPCLANG_HPGCC_LEGACY_FORMAT) && HPCLANG_HPGCC_LEGACY_FORMAT
#define snprintf(...) hpclang_hpgcc_snprintf(__VA_ARGS__)
#define vsnprintf(...) hpclang_hpgcc_vsnprintf(__VA_ARGS__)
#define sprintf(...) hpclang_hpgcc_sprintf(__VA_ARGS__)
#define vsprintf(...) hpclang_hpgcc_vsprintf(__VA_ARGS__)
#define fprintf(...) hpclang_hpgcc_fprintf(__VA_ARGS__)
#define vfprintf(...) hpclang_hpgcc_vfprintf(__VA_ARGS__)
#define printf(...) hpclang_hpgcc_printf(__VA_ARGS__)
#define vprintf(...) hpclang_hpgcc_vprintf(__VA_ARGS__)
#endif
#endif
