#ifndef HPCLANG_ARMTOOLBOX_PORT_MEMORY_H
#define HPCLANG_ARMTOOLBOX_PORT_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <hpclang/target.h>

/* A Port 1 chain grows upward from physical 0x08020000. Every page in the
 * original ARMToolbox pool stays reserved until native return, including stage
 * code, page tables, application code, stack and heap. Publish only the prefix
 * before the first such ERAM page; never infer free space from heap usage. */
static inline int hpclang_stage_port1_limit(uint32_t model, const volatile uint32_t *pool,
                                            uint32_t count, uint32_t *limit)
{
    if (!limit || !pool || !count || count > 256u ||
        (model != HPCLANG_MODEL_HP49GPLUS && model != HPCLANG_MODEL_HP50G))
        return 0;
    uint32_t end = 0x08040000u;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t descriptor = pool[i];
        uint32_t page = descriptor & 0xfffff000u;
        if (!(descriptor & 2u) || page < 0x08000000u || page >= 0x08080000u)
            return 0;
        if (page >= 0x08020000u && page < end)
            end = page;
    }
    *limit = (end - 0x08020000u) * 2u;
    return 1;
}

#endif
