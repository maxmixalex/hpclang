#include "hpg-internal.h"
#include <stdint.h>

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
            const hpg_pattern_t *pattern = g->pattern;
            if (pattern) {
                int64_t row = ((int64_t)y - (pattern->fixed ? 0 : y1)) % pattern->height;
                int64_t col = ((int64_t)x - (pattern->fixed ? 0 : x1)) % 8;
                if (row < 0)
                    row += pattern->height;
                if (col < 0)
                    col += 8;
                if (!(pattern->buffer[row] & (1u << col)))
                    continue;
            }
            pixel(g, surface, x, y);
        }
}
void hpg_fill_rect(int x1, int y1, int x2, int y2)
{
    hpg_fill_rect_on(NULL, x1, y1, x2, y2);
}
