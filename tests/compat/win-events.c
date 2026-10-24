#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*callback)(void *, void *);

#ifdef REFERENCE
#include "winpriv.h"
struct win_eventqueue _win_sysqueue;
static struct win_eventhandler *rows[EQ_LEV1SIZE][EQ_LEV2SIZE];
void win_init(void)
{
}
static void reset(void)
{
    for (unsigned i = 0; i < EQ_LEV1SIZE; ++i) {
        _win_sysqueue.handler_chains[i] = rows[i];
        for (unsigned j = 0; j < EQ_LEV2SIZE; ++j) {
            while (rows[i][j]) {
                struct win_eventhandler *next = rows[i][j]->next;
                free(rows[i][j]);
                rows[i][j] = next;
            }
        }
    }
}
static void add(int event, int priority, callback function, void *data)
{
    win_add_event_handler(event, priority, function, data);
}
static void remove_handler(int event, callback function, void *data)
{
    win_remove_event_handler(event, function, data);
}
static int post(int event, void *data)
{
    return win_post_event(event, data);
}
#else
#include "win-events.h"
static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[256];
static struct hpclang_win_call snapshots[512];
static void reset(void)
{
    assert(!hpclang_win_events_init(&events, handlers, 256, snapshots, 512));
}
static void add(int event, int priority, callback function, void *data)
{
    assert(!hpclang_win_events_add(&events, event, priority, function, data));
}
static void remove_handler(int event, callback function, void *data)
{
    assert(!hpclang_win_events_remove(&events, event, function, data));
}
static int post(int event, void *data)
{
    int result = 999;
    assert(!hpclang_win_events_post(&events, event, data, &result));
    return result;
}
#endif

static int ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
static int record(void *data, void *context)
{
    int id = *(int *)context;
    printf("call %d %d\n", *(int *)data, id);
    return id == 6 ? -17 : id == 7 ? 23 : 0;
}
static int nesting;
static int mutate(void *data, void *context)
{
    (void)context;
    printf("mutate %d\n", nesting);
    if (!nesting) {
        remove_handler(0, record, ids + 1);
        add(0, 0, record, ids + 2);
        nesting = 1;
        printf("nested-result %d\n", post(0, data));
        nesting = 0;
    }
    return 0;
}
static int self_remove(void *data, void *context)
{
    remove_handler(0, self_remove, context);
    ++*(int *)data;
    printf("self %d\n", *(int *)data);
    return 0;
}
static uint32_t random_step(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

int main(void)
{
    int payload = 42;
    reset();
    add(0, 0, record, ids);
    add(0, 0, record, ids + 1);
    add(0, INT_MAX, record, ids + 2);
    add(0, INT_MIN, record, ids + 3);
    printf("ordered %d\n", post(0, &payload));
    /* Different-priority duplicates: original removal walks dispatch order,
     * rather than removing the most recently registered duplicate globally. */
    add(0, 100, record, ids + 4);
    add(0, -100, record, ids + 4);
    remove_handler(0, record, ids + 4);
    printf("duplicate %d\n", post(0, &payload));
    reset();
    add(0, 0, record, ids + 1);
    add(0, 10, mutate, NULL);
    printf("outer %d\n", post(0, &payload));
    remove_handler(0, mutate, NULL);
    printf("future %d\n", post(0, &payload));
    reset();
    add(0, 0, self_remove, NULL);
    printf("self-result %d\n", post(0, &payload));
    printf("removed-result %d\n", post(0, &payload));
    for (uint32_t seed = 1; seed <= 32; ++seed) {
        reset();
        uint32_t state = seed;
        for (unsigned step = 0; step < 120; ++step) {
            unsigned operation = random_step(&state) % 3;
            int event = (int)((random_step(&state) >> 16) % 4);
            int priority = (int)((random_step(&state) >> 16) % 5) - 2;
            unsigned id = (random_step(&state) >> 16) % 8;
            if (!operation)
                add(event, priority, record, ids + id);
            else if (operation == 1)
                remove_handler(event, record, ids + id);
            else
                printf("random %u %u %d\n", seed, step, post(event, &payload));
        }
        for (int event = 0; event < 4; ++event)
            printf("drain %u %d %d\n", seed, event, post(event, &payload));
    }
    reset();
    return 0;
}
