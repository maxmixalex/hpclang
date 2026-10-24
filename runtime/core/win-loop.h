#ifndef HPCLANG_WIN_LOOP_H
#define HPCLANG_WIN_LOOP_H
#include "win-events.h"

/* Foreground loop state; the event bus and its callback data remain borrowed.
 * Initialize only a fresh/inactive context. Nested runs deliberately share the
 * quit flag, matching the historical loop: an inner quit also ends the outer
 * run after its current dispatch finishes. No implicit reset of registrations.
 */
struct hpclang_win_loop
{
    struct hpclang_win_events *events;
    int idle_event, stopped;
};
int hpclang_win_loop_init(struct hpclang_win_loop *, struct hpclang_win_events *, int idle_event);
int hpclang_win_loop_quit(struct hpclang_win_loop *);
/* Each entry clears the quit flag. Dispatch idle with NULL event data, at most
 * budget times. Callback consumption stops that dispatch, not the loop. Quit
 * takes effect after the whole current snapshot; it does not cancel callbacks.
 * Returns 0 on quit, 1 at the budget, -1/errno on dispatch failure. SIZE_MAX
 * permits effectively unbounded operation. A later run continues with the
 * existing registry/timers but clears any quit requested before its entry.
 */
int hpclang_win_loop_run(struct hpclang_win_loop *, size_t budget);
#endif
