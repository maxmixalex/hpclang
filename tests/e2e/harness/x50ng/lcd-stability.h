#ifndef HPCLANG_LCD_STABILITY_H
#define HPCLANG_LCD_STABILITY_H
#include <stdint.h>

struct hpclang_lcd_quiescence
{
    uint64_t baseline;
    uint64_t current;
    unsigned stable_observations;
    unsigned observations;
    int baseline_set;
    int current_set;
    int changed_from_baseline;
};

/* Blank repaint intervals are observations, never successful return evidence. */
static inline int hpclang_lcd_observe_sample(struct hpclang_lcd_quiescence *state,
                                             uint64_t fingerprint, int nonblank,
                                             unsigned required_stable_observations)
{
    ++state->observations;
    if (!state->baseline_set) {
        state->baseline = fingerprint;
        state->baseline_set = 1;
    }
    if (fingerprint != state->baseline)
        state->changed_from_baseline = 1;

    if (!state->current_set || fingerprint != state->current) {
        state->current = fingerprint;
        state->current_set = 1;
        state->stable_observations = 1;
    } else {
        ++state->stable_observations;
    }

    if (!nonblank)
        state->stable_observations = 0;

    return nonblank && state->changed_from_baseline &&
           state->stable_observations >= required_stable_observations;
}
#endif
