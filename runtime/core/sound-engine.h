#ifndef HPCLANG_SOUND_ENGINE_H
#define HPCLANG_SOUND_ENGINE_H
#include <stdint.h>

enum hpclang_sound_reg
{
    SOUND_TCFG0,
    SOUND_TCFG1,
    SOUND_TCON,
    SOUND_COUNT,
    SOUND_COMPARE,
    SOUND_CURRENT,
    SOUND_PIN,
    SOUND_INTMASK,
    SOUND_SOURCE_PENDING,
    SOUND_PENDING
};
struct hpclang_sound_bus
{
    uint32_t (*read)(void *context, enum hpclang_sound_reg reg);
    void (*write)(void *context, enum hpclang_sound_reg reg, uint32_t value);
    void *context;
};
struct hpclang_sound_saved
{
    uint32_t mux, control, count, compare, current, pin, source_pending, pending, intmask;
    int owned;
};
int hpclang_sound_acquire(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved,
                          uint64_t clock_num, uint64_t clock_den, uint32_t frequency);
void hpclang_sound_release(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved);
#endif
