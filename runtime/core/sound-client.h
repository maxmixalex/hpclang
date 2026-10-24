#ifndef HPCLANG_SOUND_CLIENT_H
#define HPCLANG_SOUND_CLIENT_H
#include <stdint.h>

/* Foreground-only identity for compatibility clients sharing the public sound
 * lifetime. A canonical start uses NULL; replacement must not inherit a prior
 * client's policy. The IRQ never reads or writes this identity. */
int hpclang_sound_start_client(uint32_t frequency, uint32_t duration, int replace,
                               const void *client);
int hpclang_sound_is_client(const void *client);
#endif
