#ifndef HPCLANG_WIN_EVENTS_H
#define HPCLANG_WIN_EVENTS_H

#include <stddef.h>

/* Internal foreground-only foundation for the historical window adapter.
 * Caller-owned registry and snapshot arrays must be disjoint and outlive the
 * context. Callback data remains borrowed through every active dispatch that
 * captured it, even after removal. Initialize only a fresh/inactive context;
 * reset rejects calls from callbacks. No allocation, IRQ work or UI mutation.
 */
typedef int (*hpclang_win_event_fn)(void *event_data, void *app_data);
struct hpclang_win_call
{
    hpclang_win_event_fn function;
    void *data;
};
struct hpclang_win_handler
{
    int event, priority;
    struct hpclang_win_call call;
};
struct hpclang_win_events
{
    struct hpclang_win_handler *handlers;
    struct hpclang_win_call *snapshots;
    size_t capacity, count, snapshot_capacity, used, depth;
};

int hpclang_win_events_init(struct hpclang_win_events *, struct hpclang_win_handler *, size_t,
                            struct hpclang_win_call *, size_t);
int hpclang_win_events_reset(struct hpclang_win_events *);
int hpclang_win_events_add(struct hpclang_win_events *, int event, int priority,
                           hpclang_win_event_fn, void *app_data);
/* Remove the first matching callback/data in dispatch order, matching the
 * original implementation even when duplicate registrations have different
 * priorities. Absence is successful; active snapshots remain unchanged. */
int hpclang_win_events_remove(struct hpclang_win_events *, int event, hpclang_win_event_fn,
                              void *app_data);
/* High priorities first, newer equal priorities first. Each synchronous post
 * captures its own handlers before calling any of them. Nested posts observe
 * mutations immediately; the outer snapshot does not. Stop at nonzero return.
 * Returns 0 on completed dispatch and writes that callback result (or zero).
 * Returns -1/errno on preflight failure, leaving result untouched and invoking
 * no callback. A callback's -1 is a successful dispatch result, not this error.
 * Snapshot exhaustion returns ENOSPC; no partial delivery or deferred payload.
 */
int hpclang_win_events_post(struct hpclang_win_events *, int event, void *event_data, int *result);

#endif
