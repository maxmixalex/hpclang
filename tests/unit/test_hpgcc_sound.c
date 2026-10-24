#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <kos.h>
#include <hpclang/sound.h>
#include "sound-client.h"

static const void *owner;
static unsigned frequency, duration, starts, remaining;
static int running, start_error, status_error, replacement;
static void (*on_poll)(void);

int hpclang_sound_is_client(const void *client)
{
    return running && client && owner == client;
}
int hpclang_sound_status(void)
{
    if (status_error) {
        errno = status_error;
        status_error = running = 0;
        owner = 0;
        return -1;
    }
    if (running && on_poll) {
        void (*callback)(void) = on_poll;
        on_poll = 0;
        callback();
    }
    if (running && remaining && !--remaining) {
        running = 0;
        owner = 0;
    }
    return running;
}
int hpclang_sound_start_client(uint32_t hz, uint32_t ms, int replace, const void *client)
{
    ++starts;
    replacement = replace;
    if (start_error) {
        running = 0;
        owner = 0;
        errno = start_error;
        return -1;
    }
    assert(!running || replace);
    frequency = hz;
    duration = ms;
    owner = client;
    running = 1;
    return 0;
}
void hpclang_sound_stop(void)
{
    running = remaining = 0;
    owner = 0;
}
static unsigned old_mode, new_mode;
static void fail_during_wait(void)
{
    status_error = EIO;
}
static void overlap(void)
{
    assert(kos_CheckBeepEnd() == !(old_mode & 2u));
    unsigned before = starts;
    assert(kos_beep(660, 7, new_mode) == 0);
    assert(starts == before + !!(old_mode & 1u));
    assert(frequency == ((old_mode & 1u) ? 660u : 440u));
}
int main(void)
{
    assert(kos_CheckBeepEnd() == TRUE);
    assert(kos_beep(0, 1, 0) == -51 && errno == EINVAL);
    assert(kos_beep(15001, 1, 0) == -51 && errno == EINVAL);
    assert(kos_beep(UINT32_MAX, 1, 0) == -51 && errno == EINVAL);
    assert(kos_beep(440, 0, 0) == -51 && errno == EINVAL && !starts);
    assert(!kos_beep(1, UINT32_MAX, 1) && frequency == 1 && duration == UINT32_MAX);
    assert(kos_CheckBeepEnd() == TRUE && running);
    assert(!kos_beep(15000, 1, 0) && frequency == 15000 && replacement);
    unsigned before = starts;
    assert(!kos_beep(440, 20, 3) && starts == before && running);
    hpclang_sound_stop();
    /* Exercise the ROM's entire 16-state query table and all four incoming
     * modes, including a nested call while an old waiting request is active.
     * Countdown models independent backend completion, not adapter time. */
    for (old_mode = 0; old_mode < 16; ++old_mode) {
        for (new_mode = 0; new_mode < 4; ++new_mode) {
            remaining = 30;
            on_poll = overlap;
            assert(!kos_beep(440, 10, old_mode));
            assert(!on_poll);
            hpclang_sound_stop();
        }
    }
    assert(!kos_beep(440, 10, 1));
    before = starts;
    assert(kos_beep(0, 10, 1) == -51 && starts == before && running);
    start_error = ERANGE;
    assert(kos_beep(880, 10, 0) == -1 && errno == ERANGE && !running);
    start_error = 0;
    assert(kos_CheckBeepEnd());
    remaining = 30;
    on_poll = fail_during_wait;
    assert(kos_beep(440, 10, 2) == -1 && errno == EIO && !running);
    remaining = 0;
    assert(!kos_beep(440, 10, 0));
    status_error = EIO;
    assert(kos_CheckBeepEnd() && errno == EIO && !running);
    status_error = EIO;
    before = starts;
    assert(kos_beep(440, 10, 0) == -1 && errno == EIO && starts == before);
    /* A later canonical start must not inherit a prior KOS mode. */
    running = 1;
    owner = 0;
    assert(kos_CheckBeepEnd() && running);
    assert(kos_beep(440, 10, 1) == -1 && errno == EBUSY && starts == before);
    hpclang_sound_stop();
    return 0;
}
