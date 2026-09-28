#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "event-queue.h"
#include "ready-window.h"

static int key_down;
static int guest_saw_key;
static int gate_ready;

static int64_t press(void *unused)
{
    (void)unused;
    key_down = 1;
    return 0;
}

static int64_t release(void *unused)
{
    (void)unused;
    assert(guest_saw_key);
    key_down = 0;
    return 0;
}

static int64_t gate(void *unused)
{
    (void)unused;
    return gate_ready ? 0 : 100000;
}

int main(void)
{
    struct hpclang_event_queue events = {0};
    /* A host pause has made both nominal key deadlines expire. One callback
     * must still leave 100 ms for the guest to observe the held key. */
    assert(hpclang_event_add(&events, 22500000, release, NULL));
    assert(hpclang_event_add(&events, 22400000, press, NULL));
    assert(hpclang_event_advance(&events) == 100000);
    assert(key_down);
    guest_saw_key = key_down;
    assert(hpclang_event_advance(&events) == -1);
    assert(!key_down && !events.first);

    /* Host completion can take arbitrarily many polls, but the first action
     * after completion retains its own delay and is delivered exactly once. */
    assert(hpclang_event_add(&events, 34000000, gate, NULL));
    assert(hpclang_event_add(&events, 35000000, press, NULL));
    assert(hpclang_event_advance(&events) == 100000);
    assert(hpclang_event_advance(&events) == 100000);
    assert(!key_down);
    gate_ready = 1;
    assert(hpclang_event_advance(&events) == 1000000);
    assert(!key_down);
    assert(hpclang_event_advance(&events) == -1);
    assert(key_down && !events.first);

    struct hpclang_ready_window repaint = {0};
    assert(hpclang_ready_observe(&repaint, 100, 2000000, 0) == 0);
    assert(hpclang_ready_observe(&repaint, 20100, 2000000, 0) == 0);
    assert(hpclang_ready_observe(&repaint, 40100, 2000000, 1) == 1);
    struct hpclang_ready_window blank = {0};
    assert(hpclang_ready_observe(&blank, 100, 2000000, 0) == 0);
    assert(hpclang_ready_observe(&blank, 2000100, 2000000, 0) == -1);
    assert(hpclang_ready_observe(&blank, 2000101, 2000000, 1) == -1);
    puts("PASS: delayed input, host gate, and bounded LCD readiness");
    return 0;
}
