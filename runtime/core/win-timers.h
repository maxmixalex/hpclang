#ifndef HPCLANG_WIN_TIMERS_H
#define HPCLANG_WIN_TIMERS_H
#include "win-events.h"
#include <stdint.h>

typedef int (*hpclang_win_timer_fn)(void *);
typedef int (*hpclang_win_clock_fn)(void *, uint64_t *);
struct hpclang_win_timer
{
    hpclang_win_timer_fn function; /* NULL for an event timer */
    void *data;
    int event, periodic, state; /* state: 0 free, 1 queued, 2 executing */
    uint64_t deadline, interval;
    struct hpclang_win_timer *next;
};
struct hpclang_win_timers
{
    struct hpclang_win_timer *slots, *head;
    size_t capacity, depth;
    hpclang_win_clock_fn clock;
    void *clock_data;
    struct hpclang_win_events *events;
    uint64_t last_time;
    int clock_valid;
};

/* Internal foreground scheduler, without hardware or allocation. Caller owns
 * slots, clock context, optional event bus and callback data for their complete
 * lifetimes. Clock returns 0/value or -1/errno; intervals use its monotonic unit.
 * Initialize only a fresh/inactive context. Reset starts a new clock epoch and
 * rejects active callbacks. Positive intervals only; arithmetic never wraps.
 */
int hpclang_win_timers_init(struct hpclang_win_timers *, struct hpclang_win_timer *, size_t,
                            hpclang_win_clock_fn, void *, struct hpclang_win_events *);
int hpclang_win_timers_reset(struct hpclang_win_timers *);
int hpclang_win_timers_add(struct hpclang_win_timers *, hpclang_win_timer_fn, void *, uint64_t,
                           int);
int hpclang_win_timers_add_event(struct hpclang_win_timers *, int, void *, uint64_t, int);
int hpclang_win_timers_remove(struct hpclang_win_timers *, hpclang_win_timer_fn, void *);
int hpclang_win_timers_remove_event(struct hpclang_win_timers *, int, void *);
/* Newer equal deadlines run first. Remove affects only the earliest queued
 * match; an executing timer survives removal and stops on nonzero callback
 * result. Nested pumping is supported; executing slots cannot be reused.
 * Periodic timers advance their previous deadline (including catch-up), delay
 * timers restart from the post-callback clock. At most budget callbacks run;
 * return 1 if more are due, 0 if none are due, or -1/errno. No due work is
 * discarded when the budget is reached. SIZE_MAX requests an unbounded drain.
 * Event preflight failure requeues the undelivered timer. A clock/rescheduling
 * failure after delivery drops only that executing timer and reports failure;
 * it must not replay an already delivered callback. Other timers stay queued.
 */
int hpclang_win_timers_pump(struct hpclang_win_timers *, size_t budget);
#endif
