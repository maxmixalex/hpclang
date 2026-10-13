#ifndef HPCLANG_HPG_INTERNAL_H
#define HPCLANG_HPG_INTERNAL_H
#include <stddef.h>
#include <hpgraphics.h>
#include <hpclang/display.h>

struct hpg_font
{
    const unsigned char *buffer;
    unsigned count, height, advance;
    int borrowed_descriptor;
};
struct hpg_pattern
{
    const unsigned char *buffer;
    unsigned height;
    int fixed;
};
struct hpg_graphics
{
    struct hpclang_surface *surface;
    int width, height, left, top, right, bottom;
    unsigned char color, mode;
    struct hpg_pattern *pattern;
    struct hpg_font *font;
};
struct hpg_graphics *hpclang_hpg_resolve(struct hpg_graphics *g);
struct hpclang_surface *hpclang_hpg_surface(struct hpg_graphics *g);
#endif
