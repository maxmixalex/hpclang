#include "win-events.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

static int valid(const struct hpclang_win_events *events)
{
    if (!events || (events->capacity && !events->handlers) ||
        (events->snapshot_capacity && !events->snapshots) || events->count > events->capacity ||
        events->used > events->snapshot_capacity) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

int hpclang_win_events_init(struct hpclang_win_events *events, struct hpclang_win_handler *handlers,
                            size_t capacity, struct hpclang_win_call *snapshots, size_t slots)
{
    if (!events || (capacity && !handlers) || (slots && !snapshots) ||
        capacity > SIZE_MAX / sizeof(*handlers) || slots > SIZE_MAX / sizeof(*snapshots)) {
        errno = EINVAL;
        return -1;
    }
    *events = (struct hpclang_win_events){handlers, snapshots, capacity, 0, slots, 0, 0};
    return 0;
}

int hpclang_win_events_reset(struct hpclang_win_events *events)
{
    if (!valid(events))
        return -1;
    if (events->depth) {
        errno = EBUSY;
        return -1;
    }
    events->count = events->used = 0;
    return 0;
}

int hpclang_win_events_add(struct hpclang_win_events *events, int event, int priority,
                           hpclang_win_event_fn function, void *data)
{
    if (!valid(events))
        return -1;
    if (event < 0 || !function) {
        errno = EINVAL;
        return -1;
    }
    if (events->count == events->capacity) {
        errno = ENOSPC;
        return -1;
    }
    size_t index = 0;
    while (index < events->count && events->handlers[index].priority > priority)
        ++index;
    memmove(events->handlers + index + 1, events->handlers + index,
            (events->count - index) * sizeof(*events->handlers));
    events->handlers[index] = (struct hpclang_win_handler){event, priority, {function, data}};
    ++events->count;
    return 0;
}

int hpclang_win_events_remove(struct hpclang_win_events *events, int event,
                              hpclang_win_event_fn function, void *data)
{
    if (!valid(events))
        return -1;
    if (event < 0 || !function) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < events->count; ++i) {
        struct hpclang_win_handler *handler = events->handlers + i;
        if (handler->event == event && handler->call.function == function &&
            handler->call.data == data) {
            memmove(handler, handler + 1, (events->count - i - 1) * sizeof(*handler));
            --events->count;
            break;
        }
    }
    return 0;
}

int hpclang_win_events_post(struct hpclang_win_events *events, int event, void *data, int *result)
{
    if (!valid(events))
        return -1;
    if (event < 0 || !result) {
        errno = EINVAL;
        return -1;
    }
    size_t count = 0;
    for (size_t i = 0; i < events->count; ++i)
        count += events->handlers[i].event == event;
    if (count > events->snapshot_capacity - events->used) {
        errno = ENOSPC;
        return -1;
    }
    size_t first = events->used;
    for (size_t i = 0; i < events->count; ++i)
        if (events->handlers[i].event == event)
            events->snapshots[events->used++] = events->handlers[i].call;
    ++events->depth;
    int value = 0;
    for (size_t i = 0; i < count && !value; ++i) {
        struct hpclang_win_call call = events->snapshots[first + i];
        value = call.function(data, call.data);
    }
    --events->depth;
    events->used = first;
    *result = value;
    return 0;
}
