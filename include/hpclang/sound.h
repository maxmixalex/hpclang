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

#ifdef __cplusplus
}
#endif
#endif
