#include <wincore.h>
#include "win-internal.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

struct win_widget
{
    struct hpclang_win_focus_node node;
    void (*install)(win_widget_t *), (*deinstall)(win_widget_t *), (*destroy)(win_widget_t *);
    void (*preferred_size)(win_widget_t *, int *, int *);
    void (*lost)(win_widget_t *), (*gained)(win_widget_t *);
    void *data;
    hpg_font_t *font;
    int x, y, width, height;
    unsigned char foreground, background, transparent;
};
static int valid(win_widget_t *widget)
{
    win_init();
    if (widget)
        return 1;
    hpclang_win_fail(EINVAL);
    return 0;
}
int hpclang_win_widget_installed(win_widget_t *widget)
{
    return widget && widget->node.owner;
}
void *hpclang_win_widget_data(win_widget_t *widget, void (*destroy)(win_widget_t *))
{
    if (!valid(widget))
        return NULL;
    if (widget->destroy != destroy) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    return widget->data;
}
static void call(win_widget_t *widget, void (*function)(win_widget_t *))
{
    if (function) {
        ++hpclang_win_widget_callbacks;
        function(widget);
        --hpclang_win_widget_callbacks;
    }
}
static void install(void *data)
{
    win_widget_t *widget = data;
    call(widget, widget->install);
}
static void deinstall(void *data)
{
    win_widget_t *widget = data;
    call(widget, widget->deinstall);
}
static void lost(void *data)
{
    win_widget_t *widget = data;
    call(widget, widget->lost);
}
static void gained(void *data)
{
    win_widget_t *widget = data;
    call(widget, widget->gained);
}
static void repaint(void *data)
{
    (void)data;
    win_repaint();
}
static int geometry(int x, int y, int width, int height)
{
    if (width < 0 || height < 0) {
        hpclang_win_fail(EINVAL);
        return 0;
    }
    if (x > INT_MAX - width || y > INT_MAX - height) {
        hpclang_win_fail(EOVERFLOW);
        return 0;
    }
    return 1;
}
win_widget_t *win_wcore_alloc(void (*on_install)(win_widget_t *),
                              void (*on_deinstall)(win_widget_t *),
                              void (*on_destroy)(win_widget_t *),
                              void (*preferred_size)(win_widget_t *, int *, int *), int focusable,
                              void *data)
{
    win_init();
    if (!on_install || !on_deinstall || !on_destroy || !preferred_size) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    if (hpclang_win_error())
        return NULL;
    win_widget_t *widget = calloc(1, sizeof(*widget));
    if (!widget) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    widget->install = on_install;
    widget->deinstall = on_deinstall;
    widget->destroy = on_destroy;
    widget->preferred_size = preferred_size;
    widget->data = data;
    widget->font = hpg_get_minifont();
    widget->foreground = HPG_COLOR_BLACK;
    widget->node = (struct hpclang_win_focus_node){.data = widget,
                                                   .install = install,
                                                   .deinstall = deinstall,
                                                   .lost = lost,
                                                   .gained = gained,
                                                   .focusable = !!focusable};
    win_widget_pack(widget);
    if (hpclang_win_error()) {
        /* Construction failed: application data ownership stays with caller. */
        free(widget);
        return NULL;
    }
    return widget;
}
void win_widget_destroy(win_widget_t *widget)
{
    if (!valid(widget))
        return;
    if (widget->node.owner || hpclang_win_dispatch_active()) {
        hpclang_win_fail(EBUSY);
        return;
    }
    call(widget, widget->destroy);
    free(widget);
}
void win_add_widget(win_widget_t *widget)
{
    if (!valid(widget) || hpclang_win_error())
        return;
    if (hpclang_win_widget_callbacks) {
        hpclang_win_fail(EBUSY);
        return;
    }
    hpclang_win_widgets.repaint = repaint;
    if (hpclang_win_focus_add(&hpclang_win_widgets, &widget->node))
        hpclang_win_fail(errno);
}
void win_remove_widget(win_widget_t *widget)
{
    if (hpclang_win_widget_callbacks) {
        hpclang_win_fail(EBUSY);
        return;
    }
    if (valid(widget) && hpclang_win_focus_remove(&hpclang_win_widgets, &widget->node))
        hpclang_win_fail(errno);
}
void win_focus_next(void)
{
    win_init();
    if (!hpclang_win_error() && hpclang_win_focus_step(&hpclang_win_widgets, 0))
        hpclang_win_fail(errno);
}
void win_focus_prev(void)
{
    win_init();
    if (!hpclang_win_error() && hpclang_win_focus_step(&hpclang_win_widgets, 1))
        hpclang_win_fail(errno);
}
int win_widget_has_focus(win_widget_t *widget)
{
    return valid(widget) && hpclang_win_focus_has(&hpclang_win_widgets, &widget->node);
}
void *win_wcore_get_data(win_widget_t *widget)
{
    return valid(widget) ? widget->data : NULL;
}
void win_wcore_notify_focus(win_widget_t *widget, void (*on_lost)(win_widget_t *),
                            void (*on_gained)(win_widget_t *))
{
    if (valid(widget)) {
        widget->lost = on_lost;
        widget->gained = on_gained;
    }
}
void win_wcore_pref_size(win_widget_t *widget, int *width, int *height)
{
    if (!valid(widget))
        return;
    if (!width || !height) {
        hpclang_win_fail(EINVAL);
        return;
    }
    ++hpclang_win_widget_callbacks;
    widget->preferred_size(widget, width, height);
    --hpclang_win_widget_callbacks;
}
void win_widget_pack(win_widget_t *widget)
{
    if (!valid(widget))
        return;
    int width = 0, height = 0;
    win_wcore_pref_size(widget, &width, &height);
    if (hpclang_win_error())
        return;
    if (geometry(widget->x, widget->y, width, height)) {
        widget->width = width;
        widget->height = height;
    }
}
void win_widget_set_location(win_widget_t *widget, int x, int y)
{
    if (valid(widget) && geometry(x, y, widget->width, widget->height)) {
        widget->x = x;
        widget->y = y;
        win_repaint();
    }
}
void win_widget_get_location(win_widget_t *widget, int *x, int *y)
{
    if (!valid(widget))
        return;
    if (!x || !y) {
        hpclang_win_fail(EINVAL);
        return;
    }
    *x = widget->x;
    *y = widget->y;
}
void win_widget_set_size(win_widget_t *widget, int width, int height)
{
    if (valid(widget) && geometry(widget->x, widget->y, width, height)) {
        widget->width = width;
        widget->height = height;
        win_repaint();
    }
}
void win_widget_get_size(win_widget_t *widget, int *width, int *height)
{
    if (!valid(widget))
        return;
    if (!width || !height) {
        hpclang_win_fail(EINVAL);
        return;
    }
    *width = widget->width;
    *height = widget->height;
}
void win_widget_set_colors(win_widget_t *widget, unsigned char fg, unsigned char bg)
{
    if (valid(widget)) {
        widget->foreground = fg;
        widget->background = bg;
        win_repaint();
    }
}
void win_widget_get_colors(win_widget_t *widget, unsigned char *fg, unsigned char *bg)
{
    if (!valid(widget))
        return;
    if (!fg || !bg) {
        hpclang_win_fail(EINVAL);
        return;
    }
    *fg = widget->foreground;
    *bg = widget->background;
}
void win_widget_set_font(win_widget_t *widget, hpg_font_t *font)
{
    if (valid(widget)) {
        widget->font = font;
        win_repaint();
    }
}
hpg_font_t *win_widget_get_font(win_widget_t *widget)
{
    return valid(widget) ? widget->font : NULL;
}
void win_widget_set_transparent(win_widget_t *widget, unsigned value)
{
    if (valid(widget)) {
        widget->transparent = value & 1u;
        win_repaint();
    }
}
void win_wcore_paint_base(win_widget_t *widget)
{
    if (!valid(widget) || hpclang_win_error())
        return;
    int saved = errno;
    errno = 0;
    int right = widget->x + widget->width, bottom = widget->y + widget->height;
    hpg_clip_reset(hpg_stdscreen);
    hpg_clip(hpg_stdscreen, widget->x, widget->y, right, bottom);
    if (!widget->transparent) {
        hpg_set_color(hpg_stdscreen, widget->background);
        hpg_fill_rect(widget->x, widget->y, right, bottom);
    }
    hpg_set_color(hpg_stdscreen, widget->foreground);
    hpg_set_font(hpg_stdscreen, widget->font);
    hpg_set_pattern(hpg_stdscreen, NULL);
    hpg_set_mode(hpg_stdscreen, HPG_MODE_PAINT);
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
}
