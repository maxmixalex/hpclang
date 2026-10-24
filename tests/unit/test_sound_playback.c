#include <assert.h>
#include <errno.h>
#include <string.h>
#include "sound-playback.h"

struct hardware
{
    uint32_t regs[12];
    unsigned writes;
};
static const unsigned alarm_regs[] = {0, 1, 2, 10, 11, 7, 8, 9};
static uint32_t sound_read(void *context, enum hpclang_sound_reg reg)
{
    return ((struct hardware *)context)->regs[reg];
}
static void write_index(struct hardware *h, unsigned reg, uint32_t value)
{
    ++h->writes;
    if (reg == 8 || reg == 9)
        h->regs[reg] &= ~value;
    else
        h->regs[reg] = value;
    if (reg == 2) {
        if (value & 0x2000)
            h->regs[5] = h->regs[3];
        if (value & 0x200000)
            h->regs[11] = h->regs[10];
    }
}
static void sound_write(void *context, enum hpclang_sound_reg reg, uint32_t value)
{
    write_index(context, reg, value);
}
static uint32_t alarm_read(void *context, enum hpclang_sound_alarm_reg reg)
{
    return ((struct hardware *)context)->regs[alarm_regs[reg]];
}
static void alarm_write(void *context, enum hpclang_sound_alarm_reg reg, uint32_t value)
{
    write_index(context, alarm_regs[reg], value);
}
int main(void)
{
    struct hardware h = {
        {0x6300, 0x33333, 0x400809, 800, 200, 600, 0x100, UINT32_MAX, 0x40, 0x40, 400, 222}, 0};
    struct hardware original = h;
    struct hpclang_sound_bus sound = {sound_read, sound_write, &h};
    struct hpclang_sound_alarm_bus alarm = {&h, alarm_read, alarm_write};
    struct hpclang_sound_playback playback = {0};
    assert(
        !hpclang_sound_playback_begin(&sound, &alarm, &playback, 50000000, 1, 880, 10, UINT64_MAX));
    assert(playback.tone.owned && playback.alarm.owned && playback.deadline == UINT64_MAX);
    assert((h.regs[2] & 0xf000) == 0x9000 && (h.regs[6] & 0x30) == 0x20);
    struct hardware active = h;
    assert(hpclang_sound_playback_begin(&sound, &alarm, &playback, 50000000, 1, 440, 20, 30) == -1);
    assert(errno == EBUSY && !memcmp(&h, &active, sizeof h));
    for (unsigned i = 0; i < 3; ++i) {
        h.regs[8] |= 0x4000;
        h.regs[9] |= 0x4000;
        assert(!hpclang_sound_playback_advance(&sound, &alarm, &playback, 100 + i));
        assert(playback.tone.owned && playback.alarm.owned);
        assert((h.regs[2] & 0xf000) == 0x9000 && h.regs[8] == 0x40 && h.regs[9] == 0x40);
    }
    assert(hpclang_sound_playback_advance(&sound, &alarm, &playback, UINT64_MAX) == 1);
    assert(!playback.tone.owned && !playback.alarm.owned);
    assert(!memcmp(h.regs, original.regs, sizeof h.regs));
    unsigned writes = h.writes;
    hpclang_sound_playback_stop(&sound, &alarm, &playback);
    assert(h.writes == writes);
    assert(hpclang_sound_playback_advance(&sound, &alarm, &playback, UINT64_MAX) == -1);
    assert(errno == EINVAL && h.writes == writes);

    /* Alarm acquired first: tone contention and frequency failure must roll it
     * back, including the separate stopped counter and shared register bits. */
    for (unsigned busy = 0; busy < 2; ++busy) {
        h = original;
        if (busy)
            h.regs[2] |= 0x1000;
        struct hardware before = h;
        assert(hpclang_sound_playback_begin(&sound, &alarm, &playback, 50000000, 1,
                                            busy ? 880 : UINT32_MAX, 1, 1000) == -1);
        assert(errno == (busy ? EBUSY : ERANGE));
        assert(!playback.tone.owned && !playback.alarm.owned);
        assert(!memcmp(h.regs, before.regs, sizeof h.regs));
    }
    h = original;
    h.regs[2] |= 0x100000;
    struct hardware before = h;
    assert(hpclang_sound_playback_begin(&sound, &alarm, &playback, 50000000, 1, 880, 1, 1000) ==
           -1);
    assert(errno == EBUSY && !memcmp(&h, &before, sizeof h));
    h = original;
    assert(hpclang_sound_playback_begin(NULL, &alarm, &playback, 50000000, 1, 880, 1, 1000) == -1);
    assert(errno == EINVAL && !h.writes);
    assert(!hpclang_sound_playback_begin(&sound, &alarm, &playback, 50000000, 1, 880, 1, 1000));
    hpclang_sound_playback_stop(&sound, &alarm, &playback);
    assert(!memcmp(h.regs, original.regs, sizeof h.regs));
    return 0;
}
