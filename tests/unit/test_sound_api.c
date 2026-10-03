#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpclang/device.h>
#include <hpclang/sound.h>
#include "sound-engine.h"

static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};
static uint64_t now, acquired_at, released_at;
static unsigned reads, fail_read, releases;
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_time_read_us(uint64_t *value)
{
    if (++reads == fail_read) {
        errno = EIO;
        return -1;
    }
    *value = now;
    now += 1000;
    return 0;
}
void hpclang_clock_pclk(uint64_t *num, uint64_t *den)
{
    *num = 12000000;
    *den = 1;
}
int hpclang_sound_acquire(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved,
                          uint64_t num, uint64_t den, uint32_t frequency)
{
    (void)bus;
    (void)num;
    (void)den;
    (void)frequency;
    if (saved->owned) {
        errno = EBUSY;
        return -1;
    }
    now += 10000; /* Setup must not consume the requested playback duration. */
    acquired_at = now;
    saved->owned = 1;
    return 0;
}
void hpclang_sound_release(const struct hpclang_sound_bus *bus, struct hpclang_sound_saved *saved)
{
    (void)bus;
    assert(saved->owned);
    saved->owned = 0;
    released_at = now;
    ++releases;
}
static int cancel(void *context)
{
    ++*(unsigned *)context;
    assert(hpclang_sound_play(440, 1, NULL, NULL) == -1 && errno == EBUSY);
    return 1;
}
int main(void)
{
    assert(hpclang_sound_play(440, 5, NULL, NULL) == 0);
    assert(released_at - acquired_at >= 5000 && releases == 1);
    reads = 0;
    fail_read = 2;
    assert(hpclang_sound_play(440, 5, NULL, NULL) == -1 && errno == EIO && releases == 2);
    reads = 0;
    fail_read = 3;
    assert(hpclang_sound_play(440, 5, NULL, NULL) == -1 && errno == EIO && releases == 3);
    fail_read = 0;
    unsigned callbacks = 0;
    assert(hpclang_sound_play(440, 100, cancel, &callbacks) == 1 && callbacks == 1 &&
           releases == 4);
    assert(hpclang_sound_play(0, 5, NULL, NULL) == -1 && errno == EINVAL);
    assert(hpclang_sound_play(440, 0, NULL, NULL) == 0 && releases == 4);
    device.family = HPCLANG_FAMILY_UNKNOWN;
    assert(hpclang_sound_play(440, 5, NULL, NULL) == -1 && errno == ENOSYS);
    return 0;
}
