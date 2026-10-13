#include "hpg-internal.h"
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

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
    hpclang_hpg_finish(g);
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
    hpclang_hpg_finish(g);
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
    hpclang_hpg_finish(g);
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

static void polygon(hpg_t *g, int vx[], int vy[], int len, int filled)
{
    if (len < 0 || (len && (!vx || !vy))) {
        errno = EINVAL;
        return;
    }
    if (!len)
        return;
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    if ((size_t)len > SIZE_MAX / sizeof(struct hpclang_point)) {
        errno = EOVERFLOW;
        return;
    }
    struct hpclang_point *points = malloc((size_t)len * sizeof(*points));
    if (!points) {
        errno = ENOMEM;
        return;
    }
    int left = vx[0], top = vy[0];
    for (int i = 0; i < len; ++i) {
        points[i] = (struct hpclang_point){vx[i], vy[i]};
        if (vx[i] < left)
            left = vx[i];
        if (vy[i] < top)
            top = vy[i];
    }
    struct hpclang_surface *coverage = mask(g);
    if (coverage) {
        if (filled)
            hpclang_surface_fill_polygon(coverage, points, (unsigned)len, 255);
        else
            hpclang_surface_draw_polygon(coverage, points, (unsigned)len, 255);
        composite(g, filled, left, top);
    }
    free(points);
}
void hpg_draw_polygon_on(hpg_t *g, int vx[], int vy[], int len)
{
    polygon(g, vx, vy, len, 0);
}
void hpg_draw_polygon(int vx[], int vy[], int len)
{
    polygon(NULL, vx, vy, len, 0);
}
void hpg_fill_polygon_on(hpg_t *g, int vx[], int vy[], int len)
{
    polygon(g, vx, vy, len, 1);
}
void hpg_fill_polygon(int vx[], int vy[], int len)
{
    polygon(NULL, vx, vy, len, 1);
}

void hpg_blit(hpg_t *src, int sx, int sy, int width, int height, hpg_t *dst, int dx, int dy)
{
    if (width < 0 || height < 0) {
        errno = EINVAL;
        return;
    }
    if (!width || !height)
        return;
    src = hpclang_hpg_resolve(src);
    dst = hpclang_hpg_resolve(dst);
    if (!src || !dst)
        return;
    int64_t left = 0, top = 0, right = width, bottom = height;
    if (left < -(int64_t)sx)
        left = -(int64_t)sx;
    if (top < -(int64_t)sy)
        top = -(int64_t)sy;
    if (left < (int64_t)dst->left - dx)
        left = (int64_t)dst->left - dx;
    if (top < (int64_t)dst->top - dy)
        top = (int64_t)dst->top - dy;
    if (right > (int64_t)src->width - sx)
        right = (int64_t)src->width - sx;
    if (bottom > (int64_t)src->height - sy)
        bottom = (int64_t)src->height - sy;
    if (right > (int64_t)dst->right - dx)
        right = (int64_t)dst->right - dx;
    if (bottom > (int64_t)dst->bottom - dy)
        bottom = (int64_t)dst->bottom - dy;
    if (left >= right || top >= bottom)
        return;
    struct hpclang_surface *from = hpclang_hpg_surface(src), *to = hpclang_hpg_surface(dst);
    int reverse_x = from == to && dx > sx, reverse_y = from == to && dy > sy;
    for (int64_t row = 0; row < bottom - top; ++row) {
        int64_t y = reverse_y ? bottom - 1 - row : top + row;
        for (int64_t col = 0; col < right - left; ++col) {
            int64_t x = reverse_x ? right - 1 - col : left + col;
            unsigned char color = hpclang_surface_get_pixel(from, (int)(sx + x), (int)(sy + y));
            if (dst->mode == HPG_MODE_XOR)
                color ^= hpclang_surface_get_pixel(to, (int)(dx + x), (int)(dy + y));
            hpclang_surface_set_pixel(to, (int)(dx + x), (int)(dy + y), color);
        }
    }
    hpclang_hpg_finish(dst);
}

static void letter(hpg_t *g, unsigned char code, int64_t x, int64_t y)
{
    const hpg_font_t *font = g->font;
    if (code >= font->count)
        return;
    int64_t left = x > g->left ? x : g->left, top = y > g->top ? y : g->top;
    int64_t right = x + font->advance, bottom = y + font->height;
    if (right > g->right)
        right = g->right;
    if (bottom > g->bottom)
        bottom = g->bottom;
    if (left >= right || top >= bottom)
        return;
    if (!font->buffer) {
        struct hpclang_surface *coverage = mask(g);
        if (!coverage)
            return;
        hpclang_hpg_builtin_glyph(coverage, g->font, code, (int)x, (int)y);
        struct hpclang_surface *surface = hpclang_hpg_surface(g);
        for (int64_t row = top; row < bottom; ++row)
            for (int64_t col = left; col < right; ++col)
                if (hpclang_surface_get_pixel(coverage, (int)col, (int)row))
                    pixel(g, surface, (int)col, (int)row);
        return;
    }
    struct hpclang_surface *surface = hpclang_hpg_surface(g);
    for (int64_t row = top; row < bottom; ++row) {
        unsigned char bits = font->buffer[(size_t)code * font->height + (size_t)(row - y)];
        for (int64_t col = left; col < right; ++col)
            if (bits & (1u << ((col - x) % 8)))
                pixel(g, surface, (int)col, (int)row);
    }
}
void hpg_draw_letter_on(hpg_t *g, char code, int x, int y)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    if (!g->font)
        g->font = hpg_get_minifont();
    letter(g, (unsigned char)code, x, y);
    hpclang_hpg_finish(g);
}
void hpg_draw_letter(char code, int x, int y)
{
    hpg_draw_letter_on(NULL, code, x, y);
}
void hpg_draw_text_on(hpg_t *g, char *text, int x, int y)
{
    if (!text) {
        errno = EINVAL;
        return;
    }
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    if (!g->font)
        g->font = hpg_get_minifont();
    int64_t cursor_x = x, cursor_y = y;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            if (cursor_y <= INT_MAX)
                cursor_y += g->font->height;
        } else if (*p < g->font->count) {
            letter(g, *p, cursor_x, cursor_y);
            if (cursor_x <= INT_MAX)
                cursor_x += g->font->advance;
        }
    }
    hpclang_hpg_finish(g);
}
void hpg_draw_text(char *text, int x, int y)
{
    hpg_draw_text_on(NULL, text, x, y);
}
