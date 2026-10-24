#include "win-timers.h"
#include <errno.h>
#include <string.h>

static int valid(const struct hpclang_win_timers *timers)
{
    if (!timers || !timers->clock || (timers->capacity && !timers->slots)) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}
static int sample(struct hpclang_win_timers *timers, uint64_t *now)
{
    if (timers->clock(timers->clock_data, now))
        return -1;
    if (timers->clock_valid && *now < timers->last_time) {
        errno = EIO;
        return -1;
    }
    timers->last_time = *now;
    timers->clock_valid = 1;
    return 0;
}
static void insert(struct hpclang_win_timers *timers, struct hpclang_win_timer *timer)
{
    struct hpclang_win_timer **link = &timers->head;
    while (*link && timer->deadline > (*link)->deadline)
        link = &(*link)->next;
    timer->next = *link;
    timer->state = 1;
    *link = timer;
}
int hpclang_win_timers_init(struct hpclang_win_timers *timers, struct hpclang_win_timer *slots,
                            size_t capacity, hpclang_win_clock_fn clock, void *data,
                            struct hpclang_win_events *events)
{
    if (!timers || !clock || (capacity && !slots) || capacity > SIZE_MAX / sizeof(*slots)) {
        errno = EINVAL;
        return -1;
    }
    if (capacity)
        memset(slots, 0, capacity * sizeof(*slots));
    *timers = (struct hpclang_win_timers){slots, NULL, capacity, 0, clock, data, events, 0, 0};
    return 0;
}
int hpclang_win_timers_reset(struct hpclang_win_timers *timers)
{
    if (!valid(timers))
        return -1;
    if (timers->depth) {
        errno = EBUSY;
        return -1;
    }
    for (size_t i = 0; i < timers->capacity; ++i)
        timers->slots[i].state = 0;
    timers->head = NULL;
    timers->clock_valid = 0;
    return 0;
}
static int add(struct hpclang_win_timers *timers, hpclang_win_timer_fn function, int event,
               void *data, uint64_t interval, int periodic)
{
    if (!valid(timers))
        return -1;
    if (!interval || (!function && event < 0)) {
        errno = EINVAL;
        return -1;
    }
    if (!function && !timers->events) {
        errno = ENOSYS;
        return -1;
    }
    size_t index = 0;
    while (index < timers->capacity && timers->slots[index].state)
        ++index;
    if (index == timers->capacity) {
        errno = ENOSPC;
        return -1;
    }
    uint64_t now;
    if (sample(timers, &now))
        return -1;
    if (interval > UINT64_MAX - now) {
        errno = EOVERFLOW;
        return -1;
    }
    struct hpclang_win_timer *timer = timers->slots + index;
    *timer = (struct hpclang_win_timer){function, data,           event,    !!periodic,
                                        0,        now + interval, interval, NULL};
    insert(timers, timer);
    return 0;
}
int hpclang_win_timers_add(struct hpclang_win_timers *timers, hpclang_win_timer_fn function,
                           void *data, uint64_t interval, int periodic)
{
    return add(timers, function, -1, data, interval, periodic);
}
int hpclang_win_timers_add_event(struct hpclang_win_timers *timers, int event, void *data,
                                 uint64_t interval, int periodic)
{
    return add(timers, NULL, event, data, interval, periodic);
}
static int remove_timer(struct hpclang_win_timers *timers, hpclang_win_timer_fn function, int event,
                        void *data)
{
    if (!valid(timers))
        return -1;
    if (!function && event < 0) {
        errno = EINVAL;
        return -1;
    }
    struct hpclang_win_timer **link = &timers->head;
    while (*link) {
        struct hpclang_win_timer *timer = *link;
        if (timer->function == function && timer->data == data &&
            (function || timer->event == event)) {
            *link = timer->next;
            timer->next = NULL;
            timer->state = 0;
            break;
        }
        link = &timer->next;
    }
    return 0;
}
int hpclang_win_timers_remove(struct hpclang_win_timers *timers, hpclang_win_timer_fn function,
                              void *data)
{
    return remove_timer(timers, function, -1, data);
}
int hpclang_win_timers_remove_event(struct hpclang_win_timers *timers, int event, void *data)
{
    return remove_timer(timers, NULL, event, data);
}
int hpclang_win_timers_pump(struct hpclang_win_timers *timers, size_t budget)
{
    if (!valid(timers))
        return -1;
    size_t delivered = 0;
    uint64_t now;
    if (sample(timers, &now))
        return -1;
    for (;;) {
        struct hpclang_win_timer *timer = timers->head;
        if (!timer || timer->deadline > now)
            return 0;
        if (delivered == budget)
            return 1;
        timers->head = timer->next;
        timer->next = NULL;
        timer->state = 2;
        ++timers->depth;
        int result = 0, failed = 0;
        if (timer->function)
            result = timer->function(timer->data);
        else
            failed = hpclang_win_events_post(timers->events, timer->event, timer->data, &result);
        --timers->depth;
        if (failed) {
            insert(timers, timer);
            return -1;
        }
        ++delivered;
        if (sample(timers, &now)) {
            timer->state = 0;
            return -1;
        }
        if (result) {
            timer->state = 0;
            continue;
        }
        uint64_t base = timer->periodic ? timer->deadline : now;
        if (timer->interval > UINT64_MAX - base) {
            timer->state = 0;
            errno = EOVERFLOW;
            return -1;
        }
        timer->deadline = base + timer->interval;
        insert(timers, timer);
    }
}
