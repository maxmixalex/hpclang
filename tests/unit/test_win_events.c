#include "win-events.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[8];
static struct hpclang_win_call snapshots[16];
static int trace[32], length;
struct item
{
    int id, result;
};
static struct item low = {1, 0}, old = {2, 0}, recent = {3, 0}, high = {4, 0};

static int record(void *payload, void *data)
{
    struct item *item = data;
    assert(payload && *(int *)payload == 42);
    assert(length < 32);
    trace[length++] = item->id;
    return item->result;
}

static void init(size_t capacity, size_t slots)
{
    assert(!hpclang_win_events_init(&events, handlers, capacity, snapshots, slots));
    length = 0;
}

static void ordering(void)
{
    init(8, 16);
    assert(!hpclang_win_events_add(&events, 7, INT_MIN, record, &low));
    assert(!hpclang_win_events_add(&events, 7, 0, record, &old));
    assert(!hpclang_win_events_add(&events, 7, 0, record, &recent));
    assert(!hpclang_win_events_add(&events, 7, INT_MAX, record, &high));
    assert(!hpclang_win_events_add(&events, 8, INT_MAX, record, &high));
    int payload = 42, result = 99;
    assert(!hpclang_win_events_post(&events, 7, &payload, &result) && !result);
    assert(length == 4 && trace[0] == 4 && trace[1] == 3 && trace[2] == 2 && trace[3] == 1);
    recent.result = -1;
    length = 0;
    assert(!hpclang_win_events_post(&events, 7, &payload, &result) && result == -1);
    assert(length == 2 && trace[0] == 4 && trace[1] == 3);
    assert(!events.used && !events.depth);
    recent.result = 73;
    assert(!hpclang_win_events_post(&events, 7, &payload, &result) && result == 73);
    assert(!events.used && !events.depth);
    recent.result = 0;
    assert(!hpclang_win_events_reset(&events));
    assert(!hpclang_win_events_post(&events, 7, &payload, &result) && !result);
    assert(events.used == 0 && events.depth == 0);
}

static int nested, expect_exhaustion;
static int mutate(void *payload, void *data)
{
    (void)data;
    trace[length++] = 9;
    if (!nested) {
        assert(hpclang_win_events_reset(&events) == -1 && errno == EBUSY);
        assert(!hpclang_win_events_remove(&events, 0, record, &old));
        assert(!hpclang_win_events_add(&events, 0, 0, record, &recent));
        nested = 1;
        int result = 123;
        if (expect_exhaustion) {
            assert(hpclang_win_events_post(&events, 0, payload, &result) == -1 && errno == ENOSPC);
            assert(result == 123);
        } else {
            assert(!hpclang_win_events_post(&events, 0, payload, &result) && !result);
        }
        nested = 0;
    }
    return 0;
}

static void mutations(void)
{
    for (int exhausted = 0; exhausted <= 1; ++exhausted) {
        init(2, exhausted ? 2 : 4);
        expect_exhaustion = exhausted;
        assert(!hpclang_win_events_add(&events, 0, 0, record, &old));
        assert(!hpclang_win_events_add(&events, 0, 10, mutate, NULL));
        int payload = 42, result = 0;
        assert(!hpclang_win_events_post(&events, 0, &payload, &result) && !result);
        if (exhausted) {
            assert(length == 2 && trace[0] == 9 && trace[1] == 2);
        } else {
            assert(length == 4 && trace[0] == 9 && trace[1] == 9 && trace[2] == 3 && trace[3] == 2);
        }
        assert(!events.used && !events.depth);
        assert(!hpclang_win_events_remove(&events, 0, mutate, NULL));
        length = 0;
        assert(!hpclang_win_events_post(&events, 0, &payload, &result));
        assert(length == 1 && trace[0] == 3);
    }
}

static void failures_and_duplicates(void)
{
    init(2, 1);
    assert(!hpclang_win_events_add(&events, 1, 10, record, &old));
    assert(!hpclang_win_events_add(&events, 1, -10, record, &old));
    assert(hpclang_win_events_add(&events, 1, 0, record, &recent) == -1 && errno == ENOSPC);
    int result = 17, payload = 42;
    assert(hpclang_win_events_post(&events, 1, &payload, &result) == -1 && errno == ENOSPC);
    assert(result == 17 && !length && !events.used);
    assert(!hpclang_win_events_remove(&events, 1, record, &old));
    assert(events.count == 1 && handlers[0].priority == -10);
    assert(!hpclang_win_events_post(&events, 1, &payload, &result) && !result && length == 1);
    assert(!hpclang_win_events_remove(&events, 99, record, &old) && events.count == 1);
    assert(hpclang_win_events_add(&events, -1, 0, record, &old) == -1 && errno == EINVAL);
    assert(hpclang_win_events_add(&events, 0, 0, NULL, NULL) == -1 && errno == EINVAL);
    assert(hpclang_win_events_post(&events, -1, &payload, &result) == -1 && errno == EINVAL);
    assert(hpclang_win_events_post(&events, 0, &payload, NULL) == -1 && errno == EINVAL);
    assert(hpclang_win_events_init(&events, NULL, 1, snapshots, 1) == -1 && errno == EINVAL);
    assert(events.count == 1);
    assert(hpclang_win_events_init(&events, handlers, SIZE_MAX, snapshots, 1) == -1);
    assert(!hpclang_win_events_init(&events, NULL, 0, NULL, 0));
    assert(!hpclang_win_events_post(&events, 0, NULL, &result) && !result);
    assert(hpclang_win_events_add(&events, 0, 0, record, &old) == -1 && errno == ENOSPC);
}

static int remove_self(void *payload, void *data)
{
    assert(!hpclang_win_events_remove(&events, 3, remove_self, data));
    ++*(int *)payload;
    ++*(int *)data;
    return 0;
}

static void borrowed_lifetimes(void)
{
    init(1, 1);
    int payload = 42, calls = 0, result = 7;
    assert(!hpclang_win_events_add(&events, 3, 0, remove_self, &calls));
    assert(!hpclang_win_events_post(&events, 3, &payload, &result) && !result);
    assert(payload == 43 && calls == 1 && !events.count && !events.used && !events.depth);
    assert(!hpclang_win_events_post(&events, 3, &payload, &result) && !result);
    assert(payload == 43 && calls == 1);
}

int main(void)
{
    ordering();
    mutations();
    failures_and_duplicates();
    borrowed_lifetimes();
    return 0;
}
