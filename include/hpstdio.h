#ifndef HPCLANG_HPGCC_HPSTDIO_H
#define HPCLANG_HPGCC_HPSTDIO_H

/*
 * Historical hpstdio.h compatibility surface.
 *
 * Standard C stdio stays standard C in hpclang. The only retained historical
 * convenience in this slice is fdelete(), mapped to the clean byte-stream
 * namespace rather than to a raw KOS file call.
 */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int fdelete(char *filename);

#ifdef __cplusplus
}
#endif

#endif
