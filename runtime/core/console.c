#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/console.h>
#include <hpclang/display.h>
#include <hpclang/font.h>

struct hpclang_console
{
    unsigned columns, rows, column, row, width, height;
    unsigned remaining, tab_width;
    uint32_t pending, minimum;
    const struct hpclang_font *font;
    struct hpclang_surface *canvas, *glyph;
    uint32_t cells[];
};

struct hpclang_console *hpclang_console_create(unsigned columns, unsigned rows,
                                               const struct hpclang_font *font)
{
    unsigned width = hpclang_font_advance(font), height = hpclang_font_height(font);
    if (!font || !columns || !rows || !width || !height || columns > INT32_MAX / width ||
        rows > INT32_MAX / height ||
        (size_t)columns > (SIZE_MAX - sizeof(struct hpclang_console)) / sizeof(uint32_t) / rows) {
        errno = EINVAL;
        return NULL;
    }
    struct hpclang_console *c = calloc(1, sizeof(*c) + (size_t)columns * rows * sizeof(uint32_t));
    if (!c) {
        errno = ENOMEM;
        return NULL;
    }
    c->columns = columns;
    c->rows = rows;
    c->width = width;
    c->height = height;
    c->font = font;
    c->tab_width = 4;
    c->canvas = hpclang_surface_create_mono(columns * width, rows * height);
    c->glyph = hpclang_surface_create_mono(width, height);
    if (!c->canvas || !c->glyph) {
        hpclang_console_destroy(c);
        errno = ENOMEM;
        return NULL;
    }
    hpclang_console_clear(c);
    return c;
}

void hpclang_console_destroy(struct hpclang_console *c)
{
    if (!c)
        return;
    hpclang_surface_destroy(c->glyph);
    hpclang_surface_destroy(c->canvas);
    free(c);
}

int hpclang_console_clear(struct hpclang_console *c)
{
    if (!c) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < (size_t)c->columns * c->rows; ++i)
        c->cells[i] = ' ';
    c->column = c->row = c->remaining = 0;
    return 0;
}

static void newline(struct hpclang_console *c)
{
    c->column = 0;
    if (++c->row < c->rows)
        return;
    c->row = c->rows - 1;
    size_t retained = (size_t)c->columns * c->row;
    memmove(c->cells, c->cells + c->columns, retained * sizeof(uint32_t));
    for (unsigned i = 0; i < c->columns; ++i)
        c->cells[retained + i] = ' ';
}

int hpclang_console_get_state(const struct hpclang_console *c, struct hpclang_console_state *state)
{
    if (!c || !state) {
        errno = EINVAL;
        return -1;
    }
    *state = (struct hpclang_console_state){c->columns, c->rows, c->column, c->row, c->tab_width};
    return 0;
}

int hpclang_console_move(struct hpclang_console *c, unsigned column, unsigned row)
{
    if (!c || column > c->columns || row >= c->rows) {
        errno = EINVAL;
        return -1;
    }
    hpclang_console_flush(c);
    c->column = column;
    c->row = row;
    return 0;
}

int hpclang_console_clear_eol(struct hpclang_console *c)
{
    if (hpclang_console_flush(c))
        return -1;
    for (unsigned x = c->column; x < c->columns; ++x)
        c->cells[(size_t)c->row * c->columns + x] = ' ';
    return 0;
}

int hpclang_console_set_tab_width(struct hpclang_console *c, unsigned width)
{
    if (!c || !width) {
        errno = EINVAL;
        return -1;
    }
    c->tab_width = width;
    return 0;
}

static void put(struct hpclang_console *c, uint32_t cp)
{
    if (cp == '\n') {
        newline(c);
    } else if (cp == '\r') {
        c->column = 0;
    } else if (cp == '\b') {
        if (c->column)
            --c->column;
    } else if (cp == '\f') {
        hpclang_console_clear(c);
    } else if (cp == '\t') {
        if (c->column == c->columns)
            newline(c);
        unsigned spaces = c->tab_width - c->column % c->tab_width;
        if ((uint64_t)spaces > (uint64_t)c->columns * c->rows) {
            unsigned column = (unsigned)(((uint64_t)c->column + spaces - 1) % c->columns) + 1;
            hpclang_console_clear(c);
            c->row = c->rows - 1;
            c->column = column;
            return;
        }
        while (spaces--)
            put(c, ' ');
    } else if (cp >= 0x20 && !(cp >= 0x7f && cp <= 0x9f)) {
        if (c->column == c->columns)
            newline(c);
        c->cells[(size_t)c->row * c->columns + c->column++] = cp;
    }
}

static void byte(struct hpclang_console *c, unsigned char b)
{
    if (c->remaining) {
        if ((b & 0xc0) == 0x80) {
            c->pending = (c->pending << 6) | (b & 0x3f);
            if (--c->remaining == 0) {
                uint32_t cp = c->pending;
                put(c, cp < c->minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ? 0xfffd
                                                                                          : cp);
            }
            return;
        }
        c->remaining = 0;
        put(c, 0xfffd);
    }
    if (b < 0x80) {
        put(c, b);
    } else if (b >= 0xc2 && b <= 0xf4) {
        c->remaining = b < 0xe0 ? 1 : b < 0xf0 ? 2 : 3;
        c->minimum = b < 0xe0 ? 0x80 : b < 0xf0 ? 0x800 : 0x10000;
        c->pending = b & (b < 0xe0 ? 0x1f : b < 0xf0 ? 0x0f : 0x07);
    } else {
        put(c, 0xfffd);
    }
}

int hpclang_console_write(struct hpclang_console *c, const char *bytes, size_t length)
{
    if (!c || (!bytes && length)) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < length; ++i)
        byte(c, (unsigned char)bytes[i]);
    return 0;
}

int hpclang_console_flush(struct hpclang_console *c)
{
    if (!c) {
        errno = EINVAL;
        return -1;
    }
    if (c->remaining) {
        c->remaining = 0;
        put(c, 0xfffd);
    }
    return 0;
}

int hpclang_console_cell(const struct hpclang_console *c, unsigned column, unsigned row,
                         uint32_t *codepoint)
{
    if (!c || !codepoint || column >= c->columns || row >= c->rows) {
        errno = EINVAL;
        return -1;
    }
    *codepoint = c->cells[(size_t)row * c->columns + column];
    return 0;
}

int hpclang_console_render(struct hpclang_console *c, struct hpclang_surface *destination,
                           int32_t x, int32_t y)
{
    if (!c || !destination) {
        errno = EINVAL;
        return -1;
    }
    hpclang_surface_clear(c->canvas, 0);
    for (unsigned row = 0; row < c->rows; ++row)
        for (unsigned col = 0; col < c->columns; ++col) {
            uint32_t cp = c->cells[(size_t)row * c->columns + col];
            if (cp == ' ')
                continue;
            hpclang_surface_clear(c->glyph, 0);
            hpclang_surface_draw_codepoint(c->glyph, c->font, 0, 0, cp, 255);
            hpclang_surface_blit(c->canvas, (int32_t)(col * c->width), (int32_t)(row * c->height),
                                 c->glyph, 0, 0, (int32_t)c->width, (int32_t)c->height);
        }
    hpclang_surface_blit(destination, x, y, c->canvas, 0, 0, (int32_t)(c->columns * c->width),
                         (int32_t)(c->rows * c->height));
    return 0;
}

int hpclang_console_stdio_write(void *context, unsigned stream, unsigned char b)
{
    (void)stream;
    char data = (char)b;
    return hpclang_console_write(context, &data, 1);
}

int hpclang_console_stdio_flush(void *context, unsigned stream)
{
    (void)stream;
    return hpclang_console_flush(context);
}
