#include "hpg-internal.h"
#include <stdint.h>
#include <errno.h>

static int ink(const hpg_pattern_t *pattern, int x, int y, int64_t origin_x, int64_t origin_y)
{
    if (!pattern)
        return 1;
    int64_t row = ((int64_t)y - (pattern->fixed ? 0 : origin_y)) % pattern->height;
    int64_t col = ((int64_t)x - (pattern->fixed ? 0 : origin_x)) % 8;
    if (row < 0)
        row += pattern->height;
    if (col < 0)
        col += 8;
    return !!(pattern->buffer[row] & (1u << col));
}

static void pixel(hpg_t *g, struct hpclang_surface *surface, int x, int y)
{
    unsigned char color = g->color;
    if (g->mode == HPG_MODE_XOR)
        color ^= hpclang_surface_get_pixel(surface, x, y);
    hpclang_surface_set_pixel(surface, x, y, color);
}
void hpg_draw_pixel_on(hpg_t *g, int x, int y)
{
    g = hpclang_hpg_resolve(g);
    if (!g || x < g->left || x >= g->right || y < g->top || y >= g->bottom)
        return;
    pixel(g, hpclang_hpg_surface(g), x, y);
}
void hpg_draw_pixel(int x, int y)
{
    hpg_draw_pixel_on(NULL, x, y);
}
void hpg_fill_rect_on(hpg_t *g, int x1, int y1, int x2, int y2)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    if (x1 > x2) {
        int saved = x1;
        x1 = x2;
        x2 = saved;
    }
    int64_t right = (int64_t)x2 + 1, bottom = (int64_t)y2 + 1;
    int left = x1 > g->left ? x1 : g->left;
    int top = y1 > g->top ? y1 : g->top;
    if (right > g->right)
        right = g->right;
    if (bottom > g->bottom)
        bottom = g->bottom;
    struct hpclang_surface *surface = hpclang_hpg_surface(g);
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x) {
            if (!ink(g->pattern, x, y, x1, y1))
                continue;
            pixel(g, surface, x, y);
        }
}
void hpg_fill_rect(int x1, int y1, int x2, int y2)
{
    hpg_fill_rect_on(NULL, x1, y1, x2, y2);
}

/* Reuse one bounded monochrome coverage plane for shapes. The clean renderer
 * handles extreme-coordinate clipping; compositing applies legacy color/XOR
 * exactly once per covered pixel, including shared corners and circle axes. */
static struct hpclang_surface *mask(hpg_t *g)
{
    if (!g->mask)
        g->mask = hpclang_surface_create_mono((unsigned)g->width, (unsigned)g->height);
    if (!g->mask) {
        errno = ENOMEM;
        return NULL;
    }
    hpclang_surface_clear(g->mask, 0);
    return g->mask;
}
static void composite(hpg_t *g, int patterned, int64_t origin_x, int64_t origin_y)
{
    struct hpclang_surface *surface = hpclang_hpg_surface(g);
    for (int y = g->top; y < g->bottom; ++y)
        for (int x = g->left; x < g->right; ++x)
            if (hpclang_surface_get_pixel(g->mask, x, y) &&
                (!patterned || ink(g->pattern, x, y, origin_x, origin_y)))
                pixel(g, surface, x, y);
}
void hpg_draw_line_on(hpg_t *g, int x1, int y1, int x2, int y2)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    struct hpclang_surface *coverage = mask(g);
    if (!coverage)
        return;
    hpclang_surface_draw_line(coverage, x1, y1, x2, y2, 255);
    composite(g, 0, 0, 0);
}
void hpg_draw_line(int x1, int y1, int x2, int y2)
{
    hpg_draw_line_on(NULL, x1, y1, x2, y2);
}
void hpg_draw_rect_on(hpg_t *g, int x1, int y1, int x2, int y2)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    struct hpclang_surface *coverage = mask(g);
    if (!coverage)
        return;
    hpclang_surface_draw_line(coverage, x1, y1, x2, y1, 255);
    hpclang_surface_draw_line(coverage, x2, y1, x2, y2, 255);
    hpclang_surface_draw_line(coverage, x2, y2, x1, y2, 255);
    hpclang_surface_draw_line(coverage, x1, y2, x1, y1, 255);
    composite(g, 0, 0, 0);
}
void hpg_draw_rect(int x1, int y1, int x2, int y2)
{
    hpg_draw_rect_on(NULL, x1, y1, x2, y2);
}
static void circle(hpg_t *g, int cx, int cy, int radius, int filled)
{
    g = hpclang_hpg_resolve(g);
    if (!g || radius < 0)
        return;
    struct hpclang_surface *coverage = mask(g);
    if (!coverage)
        return;
    if (filled)
        hpclang_surface_fill_circle(coverage, cx, cy, radius, 255);
    else
        hpclang_surface_draw_circle(coverage, cx, cy, radius, 255);
    composite(g, filled, (int64_t)cx - radius, (int64_t)cy - radius);
}
void hpg_draw_circle_on(hpg_t *g, int cx, int cy, int radius)
{
    circle(g, cx, cy, radius, 0);
}
void hpg_draw_circle(int cx, int cy, int radius)
{
    circle(NULL, cx, cy, radius, 0);
}
void hpg_fill_circle_on(hpg_t *g, int cx, int cy, int radius)
{
    circle(g, cx, cy, radius, 1);
}
void hpg_fill_circle(int cx, int cy, int radius)
{
    circle(NULL, cx, cy, radius, 1);
}
