#include <assert.h>
#include <errno.h>
#include <string.h>
#include "sound-engine.h"

struct hardware
{
    uint32_t reg[10];
    unsigned writes;
};
static uint32_t read_reg(void *context, enum hpclang_sound_reg reg)
{
    return ((struct hardware *)context)->reg[reg];
}
static void write_reg(void *context, enum hpclang_sound_reg reg, uint32_t value)
{
    struct hardware *h = context;
    ++h->writes;
    if (reg == SOUND_PENDING || reg == SOUND_SOURCE_PENDING)
        h->reg[reg] &= ~value;
    else {
        h->reg[reg] = value;
        if (reg == SOUND_TCON && (value & 0x2000))
            h->reg[SOUND_CURRENT] = h->reg[SOUND_COUNT];
    }
}

int main(void)
{
    struct hardware h = {{0x6300, 0x121, 0x90009, 400, 123, 222, 0x55555, 0xffffffff, 0x40, 0x40},
                         0};
    struct hpclang_sound_bus bus = {read_reg, write_reg, &h};
    struct hpclang_sound_saved state = {0};
    uint32_t original[10];
    memcpy(original, h.reg, sizeof(original));
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == 0);
    assert(h.reg[SOUND_TCFG0] == original[SOUND_TCFG0]);
    assert(h.reg[SOUND_COUNT] == 249 && h.reg[SOUND_COMPARE] == 124);
    assert((h.reg[SOUND_PIN] & 0x30) == 0x20);
    assert((h.reg[SOUND_TCON] & 0xf000) == 0x9000);
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == -1 && errno == EBUSY);
    /* New pending IRQs from this borrowed channel must not escape to Stock. */
    h.reg[SOUND_SOURCE_PENDING] |= 0x1000;
    h.reg[SOUND_PENDING] |= 0x1000;
    /* Preserve unrelated changes made while the channel was borrowed. */
    h.reg[SOUND_TCON] ^= 0x400;
    h.reg[SOUND_TCFG1] ^= 0x4000;
    h.reg[SOUND_PIN] ^= 0x40000;
    original[SOUND_TCON] ^= 0x400;
    original[SOUND_TCFG1] ^= 0x4000;
    original[SOUND_PIN] ^= 0x40000;
    hpclang_sound_release(&bus, &state);
    assert(!state.owned && memcmp(h.reg, original, sizeof(original)) == 0);
    unsigned writes = h.writes;
    hpclang_sound_release(&bus, &state);
    assert(h.writes == writes);
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 0) == -1 && errno == EINVAL);
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, UINT32_MAX) == -1 && errno == ERANGE);
    assert(h.writes == writes);
    h.reg[SOUND_TCON] |= 0x1000;
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == -1 && errno == EBUSY);
    h.reg[SOUND_TCON] &= ~0x1000u;
    h.reg[SOUND_INTMASK] &= ~0x1000u;
    h.reg[SOUND_SOURCE_PENDING] |= 0x1000;
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == -1 && errno == EBUSY);
    h.reg[SOUND_SOURCE_PENDING] &= ~0x1000u;
    assert(h.writes == writes);
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == 0);
    assert(h.reg[SOUND_INTMASK] & 0x1000);
    hpclang_sound_release(&bus, &state);
    assert(!(h.reg[SOUND_INTMASK] & 0x1000));
    writes = h.writes;
    h.reg[SOUND_INTMASK] |= 0x1000;
    h.reg[SOUND_TCFG1] |= 3u << 20;
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1000) == -1 && errno == EBUSY);
    assert(h.writes == writes);
    h.reg[SOUND_TCFG1] &= ~(15u << 20);
    h.reg[SOUND_SOURCE_PENDING] |= 0x1000;
    h.reg[SOUND_PENDING] |= 0x1000;
    memcpy(original, h.reg, sizeof(original));
    assert(hpclang_sound_acquire(&bus, &state, 50000000, 1, 1) == 0);
    hpclang_sound_release(&bus, &state);
    assert(memcmp(h.reg, original, sizeof(original)) == 0);
    return 0;
}
