#ifndef HPCLANG_DISPLAY_SURFACE_H
#define HPCLANG_DISPLAY_SURFACE_H

#include <stdint.h>

struct hpclang_surface {
    volatile uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t levels;
};

#endif
