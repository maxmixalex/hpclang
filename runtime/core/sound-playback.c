#include "sound-playback.h"
#include <errno.h>

static int valid(const struct hpclang_sound_bus *sound, const struct hpclang_sound_alarm_bus *alarm,
                 const struct hpclang_sound_playback *playback)
{
    return sound && sound->read && sound->write && alarm && alarm->read && alarm->write && playback;
}

void hpclang_sound_playback_stop(const struct hpclang_sound_bus *sound,
                                 const struct hpclang_sound_alarm_bus *alarm,
                                 struct hpclang_sound_playback *playback)
{
    if (!valid(sound, alarm, playback))
        return;
    hpclang_sound_release(sound, &playback->tone);
    hpclang_sound_alarm_release(alarm, &playback->alarm);
}

int hpclang_sound_playback_begin(const struct hpclang_sound_bus *sound,
                                 const struct hpclang_sound_alarm_bus *alarm,
                                 struct hpclang_sound_playback *playback, uint64_t clock_num,
                                 uint64_t clock_den, uint32_t frequency, uint64_t now,
                                 uint64_t deadline)
{
    if (!valid(sound, alarm, playback) || !frequency || deadline <= now) {
        errno = EINVAL;
        return -1;
    }
    if (playback->tone.owned || playback->alarm.owned) {
        errno = EBUSY;
        return -1;
    }
    if (hpclang_sound_alarm_acquire_until(alarm, &playback->alarm, clock_num, clock_den, now,
                                          deadline))
        return -1;
    if (hpclang_sound_acquire(sound, &playback->tone, clock_num, clock_den, frequency)) {
        int error = errno;
        hpclang_sound_alarm_release(alarm, &playback->alarm);
        errno = error;
        return -1;
    }
    playback->deadline = deadline;
    return 0;
}

int hpclang_sound_playback_advance(const struct hpclang_sound_bus *sound,
                                   const struct hpclang_sound_alarm_bus *alarm,
                                   struct hpclang_sound_playback *playback, uint64_t now)
{
    if (!valid(sound, alarm, playback) || !playback->tone.owned || !playback->alarm.owned) {
        errno = EINVAL;
        return -1;
    }
    int result = hpclang_sound_alarm_advance(alarm, &playback->alarm, now, playback->deadline);
    if (result) {
        int error = errno;
        hpclang_sound_playback_stop(sound, alarm, playback);
        errno = error;
    }
    return result;
}
