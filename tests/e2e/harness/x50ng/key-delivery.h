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

/* A press requires readiness and a fresh guest sample after emission.
 * Release requires confirmation that the emulated switch is open: an idle ROM
 * need not scan again until the next press. Neither proves UI acceptance. */
static inline int hpclang_key_delivery_step(struct hpclang_key_delivery *s, int64_t now, int down,
                                            int ready, int sampled, int released)
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
    if (down ? sampled : released)
        return HPCLANG_KEY_DONE;
    if (now < s->deadline)
        return HPCLANG_KEY_WAIT;
    s->failure = down ? "input-not-sampled" : "input-not-released";
    return -1;
}
#endif
