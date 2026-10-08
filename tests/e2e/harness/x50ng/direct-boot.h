#ifndef HPCLANG_DIRECT_BOOT_H
#define HPCLANG_DIRECT_BOOT_H

#include "key-sequence.h"
#include "dialog-sequence.h"

static inline int hpclang_direct_boot(struct hpclang_event_queue *queue, hdw_t *hdw, int height,
                                      int key, void (*fail)(void *, const char *), void *context)
{
    const char *pattern = getenv("HPCLANG_E2E_BOOT_DIALOG");
    if (pattern && *pattern)
        return hpclang_dialog_add(queue, hdw, height, key, 2000000LL, pattern, fail, context);
    return hpclang_key_add(queue, 2000000LL, key, 1, fail, context) &&
           hpclang_key_add(queue, 2050000LL, key, 0, fail, context) &&
           hpclang_key_add(queue, 4000000LL, key, 1, fail, context) &&
           hpclang_key_add(queue, 4050000LL, key, 0, fail, context);
}

#endif
