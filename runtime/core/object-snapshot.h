#ifndef HPCLANG_RUNTIME_OBJECT_SNAPSHOT_H
#define HPCLANG_RUNTIME_OBJECT_SNAPSHOT_H

#include <stdint.h>

#include <hpclang/object.h>

#include "rpl-object-span.h"

struct hpclang_object {
    uint32_t kind;
    uint32_t nibble_count;
    uint32_t native_prologue;
    uint8_t data[];
};

/*
 * Copy one proven-bounded native RPL object into an owned ARM-side snapshot.
 * Returns 0 on success, -1 with errno set on malformed/unsupported/unavailable
 * input or allocation failure.
 */
int hpclang_object_snapshot_copy(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    struct hpclang_object **object);

/* Runtime-private lossless access used by future push/store operations. */
int hpclang_object_snapshot_read_nibble(
    const struct hpclang_object *object,
    uint32_t offset,
    uint8_t *value);

#endif
