#include <wintext.h>
#include <wincore.h>
#include <winkeys.h>
#include <wintimer.h>
#include <hpkeyb49.h>
#include "win-internal.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct text_control
{
    char *buffer;
    size_t capacity, limit, cursor;
    int visible;
};
static void destroy(win_widget_t *widget)
{
    struct text_control *text = win_wcore_get_data(widget);
    free(text->buffer);
    free(text);
}
static struct text_control *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
static size_t length(struct text_control *text)
{
    size_t size = strlen(text->buffer);
    if (text->cursor > size)
        text->cursor = size;
    return size;
}
char *win_text_get_text(win_widget_t *widget)
{
    struct text_control *text = data(widget);
    return text ? text->buffer : NULL;
}
void win_text_set_text(win_widget_t *widget, char *source)
{
    struct text_control *text = data(widget);
    if (!text)
        return;
    if (!source) {
        hpclang_win_fail(EINVAL);
        return;
    }
    size_t size = strlen(source);
    if (size > INT_MAX) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    if (size >= text->capacity) {
        char *replacement = malloc(size + 1);
        if (!replacement) {
            hpclang_win_fail(ENOMEM);
            return;
        }
        memcpy(replacement, source, size + 1);
        free(text->buffer);
        text->buffer = replacement;
        text->capacity = size + 1;
    } else {
        memmove(text->buffer, source, size + 1);
    }
    if (text->cursor > size)
        text->cursor = size;
    win_repaint();
}
static int position(int origin, size_t column, int advance)
{
    /* Drawing outside the widget clip has no visible effect; saturating the
     * coordinate avoids signed overflow for long text in a small component. */
    if (advance <= 0)
        return origin;
    uint64_t offset = (uint64_t)column * (unsigned)advance;
    if (offset > (uint64_t)((int64_t)INT_MAX - origin))
        return INT_MAX;
    return (int)((int64_t)origin + (int64_t)offset);
}
static int paint(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct text_control *text = data(widget);
    size_t size = length(text);
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int x, y, width, height;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &width, &height);
    if (!width || !height)
        return 0;
    ++x;
    ++y;
    int saved = errno;
    errno = 0;
    if (win_widget_has_focus(widget) && text->visible) {
        int advance = hpg_font_get_advance(win_widget_get_font(widget));
        char cursor = text->buffer[text->cursor];
        text->buffer[text->cursor] = 0;
        hpg_draw_text(text->buffer, x, y);
        text->buffer[text->cursor] = cursor;
        hpg_draw_letter(28, position(x, text->cursor, advance), y);
        if (text->cursor < size)
            hpg_draw_text(text->buffer + text->cursor + 1, position(x, text->cursor + 1, advance),
                          y);
    } else {
        hpg_draw_text(text->buffer, x, y);
    }
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static int key(void *event, void *context)
{
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget) || !win_widget_has_focus(widget))
        return 0;
    if (!event) {
        hpclang_win_fail(EINVAL);
        return 0;
    }
    const win_keytyped_t *typed = event;
    struct text_control *text = data(widget);
    size_t size = length(text);
    switch (typed->keycode) {
    case KB_UP:
        win_focus_prev();
        return 1;
    case KB_DN:
        win_focus_next();
        return 1;
    case KB_LF:
        if (text->cursor)
            --text->cursor;
        else
            win_focus_prev();
        break;
    case KB_RT:
        if (text->cursor < size)
            ++text->cursor;
        else
            win_focus_next();
        break;
    case KB_BKS:
        if (typed->lshift) {
            if (text->cursor < size)
                memmove(text->buffer + text->cursor, text->buffer + text->cursor + 1,
                        size - text->cursor);
        } else if (typed->rshift) {
            text->cursor = 0;
            text->buffer[0] = 0;
        } else if (text->cursor) {
            --text->cursor;
            memmove(text->buffer + text->cursor, text->buffer + text->cursor + 1,
                    size - text->cursor);
        }
        break;
    default:
        if (typed->ascii <= 0)
            return 0;
        if (size < text->limit) {
            memmove(text->buffer + text->cursor + 1, text->buffer + text->cursor,
                    size - text->cursor + 1);
            text->buffer[text->cursor++] = typed->ascii;
        }
        break;
    }
    win_repaint();
    return 1;
}
static int blink(void *context)
{
    win_widget_t *widget = context;
    if (win_widget_has_focus(widget)) {
        struct text_control *text = data(widget);
        text->visible = !text->visible;
        win_repaint();
    }
    return 0;
}
static void install(win_widget_t *widget)
{
    win_add_event_handler(WIN_REPAINT_EVENT, 1000000000, paint, widget);
    win_add_event_handler(WIN_KEY_TYPED, 1000000000, key, widget);
    win_add_timer(blink, widget, 500, 1);
}
static void deinstall(win_widget_t *widget)
{
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
    win_remove_event_handler(WIN_KEY_TYPED, key, widget);
    win_remove_timer(blink, widget);
}
static void size_for(win_widget_t *widget, size_t columns, size_t rows, int *width, int *height)
{
    hpg_font_t *font = win_widget_get_font(widget);
    int advance = hpg_font_get_advance(font), high = hpg_font_get_height(font);
    if (advance < 0 || high < 0 ||
        (advance && columns > (size_t)(INT_MAX - 1) / (unsigned)advance) ||
        (high && rows > (size_t)(INT_MAX - 1) / (unsigned)high)) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    *width = (int)(columns * (unsigned)advance) + 1;
    *height = (int)(rows * (unsigned)high) + 1;
}
static void label_size(win_widget_t *widget, int *width, int *height)
{
    struct text_control *text = data(widget);
    size_t columns = 0, widest = 0, rows = 0;
    for (char *p = text->buffer; *p; ++p) {
        if (*p == '\n') {
            ++rows;
            columns = 0;
        } else if (++columns > widest)
            widest = columns;
    }
    if (columns)
        ++rows;
    size_for(widget, widest, rows, width, height);
}
static void entry_size(win_widget_t *widget, int *width, int *height)
{
    struct text_control *text = data(widget);
    size_for(widget, text->limit + 1, 1, width, height);
}
static win_widget_t *create(char *source, size_t limit)
{
    struct text_control *text = calloc(1, sizeof(*text));
    if (!text) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    text->buffer = malloc(limit + 1);
    if (!text->buffer) {
        free(text);
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    text->capacity = limit + 1;
    text->limit = limit;
    if (source)
        memcpy(text->buffer, source, limit + 1);
    else
        text->buffer[0] = 0;
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy,
                                           source ? label_size : entry_size, !source, text);
    if (!widget) {
        free(text->buffer);
        free(text);
    }
    return widget;
}
win_widget_t *win_text_new_label(char *source)
{
    if (!source) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    size_t size = strlen(source);
    if (size > INT_MAX) {
        hpclang_win_fail(EOVERFLOW);
        return NULL;
    }
    return create(source, size);
}
win_widget_t *win_text_new_entry(int limit)
{
    if (limit < 0) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    return create(NULL, (size_t)limit);
}
