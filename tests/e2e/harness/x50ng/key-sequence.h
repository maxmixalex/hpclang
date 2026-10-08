#ifndef HPCLANG_KEY_SEQUENCE_H
#define HPCLANG_KEY_SEQUENCE_H
#include <stdio.h>
#include <stdlib.h>
#include "emulator_api.h"
#include "event-queue.h"
#include "timer.h"

#include "key-delivery.h"

static inline int64_t hpclang_key_poll(struct hpclang_key_delivery *s, int key, int down)
{
    /* Stock may stop scanning after handling a press. Release is a switch
     * state change; requiring another guest read can deadlock an idle ROM. */
    int step = hpclang_key_delivery_step(s, timer_get_clock(), down, emulator_key_ready(key),
                                         emulator_key_observed(key) != 0, !is_key_pressed(key));
    if (step == HPCLANG_KEY_EMIT) {
        if (down)
            press_key(key);
        else
            release_key(key);
    }
    if (step < 0) {
        fprintf(stderr, "Key delivery failed: key=%d down=%d ready=%d sampled=%u pressed=%d\n", key,
                down, emulator_key_ready(key), emulator_key_observed(key), is_key_pressed(key));
        return -1;
    }
    return step == HPCLANG_KEY_DONE ? 0 : 10000LL;
}

struct hpclang_key_event
{
    struct hpclang_key_delivery delivery;
    int key;
    int down;
    void (*fail)(void *, const char *);
    void *context;
};

static int64_t hpclang_key_event_tick(void *arg)
{
    struct hpclang_key_event *event = arg;
    int64_t delay = hpclang_key_poll(&event->delivery, event->key, event->down);
    if (delay < 0)
        event->fail(event->context, event->delivery.failure);
    if (delay <= 0)
        free(event);
    return delay;
}

static inline int hpclang_key_add(struct hpclang_event_queue *queue, int64_t us, int key, int down,
                                  void (*fail)(void *, const char *), void *context)
{
    struct hpclang_key_event *event = calloc(1, sizeof(*event));
    if (!event)
        return 0;
    event->key = key;
    event->down = down;
    event->fail = fail;
    event->context = context;
    if (!hpclang_event_add(queue, us, hpclang_key_event_tick, event)) {
        free(event);
        return 0;
    }
    return 1;
}
#endif
