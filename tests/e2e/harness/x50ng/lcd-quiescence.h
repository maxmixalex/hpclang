#ifndef HPCLANG_E2E_LCD_QUIESCENCE_H
#define HPCLANG_E2E_LCD_QUIESCENCE_H

#include <stdint.h>

#include "hdw.h"
#include "s3c2410/s3c2410_lcd.h"

struct hpclang_lcd_quiescence {
    uint64_t baseline;
    uint64_t current;
    unsigned stable_observations;
    unsigned observations;
    int baseline_set;
    int current_set;
    int changed_from_baseline;
};

static inline uint64_t hpclang_lcd_fingerprint(
    hdw_t *hdw, int width, int height)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int level = s3c2410_get_pixel_color(hdw->s3c2410_lcd, x, y);
            if (level < 0) level = 0;
            if (level > 15) level = 15;
            hash ^= (uint8_t)level;
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static inline void hpclang_lcd_quiescence_seed(
    struct hpclang_lcd_quiescence *state,
    hdw_t *hdw,
    int width,
    int height)
{
    state->baseline = hpclang_lcd_fingerprint(hdw, width, height);
    state->current = state->baseline;
    state->stable_observations = 0;
    state->observations = 0;
    state->baseline_set = 1;
    state->current_set = 1;
    state->changed_from_baseline = 0;
}

static inline int hpclang_lcd_quiescence_observe(
    struct hpclang_lcd_quiescence *state,
    hdw_t *hdw,
    int width,
    int height,
    unsigned required_stable_observations)
{
    uint64_t fingerprint = hpclang_lcd_fingerprint(hdw, width, height);

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

    return state->changed_from_baseline &&
           state->stable_observations >= required_stable_observations;
}

#endif
