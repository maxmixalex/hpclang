#include "sound-session.h"
#include <errno.h>

static int valid(const struct hpclang_sound_session_bus *bus,
                 const struct hpclang_sound_session *session)
{
    return bus && session && bus->sound.read && bus->sound.write && bus->watchdog.read &&
           bus->watchdog.write && bus->irq.read && bus->irq.write && bus->irq.exchange_bank;
}
static void stop_peripherals(const struct hpclang_sound_session_bus *bus,
                             struct hpclang_sound_session *session)
{
    hpclang_sound_release(&bus->sound, &session->tone);
    hpclang_watchdog_release(&bus->watchdog, &session->alarm);
}
void hpclang_sound_session_close(const struct hpclang_sound_session_bus *bus,
                                 struct hpclang_sound_session *session)
{
    if (!valid(bus, session))
        return;
    stop_peripherals(bus, session);
    hpclang_irq_owner_release(&bus->irq, &session->irq);
    session->state = SOUND_SESSION_IDLE;
    session->error = 0;
}
int hpclang_sound_session_start(const struct hpclang_sound_session_bus *bus,
                                struct hpclang_sound_session *session, uint64_t num, uint64_t den,
                                uint32_t frequency, uint64_t now, uint64_t deadline,
                                uint32_t stack_top, uint32_t handler, int replace)
{
    if (!valid(bus, session) || !num || !den || !frequency || deadline <= now || !stack_top ||
        (stack_top & 7) || !handler || (handler & 3)) {
        errno = EINVAL;
        return -1;
    }
    if (session->state == SOUND_SESSION_RUNNING && !replace) {
        errno = EBUSY;
        return -1;
    }
    hpclang_sound_session_close(bus, session);
    if (hpclang_irq_owner_acquire(&bus->irq, &session->irq, 0x200, stack_top, handler) ||
        hpclang_watchdog_acquire(&bus->watchdog, &session->alarm, num, den, now, deadline) ||
        hpclang_sound_acquire(&bus->sound, &session->tone, num, den, frequency)) {
        int error = errno;
        hpclang_sound_session_close(bus, session);
        errno = error;
        return -1;
    }
    session->deadline = deadline;
    session->last_time = now;
    session->state = SOUND_SESSION_RUNNING;
    bus->irq.write(bus->irq.context, IRQ_OWNER_MASK, ~UINT32_C(0x200));
    return 0;
}
void hpclang_sound_session_advance(const struct hpclang_sound_session_bus *bus,
                                   struct hpclang_sound_session *session, uint64_t now,
                                   int clock_error)
{
    if (!valid(bus, session) || session->state != SOUND_SESSION_RUNNING)
        return;
    int saved_errno = errno;
    int error = clock_error ? clock_error : now < session->last_time ? EIO : 0;
    int result = -1;
    if (!error) {
        session->last_time = now;
        result = hpclang_watchdog_advance(&bus->watchdog, &session->alarm, now, session->deadline);
        if (result < 0)
            error = errno;
    }
    if (error || result) {
        stop_peripherals(bus, session);
        bus->irq.write(bus->irq.context, IRQ_OWNER_MASK, UINT32_MAX);
        session->error = error;
        session->state = error ? SOUND_SESSION_FAILED : SOUND_SESSION_DONE;
    }
    errno = saved_errno;
}
