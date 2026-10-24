#ifndef HPCLANG_WINTIMER_H
#define HPCLANG_WINTIMER_H
#include <winevent.h>
#ifdef __cplusplus
extern "C"
{
#endif
    typedef int (*win_timerfunc_t)(void *);
    /* Positive milliseconds. Periodic timers catch up from their old deadline;
     * delay timers restart after callbacks. Nonzero callback results stop them.
     * Removal affects the earliest queued match, not a currently executing timer.
     * Errors from these historical void functions latch hpclang_win_error(). */
    void win_add_timer(win_timerfunc_t, void *, int interval_ms, int periodic);
    void win_add_timer_event(int event, void *, int interval_ms, int periodic);
    void win_remove_timer(win_timerfunc_t, void *);
    void win_remove_timer_event(int event, void *);
#ifdef __cplusplus
}
#endif
#endif
