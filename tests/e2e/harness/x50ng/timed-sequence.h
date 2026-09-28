#ifndef HPCLANG_E2E_TIMED_SEQUENCE_H
#define HPCLANG_E2E_TIMED_SEQUENCE_H

#include "event-queue.h"
#include "timer.h"

struct hpclang_timed_sequence
{
    struct hpclang_event_queue events;
    hdw_timer_t *timer;
};

static void hpclang_sequence_tick(void *arg)
{
    struct hpclang_timed_sequence *sequence = arg;
    int64_t delay = hpclang_event_advance(&sequence->events);
    if (delay < 0) {
        timer_free(sequence->timer);
        sequence->timer = NULL;
    } else {
        timer_mod(sequence->timer, timer_get_clock() + delay);
    }
}

static inline int hpclang_sequence_start(struct hpclang_timed_sequence *sequence)
{
    if (sequence->timer || !sequence->events.first)
        return 0;
    sequence->timer = timer_new(HDW_TIMER_REALTIME, hpclang_sequence_tick, sequence);
    if (!sequence->timer)
        return 0;
    timer_mod(sequence->timer, timer_get_clock() + sequence->events.first->offset);
    return 1;
}

#endif
