#ifndef HPCLANG_SOUND_H
#define HPCLANG_SOUND_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /* Called on the application stack while a tone plays. Return nonzero to stop.
     * Callbacks must return normally; nested tone playback fails with EBUSY. */
    typedef int (*hpclang_sound_cancel_fn)(void *context);

    /* Play a square-wave tone using an idle PWM channel. Frequency is rounded to
     * the nearest available timer period without changing the shared prescaler.
     * This call blocks and polls cancellation until duration_ms has elapsed.
     * With nonzero frequency, zero duration is a no-op. Returns 0 when complete,
     * 1 when cancelled, or -1:
     * EINVAL (zero frequency), ERANGE (unrepresentable frequency), EBUSY (channel
     * in use), EIO (clock failure), ENOSYS (unknown device), EOVERFLOW (deadline).
     * Completion, cancellation and errors after acquisition all restore the pin
     * configuration and borrowed timer state. No IRQ handler is installed. */
    int hpclang_sound_play(uint32_t frequency_hz, uint32_t duration_ms,
                           hpclang_sound_cancel_fn cancel, void *context);

    /* Foreground-only asynchronous tone. replace != 0 cancels our active tone
     * before admitting a new one; a failed replacement leaves it stopped.
     * Nonzero frequency/duration are required. Start temporarily enables owned
     * IRQ delivery and masks FIQ: do not call inside a critical section or from
     * interrupt context. Other IRQ sources stay masked. Audio stops without
     * foreground SDK calls; the reserved IRQ stack/vector is collected by
     * status, stop, a later start, or normal/_Exit cleanup. These restore the
     * CPU interrupt mask saved by the first start. Call status/stop before
     * changing interrupt policy. Starting amid foreign enabled IRQs fails
     * EBUSY. Hardware contention also fails EBUSY; clock/mapping failures are
     * EIO/ENOSYS. Zero duration is EINVAL, excessive deadlines EOVERFLOW.
     */
    int hpclang_sound_start(uint32_t frequency_hz, uint32_t duration_ms, int replace);
    /* 1 running; 0 idle/completed (collects resources); -1 clock/timer failure
     * (collects resources and sets errno). No clock polling drives completion. */
    int hpclang_sound_status(void);
    /* Cancel/collect idempotently. The shared runtime exit path also does this. */
    void hpclang_sound_stop(void);

#ifdef __cplusplus
}
#endif
#endif
