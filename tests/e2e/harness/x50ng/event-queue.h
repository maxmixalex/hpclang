#ifndef HPCLANG_E2E_EVENT_QUEUE_H
#define HPCLANG_E2E_EVENT_QUEUE_H

#include <stdint.h>
#include <stdlib.h>

/* A positive result waits on the same event; zero advances; negative stops. */
typedef int64_t (*hpclang_event_fn)(void *);

struct hpclang_event
{
    int64_t offset;
    hpclang_event_fn callback;
    void *argument;
    struct hpclang_event *next;
};

struct hpclang_event_queue
{
    struct hpclang_event *first;
};

static inline int hpclang_event_add(struct hpclang_event_queue *queue, int64_t offset,
                                    hpclang_event_fn callback, void *argument)
{
    struct hpclang_event *event = malloc(sizeof(*event));
    if (!event || offset < 0) {
        free(event);
        return 0;
    }
    *event = (struct hpclang_event){offset, callback, argument, NULL};
    struct hpclang_event **at = &queue->first;
    while (*at && (*at)->offset <= offset)
        at = &(*at)->next;
    event->next = *at;
    *at = event;
    return 1;
}

/* Run only one action per timer callback. Return the minimum delay until the
 * next action, measured from actual delivery, never from an expired deadline.
 * This keeps a delayed key-down/key-up pair visible to the guest CPU. */
static inline int64_t hpclang_event_advance(struct hpclang_event_queue *queue)
{
    struct hpclang_event *event = queue->first;
    if (!event)
        return -1;
    int64_t result = event->callback(event->argument);
    if (result > 0)
        return result;
    queue->first = event->next;
    int64_t delay = result < 0 || !queue->first ? -1 : queue->first->offset - event->offset;
    free(event);
    return delay;
}

#endif
