#ifndef HPCLANG_E2E_LCD_QUIESCENCE_H
#define HPCLANG_E2E_LCD_QUIESCENCE_H

#include <stdint.h>

#include "hdw.h"
#include "s3c2410/s3c2410_lcd.h"

#include "lcd-stability.h"

static inline uint64_t hpclang_lcd_measure(hdw_t *hdw, int width, int height, int *nonblank)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    *nonblank = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int level = s3c2410_get_pixel_color(hdw->s3c2410_lcd, x, y);
            if (level < 0)
                level = 0;
            if (level > 15)
                level = 15;
            *nonblank |= level != 0;
            hash ^= (uint8_t)level;
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static inline uint64_t hpclang_lcd_fingerprint(hdw_t *hdw, int width, int height)
{
    int nonblank;
    return hpclang_lcd_measure(hdw, width, height, &nonblank);
}

static inline void hpclang_lcd_quiescence_seed(struct hpclang_lcd_quiescence *state, hdw_t *hdw,
                                               int width, int height)
{
    state->baseline = hpclang_lcd_fingerprint(hdw, width, height);
    state->current = state->baseline;
    state->stable_observations = 0;
    state->observations = 0;
    state->baseline_set = 1;
    state->current_set = 1;
    state->changed_from_baseline = 0;
}

static inline int hpclang_lcd_quiescence_observe(struct hpclang_lcd_quiescence *state, hdw_t *hdw,
                                                 int width, int height,
                                                 unsigned required_stable_observations)
{
    int nonblank;
    uint64_t fingerprint = hpclang_lcd_measure(hdw, width, height, &nonblank);
    return hpclang_lcd_observe_sample(state, fingerprint, nonblank, required_stable_observations);
}

#endif
