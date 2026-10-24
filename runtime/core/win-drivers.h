#ifndef HPCLANG_WIN_DRIVERS_H
#define HPCLANG_WIN_DRIVERS_H
#include "win-keyboard.h"
#include "win-loop.h"
#include "win-timers.h"

/* Borrowed contexts for idle handlers. Register keyboard at 2000000002,
 * repaint at 2000000001 and timers at 2000000000 to preserve historical order.
 * All contexts and callback data must outlive active dispatch snapshots.
 * Event buses must be the loop's bus. Timer intervals use microseconds when
 * initialized with hpclang_win_device_clock. SIZE_MAX budget preserves full
 * catch-up; smaller budgets keep due work queued for subsequent idle cycles.
 */
struct hpclang_win_drivers
{
    struct hpclang_win_loop *loop;
    struct hpclang_win_keyboard *keyboard;
    struct hpclang_win_timers *timers;
    size_t timer_budget;
};
int hpclang_win_device_clock(void *, uint64_t *);
int hpclang_win_keyboard_idle(void *, void *);
int hpclang_win_timer_idle(void *, void *);
#endif
