#ifndef HPCLANG_HPGCC_HPSTDIO_H
#define HPCLANG_HPGCC_HPSTDIO_H

/*
 * Historical hpstdio.h compatibility surface.
 *
 * Standard C stdio stays standard C in hpclang. fdelete() uses the clean
 * byte-stream namespace. Numeric input explicitly selects the lazy console
 * when no standard-stream owner has been bound.
 */
#include <stdio.h>
#include <hpclang/hpgcc-format.h>

#ifndef HPCLANG_HPGCC_LONG_TYPES_DEFINED
#define HPCLANG_HPGCC_LONG_TYPES_DEFINED
typedef unsigned long long ULONGLONG;
typedef long long LONGLONG;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    int fdelete(char *filename);
    /* Editable line of at most 31 calculator bytes. Decimal integer / C double
     * conversion accepts a numeric prefix, as historical atoi/atof did.
     * No conversion: zero/EINVAL. Integer overflow: INT_MIN/MAX and ERANGE;
     * double range errors follow strtod. Input error/cancel: zero with the
     * input errno; empty EOF: zero. Inspect stream flags to distinguish EOF.
     * Successful conversion preserves errno. */
    int readint(void);
    double readdouble(void);
    /* Historical integers do not skip whitespace or accept '+'. Signed forms
     * accept a leading '-'; unsigned forms do not. readhex accepts bare hex
     * digits, not a 0x prefix. Range errors saturate with ERANGE. */
    unsigned readuint(void);
    unsigned readhex(void);
    LONGLONG readlonglong(void);
    ULONGLONG readulonglong(void);

#ifdef __cplusplus
}
#endif

#endif
