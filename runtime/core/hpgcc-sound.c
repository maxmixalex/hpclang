#include <errno.h>
#include <kos.h>
#include <hpclang/sound.h>
#include "sound-client.h"

static U32 active_mode;

BOOL kos_CheckBeepEnd(void)
{
    int ours = hpclang_sound_is_client(&active_mode);
    int status = hpclang_sound_status();
    return !(ours && status == 1 && (active_mode & 2u));
}

static int wait_current(void)
{
    for (;;) {
        int ours = hpclang_sound_is_client(&active_mode);
        int status = hpclang_sound_status();
        if (status < 0)
            return -1;
        if (!ours || !status || !(active_mode & 2u))
            return 0;
    }
}

S32 kos_beep(U32 frequency, TIME_T duration, U32 mode)
{
    if (!frequency || frequency > 15000u || !duration) {
        errno = EINVAL;
        return -51;
    }
    int status = hpclang_sound_status();
    if (status < 0)
        return -1;
    if (status) {
        if (!hpclang_sound_is_client(&active_mode)) {
            errno = EBUSY;
            return -1;
        }
        if (!(active_mode & 1u))
            return wait_current();
    }
    if (hpclang_sound_start_client(frequency, duration, status != 0, &active_mode))
        return -1;
    active_mode = mode;
    return wait_current();
}
