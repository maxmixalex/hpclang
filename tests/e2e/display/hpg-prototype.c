/* Executable compatibility feasibility witness, not an installed HPG adapter.
 * Mutable clip/color/pattern/PAINT-XOR state stays above the clean surface API.
 * The XPM subset here is one-character symbols, #RRGGBB and None; broad source
 * compatibility remains M8. No private surface layout or MMIO is used. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/display.h>

struct drawing
{
    struct hpclang_surface *surface;
    int left, top, right, bottom;
    uint8_t color;
    int xor_mode;
    const struct hpclang_surface *pattern;
    int pattern_x, pattern_y;
};

static void clip(struct drawing *g, int left, int top, int right, int bottom)
{
    if (left > g->left)
        g->left = left;
    if (top > g->top)
        g->top = top;
    if (right < g->right)
        g->right = right;
    if (bottom < g->bottom)
        g->bottom = bottom;
}
static void pixel(struct drawing *g, int x, int y, uint8_t color)
{
    if (x < g->left || y < g->top || x >= g->right || y >= g->bottom)
        return;
    if (g->xor_mode)
        color ^= hpclang_surface_get_pixel(g->surface, x, y);
    hpclang_surface_set_pixel(g->surface, x, y, color);
}
static int repeat(int coordinate, int origin, uint32_t period)
{
    int64_t offset = ((int64_t)coordinate - origin) % period;
    return (int)(offset < 0 ? offset + period : offset);
}
static void fill(struct drawing *g)
{
    for (int y = g->top; y < g->bottom; ++y)
        for (int x = g->left; x < g->right; ++x) {
            if (g->pattern &&
                !hpclang_surface_get_pixel(
                    g->pattern, repeat(x, g->pattern_x, hpclang_surface_width(g->pattern)),
                    repeat(y, g->pattern_y, hpclang_surface_height(g->pattern))))
                continue; /* HPG's pattern is an ink mask, not a background color. */
            pixel(g, x, y, g->color);
        }
}
static int hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static int number(const char **p, unsigned *out)
{
    while (**p == ' ')
        ++*p;
    unsigned n = 0, digits = 0;
    while (**p >= '0' && **p <= '9') {
        n = n * 10u + (unsigned)(*(*p)++ - '0');
        if (n > 4096u)
            return -1;
        ++digits;
    }
    if (!digits || !n)
        return -1;
    *out = n;
    return 0;
}
static int xpm(struct drawing *g, int x, int y, const char *const *lines, unsigned count)
{
    if (!lines || !count || !lines[0])
        return -1;
    const char *header = lines[0];
    unsigned width, height, colors, cpp;
    if (number(&header, &width) || number(&header, &height) || number(&header, &colors) ||
        number(&header, &cpp) || *header || cpp != 1 || colors > 256 ||
        count != 1 + colors + height)
        return -1;
    int palette[256];
    for (unsigned i = 0; i < 256; ++i)
        palette[i] = -2;
    for (unsigned i = 0; i < colors; ++i) {
        const char *line = lines[1 + i];
        if (!line || strlen(line) < 8 || line[1] != ' ' || line[2] != 'c' || line[3] != ' ')
            return -1;
        unsigned symbol = (unsigned char)line[0];
        if (palette[symbol] != -2)
            return -1;
        if (!strcmp(line + 4, "None"))
            palette[symbol] = -1;
        else {
            if (line[4] != '#' || strlen(line) != 11)
                return -1;
            unsigned rgb[3] = {0};
            for (unsigned j = 0; j < 6; ++j) {
                int digit = hex(line[5 + j]);
                if (digit < 0)
                    return -1;
                rgb[j / 2] = 16 * rgb[j / 2] + (unsigned)digit;
            }
            palette[symbol] = 255 - (int)((227 * rgb[0] + 724 * rgb[1] + 73 * rgb[2]) >> 10);
        }
    }
    /* Validate the whole input before modifying the destination. */
    for (unsigned row = 0; row < height; ++row) {
        const char *line = lines[1 + colors + row];
        if (!line || strlen(line) != width)
            return -1;
        for (unsigned col = 0; col < width; ++col)
            if (palette[(unsigned char)line[col]] == -2)
                return -1;
    }
    for (unsigned row = 0; row < height; ++row)
        for (unsigned col = 0; col < width; ++col) {
            int color = palette[(unsigned char)lines[1 + colors + row][col]];
            int64_t dx = (int64_t)x + col, dy = (int64_t)y + row;
            if (color >= 0 && dx >= g->left && dx < g->right && dy >= g->top && dy < g->bottom)
                pixel(g, (int)dx, (int)dy, (uint8_t)color);
        }
    return 0;
}

unsigned hpclang_hpg_graphics_witness(struct hpclang_surface *destination)
{
    struct hpclang_surface *surface = hpclang_surface_create(16, 12, 16);
    struct hpclang_surface *pattern = hpclang_surface_create_mono(8, 2);
    struct hpclang_surface *mask = hpclang_surface_create_mono(16, 12);
    unsigned failure = 0;
    if (!surface || !pattern || !mask) {
        failure = 1;
        goto done;
    }
    for (int x = 0; x < 8; ++x)
        hpclang_surface_set_pixel(pattern, x, x & 1, 255);
    struct drawing g = {surface, 0, 0, 16, 12, 255, 0, pattern, 0, 0};
    clip(&g, 2, 2, 14, 10);
    clip(&g, 3, 1, 15, 11);
    fill(&g);
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 16; ++x) {
            unsigned expected =
                (x >= 3 && x < 14 && y >= 2 && y < 10 && (x & 1) == (y & 1)) ? 255 : 0;
            if (hpclang_surface_get_pixel(surface, x, y) != expected) {
                failure = 2;
                goto done;
            }
        }
    hpclang_surface_clear(surface, 85);
    g.pattern_x = 1;
    g.color = 170;
    fill(&g);
    if (hpclang_surface_get_pixel(surface, 4, 4) != 85 ||
        hpclang_surface_get_pixel(surface, 4, 3) != 170) {
        failure = 7;
        goto done;
    }
    hpclang_surface_clear(surface, 0);
    g.pattern_x = 0;
    g.color = 255;
    fill(&g);
    g.pattern = NULL;
    g.color = 170;
    g.xor_mode = 1;
    fill(&g);
    fill(&g);
    if (hpclang_surface_get_pixel(surface, 4, 4) != 255 ||
        hpclang_surface_get_pixel(surface, 4, 3) != 0) {
        failure = 3;
        goto done;
    }
    /* A mask delegates line rasterization to the core, then applies clip/style. */
    hpclang_surface_draw_line(mask, 0, 0, 15, 11, 255);
    g.xor_mode = 0;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 16; ++x)
            if (hpclang_surface_get_pixel(mask, x, y))
                pixel(&g, x, y, g.color);
    if (hpclang_surface_get_pixel(surface, 0, 0) != 0 ||
        hpclang_surface_get_pixel(surface, 8, 6) != 170) {
        failure = 4;
        goto done;
    }
    static const char *const image[] = {"3 2 3 1",     ". c None", "w c #ffffff",
                                        "b c #000000", ".wb",      "bw."};
    hpclang_surface_clear(surface, 85);
    if (xpm(&g, 3, 3, image, 6) || hpclang_surface_get_pixel(surface, 3, 3) != 85 ||
        hpclang_surface_get_pixel(surface, 4, 3) != 0 ||
        hpclang_surface_get_pixel(surface, 5, 3) != 255 ||
        hpclang_surface_get_pixel(surface, 5, 4) != 85) {
        failure = 5;
        goto done;
    }
    static const char *const invalid[] = {"1 1 1 1", "x c #000000", "?"};
    if (xpm(&g, 3, 3, invalid, 3) != -1 || hpclang_surface_get_pixel(surface, 3, 3) != 85) {
        failure = 6;
        goto done;
    }
    if (destination)
        hpclang_surface_blit(destination, 2, 2, surface, 0, 0, 16, 12);
done:
    hpclang_surface_destroy(mask);
    hpclang_surface_destroy(pattern);
    hpclang_surface_destroy(surface);
    return failure;
}
