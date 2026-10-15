#ifndef HPCLANG_STAGE_STACK_H
#define HPCLANG_STAGE_STACK_H

#include <stdint.h>

struct hpclang_stage_stack_io
{
    void *context;
    int (*read)(void *context, uint32_t address, unsigned width, uint32_t *value);
    int (*write)(void *context, uint32_t address, uint32_t value);
};

struct stage_launcher_bias
{
    uint32_t object;
    uint32_t active;
};

/* Invocation-local stack reservation, while native execution is suspended.
 * Hide returns 1 for a recognized launcher cell, 0 when absent/unreadable,
 * -1 on write failure. Restore returns nonzero on success. The caller must
 * retain the hidden object as a relocation root and reserve its stack cell.
 * These operations do not change AVMEM or supply rollback for failed writes. */
int hpclang_stage_stack_hide(const struct hpclang_stage_stack_io *io, uintptr_t stage0_code,
                             struct stage_launcher_bias *bias);
int hpclang_stage_stack_restore(const struct hpclang_stage_stack_io *io,
                                const struct stage_launcher_bias *bias);

#endif
