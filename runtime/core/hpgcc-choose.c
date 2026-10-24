#include <winchoose.h>
#include <winsoftmenu.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "win-internal.h"
#include "win-choices.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

struct choose
{
    struct hpclang_win_choices choices;
    win_widget_t *menu;
    size_t cursor;
    int index, event, active, bound, modal;
};
static void destroy(win_widget_t *widget)
{
    struct choose *c = win_wcore_get_data(widget);
    hpclang_win_choices_destroy(&c->choices);
    free(c);
}
static struct choose *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
static void close_box(struct choose *c)
{
    if (c->modal) {
        c->modal = 0;
        win_repaint();
    }
}
void win_choose_add_with_label(win_widget_t *widget, char *option, char *label)
{
    struct choose *c = data(widget);
    if (!c)
        return;
    if (hpclang_win_choices_add(&c->choices, option, label)) {
        hpclang_win_fail(errno);
        return;
    }
    close_box(c);
    if (c->choices.position == c->choices.count - 1)
        win_repaint();
}
void win_choose_add(win_widget_t *widget, char *option)
{
    win_choose_add_with_label(widget, option, option);
}
void win_choose_remove(win_widget_t *widget, char *option)
{
    struct choose *c = data(widget);
    if (!c)
        return;
    int result = hpclang_win_choices_remove(&c->choices, option);
    if (result < 0)
        hpclang_win_fail(errno);
    else if (result) {
        close_box(c);
        win_repaint();
    }
}
char *win_choose_get_choice(win_widget_t *widget)
{
    struct choose *c = data(widget);
    return c ? hpclang_win_choices_choice(&c->choices) : NULL;
}
int win_choose_get_index(win_widget_t *widget)
{
    struct choose *c = data(widget);
    return c ? hpclang_win_choices_index(&c->choices) : -1;
}
void win_choose_set_choice(win_widget_t *widget, char *option)
{
    struct choose *c = data(widget);
    if (c) {
        close_box(c);
        hpclang_win_choices_set_choice(&c->choices, option);
    }
}
void win_choose_set_index(win_widget_t *widget, int index)
{
    struct choose *c = data(widget);
    if (c) {
        close_box(c);
        hpclang_win_choices_set_index(&c->choices, index);
    }
}
static void lost(win_widget_t *widget)
{
    struct choose *c = data(widget);
    close_box(c);
    if (c->menu && c->bound)
        win_softmenu_delete_item(c->menu, c->index, c->index);
    c->bound = 0;
}
static void gained(win_widget_t *widget)
{
    struct choose *c = data(widget);
    if (c->menu && c->active) {
        win_softmenu_set_item(c->menu, c->index, "CHOOS", c->event, NULL);
        c->bound = !hpclang_win_error();
    }
}
void win_choose_set_menu(win_widget_t *widget, win_widget_t *menu, int index)
{
    struct choose *c = data(widget);
    if (!c)
        return;
    if (menu && (index < 0 || index >= WIN_SOFTMENU_MAX_ITEMS)) {
        hpclang_win_fail(EINVAL);
        return;
    }
    if (menu && !hpclang_win_softmenu_valid(menu))
        return;
    close_box(c);
    int focused = c->active && win_widget_has_focus(widget);
    if (focused)
        lost(widget);
    c->menu = menu;
    c->index = index;
    if (focused)
        gained(widget);
}
static int open_box(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget) || !win_widget_has_focus(widget))
        return 0;
    struct choose *c = data(widget);
    if (!c->modal) {
        c->cursor = c->choices.position < c->choices.count ? c->choices.position : 0;
        c->modal = 1;
        win_repaint();
    }
    return 1;
}
static int modal_key(void *event, void *context)
{
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct choose *c = data(widget);
    if (!c->modal)
        return 0;
    if (!event) {
        hpclang_win_fail(EINVAL);
        return 1;
    }
    const win_keytyped_t *key = event;
    size_t count = c->choices.count;
    switch (key->keycode) {
    case KB_UP:
        c->cursor = !count ? 0 : c->cursor ? c->cursor - 1 : count - 1;
        win_repaint();
        break;
    case KB_DN:
        c->cursor = !count || c->cursor + 1 >= count ? 0 : c->cursor + 1;
        win_repaint();
        break;
    case KB_F:
        if (key->alpha)
            break;
        c->choices.position = count ? c->cursor : count;
        close_box(c);
        break;
    case KB_E:
        if (!key->alpha)
            close_box(c);
        break;
    }
    return 1;
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
    const win_keytyped_t *pressed = event;
    switch (pressed->keycode) {
    case KB_UP:
    case KB_LF:
        win_focus_prev();
        return 1;
    case KB_DN:
    case KB_RT:
        win_focus_next();
        return 1;
    case KB_W:
        if (!pressed->lshift && !pressed->rshift && !pressed->alpha) {
            hpclang_win_choices_next(&data(widget)->choices);
            win_repaint();
            return 1;
        }
    }
    return 0;
}
static int offset(int origin, int delta)
{
    return origin > INT_MAX - delta ? INT_MAX : origin + delta;
}
static int paint(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct choose *c = data(widget);
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int x, y, w, h;
    unsigned char foreground, background;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &w, &h);
    win_widget_get_colors(widget, &foreground, &background);
    int saved = errno;
    errno = 0;
    if (win_widget_has_focus(widget)) {
        hpg_fill_rect(x, y, offset(x, w), offset(y, h));
        hpg_set_color(NULL, background);
    }
    if (c->choices.position < c->choices.count)
        hpg_draw_text(c->choices.items[c->choices.position].label, offset(x, 1), offset(y, 1));
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static int paint_box(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct choose *c = data(widget);
    if (!c->modal)
        return 0;
    /* Set overlay state without clearing the ordinary control underneath it.
     * Never mutate the widget ring from a lifecycle callback. */
    int saved = errno;
    errno = 0;
    hpg_set_font(NULL, win_widget_get_font(widget));
    hpg_set_pattern(NULL, NULL);
    hpg_set_mode(NULL, HPG_MODE_PAINT);
    int width = hpg_get_width(NULL), height = hpg_get_height(NULL);
    int high = hpg_font_get_height(win_widget_get_font(widget));
    if (width < 12 || height < 20 || high < 0 || high > height - 12) {
        hpclang_win_fail(ERANGE);
        return 0;
    }
    int step = high + 1;
    size_t rows = (size_t)(height - 12) / (unsigned)step;
    if (rows > c->choices.count)
        rows = c->choices.count;
    size_t start = rows && c->cursor >= rows ? c->cursor - rows + 1 : 0;
    int boxw = width > 104 ? 100 : width - 4;
    int boxh = (int)rows * step + 4;
    int x = (width - boxw) / 2, y = (height - 8 - boxh) / 2;
    unsigned char fg, bg;
    win_widget_get_colors(widget, &fg, &bg);
    hpg_clip_reset(NULL);
    hpg_set_color(NULL, bg);
    hpg_fill_rect(x, y, x + boxw, y + boxh);
    hpg_set_color(NULL, fg);
    hpg_draw_rect(x, y, x + boxw - 1, y + boxh - 1);
    for (size_t row = 0; row < rows; ++row) {
        int top = y + 2 + (int)row * step;
        hpg_clip_reset(NULL);
        hpg_clip(NULL, x + 2, top, x + boxw - 2, top + step);
        hpg_set_color(NULL, fg);
        if (start + row == c->cursor) {
            hpg_fill_rect(x + 2, top, x + boxw - 3, top + step - 1);
            hpg_set_color(NULL, bg);
        }
        hpg_draw_text(c->choices.items[start + row].option, x + 3, top + 1);
    }
    hpg_clip_reset(NULL);
    hpg_set_color(NULL, bg);
    hpg_fill_rect(0, height - 7, width - 1, height - 1);
    hpg_set_font(NULL, hpg_get_minifont());
    hpg_set_color(NULL, fg);
    hpg_fill_rect(88, height - 7, 108, height - 1);
    hpg_fill_rect(110, height - 7, 130, height - 1);
    hpg_set_color(NULL, bg);
    hpg_draw_text("CANCL", 4 * 22 + 1, height - 6);
    hpg_draw_text("OK", 5 * 22 + 7, height - 6);
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 1;
}
static void install(win_widget_t *widget)
{
    struct choose *c = data(widget);
    c->event = win_register_event();
    if (c->event < 0)
        return;
    c->active = 1;
    win_add_event_handler(WIN_REPAINT_EVENT, 1000000000, paint, widget);
    win_add_event_handler(WIN_REPAINT_EVENT, -2000000000, paint_box, widget);
    win_add_event_handler(WIN_KEY_TYPED, 2000000000, modal_key, widget);
    win_add_event_handler(WIN_KEY_TYPED, 1000000000, key, widget);
    win_add_event_handler(c->event, 1000000000, open_box, widget);
    if (win_widget_has_focus(widget))
        gained(widget);
}
static void deinstall(win_widget_t *widget)
{
    struct choose *c = data(widget);
    lost(widget);
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
    win_remove_event_handler(WIN_REPAINT_EVENT, paint_box, widget);
    win_remove_event_handler(WIN_KEY_TYPED, key, widget);
    win_remove_event_handler(WIN_KEY_TYPED, modal_key, widget);
    if (c->active)
        win_remove_event_handler(c->event, open_box, widget);
    c->active = 0;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct choose *c = data(widget);
    size_t widest = 0, tallest = 0;
    for (size_t i = 0; i < c->choices.count; ++i) {
        size_t columns = 0, rows = 0;
        for (const char *p = c->choices.items[i].label; *p; ++p) {
            if (*p == '\n') {
                ++rows;
                columns = 0;
            } else if (++columns > widest)
                widest = columns;
        }
        rows += columns != 0;
        if (rows > tallest)
            tallest = rows;
    }
    hpg_font_t *font = win_widget_get_font(widget);
    int advance = hpg_font_get_advance(font), high = hpg_font_get_height(font);
    if (advance < 0 || high < 0 ||
        (advance && widest > (size_t)(INT_MAX - 1) / (unsigned)advance) ||
        (high && tallest > (size_t)(INT_MAX - 1) / (unsigned)high)) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    *width = (int)(widest * (unsigned)advance) + 1;
    *height = (int)(tallest * (unsigned)high) + 1;
}
win_widget_t *win_choose_new(void)
{
    struct choose *c = calloc(1, sizeof(*c));
    if (!c) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred, 1, c);
    if (!widget)
        free(c);
    else
        win_wcore_notify_focus(widget, lost, gained);
    return widget;
}
