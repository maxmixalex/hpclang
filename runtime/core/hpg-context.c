#include "hpg-internal.h"
#include <errno.h>
#include <stdlib.h>

static struct hpg_graphics screen;
hpg_t *hpg_stdscreen = &screen;
static int initialized;

static void initialize(hpg_t *g, struct hpclang_surface *surface)
{
    *g = (hpg_t){.surface = surface,
                 .width = (int)hpclang_surface_width(surface),
                 .height = (int)hpclang_surface_height(surface),
                 .color = HPG_COLOR_BLACK};
    g->right = g->width;
    g->bottom = g->height;
}
void hpg_init(void)
{
    if (initialized)
        return;
    struct hpclang_surface *surface = hpclang_display_surface();
    if (!surface) {
        errno = ENODEV;
        return;
    }
    initialize(&screen, surface);
    initialized = 1;
}
void hpg_cleanup(void)
{
    screen = (hpg_t){0};
    hpg_stdscreen = &screen;
    initialized = 0;
}
hpg_t *hpclang_hpg_resolve(hpg_t *g)
{
    if (!g)
        g = hpg_stdscreen;
    if (g && g != &screen)
        return g;
    hpg_init();
    return initialized ? &screen : NULL;
}
struct hpclang_surface *hpclang_hpg_surface(hpg_t *g)
{
    return g == &screen ? hpclang_display_surface() : g->surface;
}
static hpg_t *image(int width, int height, unsigned levels)
{
    if (width <= 0 || height <= 0) {
        errno = EINVAL;
        return NULL;
    }
    hpg_t *g = malloc(sizeof(*g));
    if (!g) {
        errno = ENOMEM;
        return NULL;
    }
    struct hpclang_surface *surface =
        hpclang_surface_create((unsigned)width, (unsigned)height, levels);
    if (!surface) {
        free(g);
        errno = ENOMEM;
        return NULL;
    }
    initialize(g, surface);
    return g;
}
hpg_t *hpg_alloc_mono_image(int w, int h)
{
    return image(w, h, 2);
}
hpg_t *hpg_alloc_gray4_image(int w, int h)
{
    return image(w, h, 4);
}
hpg_t *hpg_alloc_gray16_image(int w, int h)
{
    return image(w, h, 16);
}
void hpg_free_image(hpg_t *g)
{
    if (!g)
        return;
    if (g == &screen) {
        errno = EINVAL;
        return;
    }
    hpclang_surface_destroy(g->surface);
    free(g);
}
int hpg_get_width(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->width : 0;
}
int hpg_get_height(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->height : 0;
}
void hpg_clear_on(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    if (g)
        hpclang_surface_clear(hpclang_hpg_surface(g), 0);
}
void hpg_clear(void)
{
    hpg_clear_on(NULL);
}
unsigned char hpg_get_pixel(hpg_t *g, int x, int y)
{
    g = hpclang_hpg_resolve(g);
    return g ? hpclang_surface_get_pixel(hpclang_hpg_surface(g), x, y) : 0;
}
unsigned char hpg_get_color(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->color : 0;
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    g = hpclang_hpg_resolve(g);
    if (g)
        g->color = color;
}
unsigned char hpg_get_mode(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->mode : 0;
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    if (mode != HPG_MODE_PAINT && mode != HPG_MODE_XOR) {
        errno = EINVAL;
        return;
    }
    g = hpclang_hpg_resolve(g);
    if (g)
        g->mode = mode;
}
void hpg_clip_reset(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    g->left = g->top = 0;
    g->right = g->width;
    g->bottom = g->height;
}
void hpg_clip_set(hpg_t *g, int x1, int y1, int x2, int y2)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    g->left = x1 < 0 ? 0 : x1;
    g->top = y1 < 0 ? 0 : y1;
    /* Preserve HPG's historical setter boundary; reset alone includes the last
     * row/column. Drawing treats right/bottom as exclusive in all four versions. */
    g->right = x2 >= g->width ? g->width - 1 : x2;
    g->bottom = y2 >= g->height ? g->height - 1 : y2;
}
void hpg_clip(hpg_t *g, int x1, int y1, int x2, int y2)
{
    g = hpclang_hpg_resolve(g);
    if (!g)
        return;
    if (x1 > g->left)
        g->left = x1;
    if (y1 > g->top)
        g->top = y1;
    if (x2 < g->right)
        g->right = x2;
    if (y2 < g->bottom)
        g->bottom = y2;
}
hpg_pattern_t *hpg_alloc_pattern(char *buffer, int height, int fixed)
{
    if (!buffer || height <= 0 || height > 255 || (fixed != 0 && fixed != 1)) {
        errno = EINVAL;
        return NULL;
    }
    hpg_pattern_t *p = malloc(sizeof(*p));
    if (!p) {
        errno = ENOMEM;
        return NULL;
    }
    *p = (hpg_pattern_t){(const unsigned char *)buffer, (unsigned)height, fixed};
    return p;
}
void hpg_free_pattern(hpg_pattern_t *p)
{
    free(p);
}
hpg_pattern_t *hpg_get_pattern(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->pattern : NULL;
}
void hpg_set_pattern(hpg_t *g, hpg_pattern_t *p)
{
    g = hpclang_hpg_resolve(g);
    if (g)
        g->pattern = p;
}
hpg_font_t *hpg_alloc_font(char *buffer, int count, int height, int advance)
{
    if (!buffer || count <= 0 || count > 256 || height <= 0 || height > 255 || advance <= 0 ||
        advance > 255) {
        errno = EINVAL;
        return NULL;
    }
    hpg_font_t *f = malloc(sizeof(*f));
    if (!f) {
        errno = ENOMEM;
        return NULL;
    }
    *f = (hpg_font_t){(const unsigned char *)buffer, (unsigned)count, (unsigned)height,
                      (unsigned)advance, 0};
    return f;
}
void hpg_free_font(hpg_font_t *f)
{
    if (f && !f->borrowed_descriptor)
        free(f);
}
hpg_font_t *hpg_get_font(hpg_t *g)
{
    g = hpclang_hpg_resolve(g);
    return g ? g->font : NULL;
}
void hpg_set_font(hpg_t *g, hpg_font_t *f)
{
    g = hpclang_hpg_resolve(g);
    if (g)
        g->font = f;
}
int hpg_font_get_height(hpg_font_t *f)
{
    if (!f) {
        errno = EINVAL;
        return 0;
    }
    return (int)f->height;
}
int hpg_font_get_advance(hpg_font_t *f)
{
    if (!f) {
        errno = EINVAL;
        return 0;
    }
    return (int)f->advance;
}
