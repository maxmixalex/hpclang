#ifndef HPCLANG_HPGCC_HPSTRING_H
#define HPCLANG_HPGCC_HPSTRING_H
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* HPGCC 1.x/2.0 integer formatting. Bases 2..36 use lowercase digits;
     * other bases produce "0" (or "-0" for a negative signed value).
     * Returned storage is borrowed, not freed: uints and nonnegative ints share
     * one buffer; negative ints use another. Copy before another relevant call.
     * No allocation, display selection or errno change. Not IRQ/thread-safe. */
    char *uints(unsigned value, int base);
    char *ints(int value, int base);
#ifdef __cplusplus
}
#endif
#endif
