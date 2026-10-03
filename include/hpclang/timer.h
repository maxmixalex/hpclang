#ifndef HPCLANG_TIMER_H
#define HPCLANG_TIMER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Caller-owned cooperative timer. Initialize with {0}; do not modify fields
     * while armed. No allocation, interrupts, callbacks or hardware reprogramming.
     * Poll from the application event loop; cancellation is immediate. */
    struct hpclang_timer
    {
        uint64_t deadline_us;
        uint64_t period_us;
        unsigned armed;
    };

    /* First expiration is delay_us from now. period_us=0 means one shot.
     * Starting an armed timer replaces its schedule only on success. */
    int hpclang_timer_start(struct hpclang_timer *timer, uint64_t delay_us, uint64_t period_us);

    /* Return all expirations since the previous poll in *count. Periodic deadlines
     * stay aligned to their original schedule; a late poll does not create drift.
     * A cancelled/unarmed timer reports zero without sampling the clock.
     * Both functions return 0 or -1 (EINVAL, EIO, EOVERFLOW); timer and output
     * remain unchanged on failure. There is no implicit wait or background work. */
    int hpclang_timer_poll(struct hpclang_timer *timer, uint64_t *count);
    void hpclang_timer_cancel(struct hpclang_timer *timer);

#ifdef __cplusplus
}
#endif
#endif
