#include <wincheckbox.h>
#include <winsoftmenu.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "win-internal.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

struct checkbox
{
    char *label;
    win_widget_t *menu;
    int index, selected, event, active, bound;
};
static void destroy(win_widget_t *widget)
{
    free(win_wcore_get_data(widget));
}
static struct checkbox *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
char *win_checkbox_get_label(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    return check ? check->label : NULL;
}
void win_checkbox_set_label(win_widget_t *widget, char *label)
{
    struct checkbox *check = data(widget);
    if (!check)
        return;
    if (!label) {
        hpclang_win_fail(EINVAL);
        return;
    }
    check->label = label;
    win_repaint();
}
int win_checkbox_is_selected(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    return check ? check->selected : 0;
}
void win_checkbox_set_selected(win_widget_t *widget, int selected)
{
    struct checkbox *check = data(widget);
    if (check)
        check->selected = !!selected;
}
static void lost(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    if (check->menu && check->bound)
        win_softmenu_delete_item(check->menu, check->index, check->index);
    check->bound = 0;
}
static void gained(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    /* The focus foundation calls gained before install for the first node. */
    if (check->menu && check->active) {
        win_softmenu_set_item(check->menu, check->index, "CHK", check->event, NULL);
        check->bound = !hpclang_win_error();
    }
}
void win_checkbox_set_menu(win_widget_t *widget, win_widget_t *menu, int index)
{
    struct checkbox *check = data(widget);
    if (!check)
        return;
    if (menu && (index < 0 || index >= WIN_SOFTMENU_MAX_ITEMS)) {
        hpclang_win_fail(EINVAL);
        return;
    }
    if (menu && !hpclang_win_softmenu_valid(menu))
        return;
    int focused = check->active && win_widget_has_focus(widget);
    if (focused)
        lost(widget);
    check->menu = menu;
    check->index = index;
    if (focused)
        gained(widget);
}
static int toggle(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget) || !win_widget_has_focus(widget))
        return 0;
    struct checkbox *check = data(widget);
    check->selected = !check->selected;
    win_repaint();
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
        if (!pressed->lshift && !pressed->rshift && !pressed->alpha)
            return toggle(NULL, widget);
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
    struct checkbox *check = data(widget);
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int x, y;
    unsigned char foreground, background;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_colors(widget, &foreground, &background);
    int saved = errno;
    errno = 0;
    if (win_widget_has_focus(widget)) {
        hpg_fill_rect(x, y, offset(x, 6), offset(y, 8));
        hpg_set_color(NULL, background);
    }
    char bytes[8] = {0, 0, 0, 0, 0, 0, 0x3e, 0};
    if (check->selected) {
        bytes[1] = 0x20;
        bytes[2] = 0x30;
        bytes[3] = 0x1a;
        bytes[4] = 0x0e;
        bytes[5] = 4;
    }
    hpg_pattern_t *old = hpg_get_pattern(NULL);
    hpg_pattern_t *pattern = hpg_alloc_pattern(bytes, 8, 0);
    if (!pattern) {
        hpclang_win_fail(errno ? errno : ENOMEM);
        return 0;
    }
    hpg_set_pattern(NULL, pattern);
    hpg_fill_rect(x, y, offset(x, 8), offset(y, 8));
    hpg_set_pattern(NULL, old);
    hpg_free_pattern(pattern);
    hpg_set_color(NULL, foreground);
    hpg_draw_text(check->label, offset(x, 8), offset(y, 1));
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static void install(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    /* Allocate per installation: reset may recycle event numbers while a
     * removed descriptor still exists. Never retain a global stale event ID. */
    check->event = win_register_event();
    if (check->event < 0)
        return;
    check->active = 1;
    win_add_event_handler(WIN_REPAINT_EVENT, 1000000000, paint, widget);
    win_add_event_handler(WIN_KEY_TYPED, 1000000000, key, widget);
    win_add_event_handler(check->event, 1000000000, toggle, widget);
    if (win_widget_has_focus(widget))
        gained(widget);
}
static void deinstall(win_widget_t *widget)
{
    struct checkbox *check = data(widget);
    /* Singleton removal does not emit focus-lost. Clear its menu binding too. */
    lost(widget);
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
    win_remove_event_handler(WIN_KEY_TYPED, key, widget);
    if (check->active)
        win_remove_event_handler(check->event, toggle, widget);
    check->active = 0;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct checkbox *check = data(widget);
    size_t columns = 0, widest = 0, rows = 0;
    for (const char *p = check->label; *p; ++p) {
        if (*p == '\n') {
            ++rows;
            columns = 0;
        } else if (++columns > widest)
            widest = columns;
    }
    rows += columns != 0;
    hpg_font_t *font = win_widget_get_font(widget);
    int advance = hpg_font_get_advance(font), high = hpg_font_get_height(font);
    if (advance < 0 || high < 0 ||
        (advance && widest > (size_t)(INT_MAX - 8) / (unsigned)advance) ||
        (high && rows > (size_t)(INT_MAX - 2) / (unsigned)high)) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    *width = (int)(widest * (unsigned)advance) + 8;
    *height = (int)(rows * (unsigned)high) + 2;
}
win_widget_t *win_checkbox_new(char *label)
{
    if (!label) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    struct checkbox *check = calloc(1, sizeof(*check));
    if (!check) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    check->label = label;
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred, 1, check);
    if (!widget)
        free(check);
    else
        win_wcore_notify_focus(widget, lost, gained);
    return widget;
}
