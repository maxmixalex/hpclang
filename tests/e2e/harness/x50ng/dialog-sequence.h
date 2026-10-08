#ifndef HPCLANG_DIALOG_SEQUENCE_H
#define HPCLANG_DIALOG_SEQUENCE_H

#include <stdio.h>
#include <stdlib.h>
#include "emulator_api.h"
#include "hdw.h"
#include "timer.h"
#include "event-queue.h"
#include "view-tap.h"
#include "s3c2410/s3c2410_lcd.h"

struct hpclang_dialog_event
{
    hdw_t *hdw;
    int lcd_height;
    void (*fail)(void *, const char *);
    void *context;
    int key;
    struct hpclang_view_tap tap;
    unsigned char pattern[131 * 80];
};

static int64_t hpclang_dismiss_dialog(void *arg)
{
    struct hpclang_dialog_event *event = arg;
    int nonblank = 0;
    int matches = 1;
    uint64_t frame = UINT64_C(1469598103934665603);
    for (int y = 0; y < event->lcd_height; ++y)
        for (int x = 0; x < 131; ++x) {
            unsigned char expected = event->pattern[y * 131 + x];
            if (expected == 128)
                continue;
            int level = s3c2410_get_pixel_color(event->hdw->s3c2410_lcd, x, y);
            if (level < 0 || level > 15 || expected != 255 - 17 * level)
                matches = 0;
            nonblank |= level > 0;
            frame ^= (uint8_t)level;
            frame *= UINT64_C(1099511628211);
        }
    /* Wildcard pixels cannot prove a dialog change (e.g. blinking status). */
    int step = hpclang_view_tap_step(
        &event->tap, timer_get_clock(), frame, nonblank, matches, emulator_key_ready(event->key),
        emulator_key_observed(event->key) != 0, !is_key_pressed(event->key));
    if (step == HPCLANG_VIEW_DOWN)
        press_key(event->key);
    else if (step == HPCLANG_VIEW_UP)
        release_key(event->key);
    if (step < 0) {
        release_key(event->key);
        event->fail(event->context, event->tap.failure);
    }
    if (step < 0 || step == HPCLANG_VIEW_DONE) {
        free(event);
        return step < 0 ? -1 : 0;
    }
    return 50000LL;
}

static inline int hpclang_dialog_add(struct hpclang_event_queue *queue, hdw_t *hdw, int height,
                                     int key, int64_t us, const char *pattern,
                                     void (*fail)(void *, const char *), void *context)
{
    if (height != 64 && height != 80)
        return 0;
    struct hpclang_dialog_event *event = calloc(1, sizeof(*event));
    if (!event)
        return 0;
    event->hdw = hdw;
    event->lcd_height = height;
    event->fail = fail;
    event->context = context;
    event->key = key;
    FILE *file = fopen(pattern, "rb");
    int width, pattern_height, maximum;
    size_t size = (size_t)131 * event->lcd_height;
    int valid = file && fscanf(file, "P5 %d %d %d", &width, &pattern_height, &maximum) == 3 &&
                width == 131 && pattern_height == event->lcd_height && maximum == 255 &&
                fgetc(file) == '\n' && fread(event->pattern, 1, size, file) == size &&
                fgetc(file) == EOF;
    if (file)
        fclose(file);
    if (!valid) {
        fprintf(stderr, "invalid dialog pattern: %s\n", pattern);
        free(event);
        return 0;
    }
    if (!hpclang_event_add(queue, us, hpclang_dismiss_dialog, event)) {
        free(event);
        return 0;
    }
    return 1;
}

#endif
