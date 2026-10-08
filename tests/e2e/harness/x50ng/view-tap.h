#ifndef HPCLANG_VIEW_TAP_H
#define HPCLANG_VIEW_TAP_H

#include "key-delivery.h"
#include "lcd-stability.h"

enum
{
    HPCLANG_VIEW_WAIT,
    HPCLANG_VIEW_DOWN,
    HPCLANG_VIEW_UP,
    HPCLANG_VIEW_DONE
};

struct hpclang_view_tap
{
    struct hpclang_key_delivery key;
    struct hpclang_lcd_quiescence view;
    int phase;
    int started;
    int64_t deadline;
    int64_t next;
    const char *failure;
};

/* For a dismissible dialog only: retry a tap while the original view remains.
 * A GPIO read is not proof that the UI consumed a key. Never retry on a new or
 * blank view, and require a settled change after releasing the switch. */
static inline int hpclang_view_tap_step(struct hpclang_view_tap *s, int64_t now, uint64_t frame,
                                        int nonblank, int dialog, int ready, int sampled,
                                        int released)
{
    if (!s->started) {
        s->started = 1;
        s->deadline = now + 15000000LL;
    }
    if (now >= s->deadline) {
        s->failure = "dialog-not-dismissed";
        return -1;
    }
    if (s->phase == 0) {
        hpclang_lcd_observe_sample(&s->view, frame, nonblank, 4);
        if (!dialog || !nonblank || s->view.stable_observations < 4)
            return HPCLANG_VIEW_WAIT;
        s->view.baseline = frame;
        s->view.changed_from_baseline = 0;
        s->phase = 1;
    }
    if (s->phase == 1 || s->phase == 3) {
        int down = s->phase == 1;
        int result = hpclang_key_delivery_step(&s->key, now, down, ready, sampled, released);
        if (result < 0) {
            s->failure = s->key.failure;
            return -1;
        }
        if (result == HPCLANG_KEY_EMIT)
            return down ? HPCLANG_VIEW_DOWN : HPCLANG_VIEW_UP;
        if (result == HPCLANG_KEY_WAIT)
            return HPCLANG_VIEW_WAIT;
        s->key = (struct hpclang_key_delivery){0};
        s->phase = down ? 2 : 4;
        s->next = now + (down ? 100000LL : 500000LL);
        if (!down) {
            s->view.current_set = 0;
            s->view.stable_observations = 0;
        }
    }
    if (s->phase == 2 && now >= s->next)
        s->phase = 3;
    if (s->phase == 4) {
        int settled = hpclang_lcd_observe_sample(&s->view, frame, nonblank, 4);
        if (frame != s->view.baseline && settled)
            return HPCLANG_VIEW_DONE;
        if (now >= s->next && nonblank && frame == s->view.baseline &&
            s->view.stable_observations >= 4)
            s->phase = 1;
    }
    return HPCLANG_VIEW_WAIT;
}

#endif
