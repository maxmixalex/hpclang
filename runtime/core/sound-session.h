#ifndef HPCLANG_SOUND_SESSION_H
#define HPCLANG_SOUND_SESSION_H
#include "irq-owner.h"
#include "sound-engine.h"
#include "watchdog-alarm.h"

struct hpclang_sound_session_bus
{
    struct hpclang_sound_bus sound;
    struct hpclang_watchdog_bus watchdog;
    struct hpclang_irq_owner_bus irq;
};
enum hpclang_sound_session_state
{
    SOUND_SESSION_IDLE,
    SOUND_SESSION_RUNNING,
    SOUND_SESSION_DONE,
    SOUND_SESSION_FAILED
};
struct hpclang_sound_session
{
    struct hpclang_sound_saved tone;
    struct hpclang_watchdog_alarm alarm;
    struct hpclang_irq_owner irq;
    uint64_t deadline, last_time;
    volatile enum hpclang_sound_session_state state;
    int error;
};
/* Internal lifecycle; all calls require IRQ/FIQ masked and stable PCLK.
 * Start/close run on the foreground stack; advance may run in the owned ISR.
 * Advance restores peripherals autonomously and masks all controller sources,
 * retaining the IRQ bank/vector until foreground close or the next start.
 * It never frees its executing stack or invokes user code; it preserves errno.
 * Close is the shared cancellation/exit path and restores the IRQ lease last.
 * A replacement first closes our previous session. If new hardware admission
 * fails, the old tone stays stopped and all new leases are rolled back.
 * Invalid basic arguments and a non-replacing overlapping request leave the
 * current session untouched. No CPU IRQ enabling or clock sampling occurs here.
 */
int hpclang_sound_session_start(const struct hpclang_sound_session_bus *,
                                struct hpclang_sound_session *, uint64_t clock_num,
                                uint64_t clock_den, uint32_t frequency, uint64_t now,
                                uint64_t deadline, uint32_t stack_top, uint32_t handler,
                                int replace);
void hpclang_sound_session_advance(const struct hpclang_sound_session_bus *,
                                   struct hpclang_sound_session *, uint64_t now, int clock_error);
void hpclang_sound_session_close(const struct hpclang_sound_session_bus *,
                                 struct hpclang_sound_session *);
#endif
