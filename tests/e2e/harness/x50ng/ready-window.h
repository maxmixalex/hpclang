#ifndef HPCLANG_E2E_READY_WINDOW_H
#define HPCLANG_E2E_READY_WINDOW_H

#include <stdint.h>

struct hpclang_ready_window
{
    int started;
    int64_t deadline;
};

/* 1: ready, 0: keep observing, -1: bounded readiness wait expired. */
static inline int hpclang_ready_observe(struct hpclang_ready_window *window, int64_t now,
                                        int64_t budget, int ready)
{
    if (!window->started) {
        window->started = 1;
        window->deadline = now + budget;
    }
    if (now >= window->deadline)
        return -1;
    return ready ? 1 : 0;
}

#endif
