#ifndef HPCLANG_KEY_DELIVERY_H
#define HPCLANG_KEY_DELIVERY_H
#include <stdint.h>

enum
{
    HPCLANG_KEY_WAIT,
    HPCLANG_KEY_EMIT,
    HPCLANG_KEY_DONE
};
struct hpclang_key_delivery
{
    int started;
    int delivered;
    int64_t deadline;
    const char *failure;
};

/* Readiness precedes the edge; a fresh guest sample acknowledges the edge.
 * The caller resets its sample counter when it emits a key press/release. */
static inline int hpclang_key_delivery_step(struct hpclang_key_delivery *s, int64_t now, int down,
                                            int ready, int sampled)
{
    if (!s->started) {
        s->started = 1;
        s->deadline = now + 15000000LL;
    }
    if (!s->delivered) {
        if (down && !ready) {
            if (now < s->deadline)
                return HPCLANG_KEY_WAIT;
            s->failure = "input-not-ready";
            return -1;
        }
        s->delivered = 1;
        s->deadline = now + 15000000LL;
        return HPCLANG_KEY_EMIT;
    }
    if (sampled)
        return HPCLANG_KEY_DONE;
    if (now < s->deadline)
        return HPCLANG_KEY_WAIT;
    s->failure = "input-not-sampled";
    return -1;
}
#endif
