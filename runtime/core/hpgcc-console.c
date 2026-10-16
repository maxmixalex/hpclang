#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpclang/console.h>
#include <hpclang/sound.h>
#include <hpclang/stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "console-internal.h"
#include "hpg-internal.h"
#include "stdio-routing.h"

static struct hpclang_console *console;
static hpg_t *screen;
static int enabled, cleanup_registered;

static void cleanup(void)
{
    hpclang_console_destroy(console);
    console = NULL;
}
static void draw(struct hpclang_surface *cell, const void *context, uint32_t code)
{
    (void)context;
    hpclang_hpg_builtin_glyph(cell, hpg_get_minifont(), (unsigned char)code, 0, 0);
}
static struct hpclang_surface *prepare(void)
{
    hpg_t *g = hpclang_hpg_acquire_screen();
    if (!g)
        return NULL;
    screen = g;
    if (!console) {
        if (!cleanup_registered) {
            if (atexit(cleanup)) {
                errno = ENOMEM;
                return NULL;
            }
            cleanup_registered = 1;
        }
        console = hpclang_console_create_cells((unsigned)g->width / 4u, (unsigned)g->height / 6u, 4,
                                               6, draw, NULL);
        if (!console)
            return NULL;
    }
    return hpclang_hpg_surface(g);
}
static struct hpclang_console_state state(void)
{
    struct hpclang_console_state result = {0};
    (void)hpclang_console_get_state(console, &result);
    return result;
}
static void newline(struct hpclang_surface *s)
{
    struct hpclang_console_state before = state();
    (void)hpclang_console_write_byte(console, '\n');
    if (before.row + 1 == before.rows) {
        int width = (int)before.columns * 4, height = (int)before.rows * 6;
        hpclang_surface_blit(s, 0, 0, s, 0, 6, width, height - 6);
        hpclang_surface_fill_rect(s, 0, height - 6, width, 6, 0);
    }
}
static void printable(struct hpclang_surface *s, unsigned char byte)
{
    struct hpclang_console_state before = state();
    (void)hpclang_console_write_byte(console, byte);
    (void)hpclang_console_render_cell(console, s, before.column, before.row);
    /* Historical output wraps immediately, including at the final cell. */
    if (before.column + 1 == before.columns)
        newline(s);
}
static void erase_line(struct hpclang_surface *s)
{
    struct hpclang_console_state before = state();
    (void)hpclang_console_clear_eol(console);
    hpclang_surface_fill_rect(s, (int)before.column * 4, (int)before.row * 6,
                              (int)(before.columns - before.column) * 4, 6, 0);
}
static int write_byte(void *context, unsigned stream, unsigned char byte)
{
    (void)context;
    (void)stream;
    struct hpclang_surface *s = prepare();
    if (!s)
        return -1;
    if (byte >= 0x20) {
        printable(s, byte);
    } else if (byte == '\n') {
        newline(s);
    } else if (byte == '\t') {
        struct hpclang_console_state before = state();
        unsigned spaces = before.tab_width - before.column % before.tab_width;
        if ((uint64_t)spaces > (uint64_t)before.columns * before.rows) {
            (void)hpclang_console_write_byte(console, byte);
            struct hpclang_console_state after = state();
            hpclang_surface_fill_rect(s, 0, 0, (int)after.columns * 4, (int)after.rows * 6, 0);
            if (after.column == after.columns)
                newline(s);
        } else {
            while (spaces--)
                printable(s, ' ');
        }
    } else if (byte == '\f') {
        (void)hpclang_console_clear(console);
        hpclang_surface_clear(s, 0);
    } else if (byte == 5) {
        erase_line(s);
    } else if (byte == 7) {
        if (hpclang_sound_play(880, 100, NULL, NULL) < 0)
            return -1;
    } else {
        (void)hpclang_console_write_byte(console, byte);
    }
    hpclang_hpg_finish(screen);
    return 0;
}
int hpclang_hpgcc_console_enable(void)
{
    if (enabled)
        return 0;
    const struct hpclang_stdio_callbacks callbacks = {.write = write_byte,
                                                      .read = hpclang_hpgcc_console_read};
    if (hpclang_stdio_set_default(&callbacks))
        return -1;
    enabled = 1;
    return 0;
}
void home(void)
{
    gotoxy(0, 0);
}
void gotoxy(int column, int row)
{
    if (column < 0 || row < 0) {
        errno = EINVAL;
        return;
    }
    if (!prepare())
        return;
    struct hpclang_console_state before = state();
    if ((unsigned)column >= before.columns || (unsigned)row >= before.rows) {
        errno = EINVAL;
        return;
    }
    (void)hpclang_console_move(console, (unsigned)column, (unsigned)row);
}
int getx(void)
{
    return prepare() ? (int)state().column : -1;
}
int gety(void)
{
    return prepare() ? (int)state().row : -1;
}
int screen_width(void)
{
    return prepare() ? (int)state().columns : -1;
}
int screen_height(void)
{
    return prepare() ? (int)state().rows : -1;
}
int get_tabsize(void)
{
    return prepare() ? (int)state().tab_width : -1;
}
void set_tabsize(int width)
{
    if (width <= 0) {
        errno = EINVAL;
        return;
    }
    if (prepare())
        (void)hpclang_console_set_tab_width(console, (unsigned)width);
}
void clreol(void)
{
    struct hpclang_surface *s = prepare();
    if (s) {
        erase_line(s);
        hpclang_hpg_finish(screen);
    }
}
void _clear_screen(void)
{
    (void)write_byte(NULL, 1, '\f');
}
void beep(void)
{
    (void)hpclang_sound_play(880, 100, NULL, NULL);
}
