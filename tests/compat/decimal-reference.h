#ifndef HPCLANG_DECIMAL_REFERENCE_H
#define HPCLANG_DECIMAL_REFERENCE_H
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Host support only. The three original formatter sources are caller-supplied.
 * Integer conversion and powers of ten use host services, so this probe does
 * not establish ARM math precision or historical overflow behavior. */
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
#define min(a, b) ((a) < (b) ? (a) : (b))
int get_decimal_digits(void);
void set_decimal_digits(int value);
char *get_decimal_separator(void);
void set_decimal_separator(char value);
double dround(double value, int digits);
double ipow(double value, int exponent);
double i10powx(int exponent);
char *__dtoa(double value, char *buffer, char exponent);
char *itoa(int value, char *buffer, int base);
char *utoa(unsigned value, char *buffer, int base);
char *lltoa(LONGLONG value, char *buffer, int base);
char *ulltoa(ULONGLONG value, char *buffer, int base);
char *upper(char *buffer, char *source);
char *xpad(char *buffer, char *source, int width, int left, char fill);
int legacy_sprintf(char *buffer, const char *format, ...);
int legacy_vsprintf(char *buffer, const char *format, va_list arguments);
#ifndef HPCLANG_REFERENCE_SUPPORT
#define sprintf legacy_sprintf
#define vsprintf legacy_vsprintf
#endif
#endif
