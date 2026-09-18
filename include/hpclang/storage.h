#ifndef HPCLANG_STORAGE_H
#define HPCLANG_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Query free bytes in the active byte-stream filesystem.
 *
 * This service is available on HP49g+/HP50g, whose Stock environment exposes
 * the SD-card byte-stream filesystem used by picolibc. Other calculator
 * families return -1 with errno=ENOSYS rather than pretending their native
 * calculator-object stores are POSIX filesystems.
 */
int hpclang_stream_storage_free_bytes(uint64_t *bytes);

#ifdef __cplusplus
}
#endif

#endif
