#ifndef HPCLANG_CORE_HPGCC_FORMAT_H
#define HPCLANG_CORE_HPGCC_FORMAT_H
#include <stddef.h>
#include <stdarg.h>

/* Internal building block for the explicitly selected legacy formatter.
 * This module does not intercept standard printf or activate via headers. */
int get_decimal_digits(void);
void set_decimal_digits(int value);
char *get_decimal_separator(void);
void set_decimal_separator(char value);
/* f/e/E/g; precision -1 selects the setting, nonnegative precision is capped
 * at 15. snprintf-style length/truncation, EINVAL for invalid arguments.
 * No field padding here; the format parser owns width and flags. */
int hpclang_hpgcc_format_double(char *buffer, size_t capacity, double value, char conversion,
                                int precision);
/* Internal historical grammar: -/0, decimal width/precision, s/c/d/i/u/x/X/p,
 * b/U/L, f/e/E/g and %. Other grammar reports EINVAL. No stdio interposition.
 * Like snprintf, returns required length; failures retain terminated partial
 * output. The caller's va_list is not consumed. */
int hpclang_hpgcc_vsnprintf(char *buffer, size_t capacity, const char *format, va_list arguments);
int hpclang_hpgcc_snprintf(char *buffer, size_t capacity, const char *format, ...);
#endif
