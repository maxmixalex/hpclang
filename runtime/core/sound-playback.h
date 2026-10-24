#ifndef HPCLANG_SOUND_PLAYBACK_H
#define HPCLANG_SOUND_PLAYBACK_H
#include "sound-alarm.h"
#include "sound-engine.h"

struct hpclang_sound_playback
{
    struct hpclang_sound_saved tone;
    struct hpclang_sound_alarm alarm;
    uint64_t deadline;
};

/* Internal paired PWM2/PWM4 ownership. All calls require IRQ/FIQ masking and
 * stable PCLK/prescaler. The caller owns interrupt routing and clock sampling.
 * Begin rolls back the alarm if tone admission fails. Advance returns 0 while
 * running, 1 after restoring both resources at the deadline, or -1 on error.
 * Stop is idempotent. No callback, allocation, clock read or supervisor call
 * occurs here, so an owned IRQ dispatcher may advance/stop the playback.
 * The caller must stop on clock failure and preserve interrupted errno. */
int hpclang_sound_playback_begin(const struct hpclang_sound_bus *sound,
                                 const struct hpclang_sound_alarm_bus *alarm,
                                 struct hpclang_sound_playback *playback, uint64_t clock_num,
                                 uint64_t clock_den, uint32_t frequency, uint64_t now,
                                 uint64_t deadline);
int hpclang_sound_playback_advance(const struct hpclang_sound_bus *sound,
                                   const struct hpclang_sound_alarm_bus *alarm,
                                   struct hpclang_sound_playback *playback, uint64_t now);
void hpclang_sound_playback_stop(const struct hpclang_sound_bus *sound,
                                 const struct hpclang_sound_alarm_bus *alarm,
                                 struct hpclang_sound_playback *playback);
#endif
