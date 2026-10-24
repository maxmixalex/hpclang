#include <winprogress.h>
#include <wincore.h>
#include <winkeys.h>
#include <wintimer.h>
#include <hpkeyb49.h>
#include "win-internal.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct progress
{
    double minimum, maximum, value, increment;
    int first, next, event, direction, orientation, cursor;
};
static void destroy(win_widget_t *widget)
{
    free(win_wcore_get_data(widget));
}
static struct progress *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
void win_progress_set_value(win_widget_t *widget, double value)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (isnan(value)) {
        hpclang_win_fail(EINVAL);
        return;
    }
    bar->value = value <= bar->minimum   ? bar->minimum
                 : value >= bar->maximum ? bar->maximum
                                         : value;
    win_repaint();
}
double win_progress_get_value(win_widget_t *widget)
{
    struct progress *bar = data(widget);
    return bar ? bar->value : 0;
}
void win_progress_set_inc(win_widget_t *widget, double increment)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (!isfinite(increment) || increment <= 0) {
        hpclang_win_fail(EINVAL);
        return;
    }
    bar->increment = increment;
}
void win_progress_set_range(win_widget_t *widget, double minimum, double maximum)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (!isfinite(minimum) || !isfinite(maximum) || !isfinite(maximum - minimum)) {
        hpclang_win_fail(EINVAL);
        return;
    }
    if (minimum < maximum) {
        bar->minimum = minimum;
        bar->maximum = maximum;
        if (bar->value < minimum)
            bar->value = minimum;
        if (bar->value > maximum)
            bar->value = maximum;
    }
    win_repaint();
}
void win_progress_set_orient(win_widget_t *widget, int orientation)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (orientation < 0 || orientation > 3) {
        hpclang_win_fail(EINVAL);
        return;
    }
    bar->orientation = orientation;
    win_repaint();
}
void win_progress_set_delay(win_widget_t *widget, int first, int next)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (first <= 0 || next <= 0) {
        hpclang_win_fail(EINVAL);
        return;
    }
    bar->first = first;
    bar->next = next;
}
void win_progress_set_event(win_widget_t *widget, int event)
{
    struct progress *bar = data(widget);
    if (!bar)
        return;
    if (event < -1) {
        hpclang_win_fail(EINVAL);
        return;
    }
    bar->event = event;
}
static int paint(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    struct progress *bar = data(widget);
    if (!bar || !hpclang_win_widget_installed(widget))
        return 0;
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int x, y, width, height;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &width, &height);
    if (!width || !height)
        return 0;
    int saved = errno;
    errno = 0;
    if (win_widget_has_focus(widget) && bar->cursor) {
        char bytes[] = {0x55, (char)0xaa, 0x55, (char)0xaa};
        hpg_pattern_t *old = hpg_get_pattern(hpg_stdscreen);
        hpg_pattern_t *pattern = hpg_alloc_pattern(bytes, 4, 0);
        if (!pattern) {
            hpclang_win_fail(errno ? errno : ENOMEM);
            return 0;
        }
        hpg_set_pattern(hpg_stdscreen, pattern);
        hpg_fill_rect(x, y, x + width - 1, y + height - 1);
        hpg_set_pattern(hpg_stdscreen, old);
        hpg_free_pattern(pattern);
    }
    int right = x + width - 1, bottom = y + height - 1;
    hpg_draw_rect(x, y, right, bottom);
    double fraction = (bar->value - bar->minimum) / (bar->maximum - bar->minimum);
    switch (bar->orientation) {
    case WIN_PROGRESS_HORIZ:
        hpg_fill_rect(x, y, x + (int)((width - 1) * fraction), bottom);
        break;
    case WIN_PROGRESS_VERT:
        hpg_fill_rect(x, y + (int)(height * (1 - fraction)), right, bottom);
        break;
    case WIN_PROGRESS_HORIZ_RIGHT:
        hpg_fill_rect(x + (int)(width * (1 - fraction)), y, right, bottom);
        break;
    default:
        hpg_fill_rect(x, y, right, y + (int)((height - 1) * fraction));
        break;
    }
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static void update(win_widget_t *widget)
{
    struct progress *bar = data(widget);
    win_progress_set_value(widget, bar->value + bar->direction * bar->increment);
    if (bar->event >= 0)
        win_post_event(bar->event, widget);
}
static int repeat(void *context)
{
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget) || !win_widget_has_focus(widget))
        return 1;
    update(widget);
    return !hpclang_win_widget_installed(widget);
}
static int first(void *context)
{
    win_widget_t *widget = context;
    if (hpclang_win_widget_installed(widget) && win_widget_has_focus(widget)) {
        update(widget);
        struct progress *bar = data(widget);
        if (hpclang_win_widget_installed(widget))
            win_add_timer(repeat, widget, bar->next, 1);
    }
    return 1;
}
static void cancel(win_widget_t *widget)
{
    win_remove_timer(first, widget);
    win_remove_timer(repeat, widget);
}
static int pressed(void *event, void *context)
{
    uintptr_t key = (uintptr_t)event;
    if (key != KB_UP && key != KB_DN && key != KB_LF && key != KB_RT)
        return 0;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget) || !win_widget_has_focus(widget))
        return 0;
    struct progress *bar = data(widget);
    static const int increment[] = {KB_RT, KB_UP, KB_LF, KB_DN};
    static const int decrement[] = {KB_LF, KB_DN, KB_RT, KB_UP};
    bar->direction = key == (uintptr_t)increment[bar->orientation]   ? 1
                     : key == (uintptr_t)decrement[bar->orientation] ? -1
                                                                     : 0;
    if ((bar->direction > 0 && bar->value == bar->maximum) ||
        (bar->direction < 0 && bar->value == bar->minimum))
        bar->direction = 0;
    if (!bar->direction) {
        cancel(widget);
        if (key == KB_LF || key == KB_UP)
            win_focus_prev();
        else
            win_focus_next();
    } else {
        update(widget);
        win_repaint();
        cancel(widget);
        if (hpclang_win_widget_installed(widget))
            win_add_timer(first, widget, bar->first, 0);
    }
    return 1;
}
static int released(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    struct progress *bar = data(widget);
    bar->direction = 0;
    cancel(widget);
    return 0;
}
static int blink(void *context)
{
    win_widget_t *widget = context;
    struct progress *bar = data(widget);
    bar->cursor = !bar->cursor;
    if (win_widget_has_focus(widget))
        win_repaint();
    return 0;
}
static void install(win_widget_t *widget)
{
    win_add_event_handler(WIN_REPAINT_EVENT, 2000000000, paint, widget);
    win_add_event_handler(WIN_KEY_PRESSED, 2000000000, pressed, widget);
    win_add_event_handler(WIN_KEY_RELEASED, 2000000000, released, widget);
    win_add_timer(blink, widget, 500, 1);
}
static void deinstall(win_widget_t *widget)
{
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
    win_remove_event_handler(WIN_KEY_PRESSED, pressed, widget);
    win_remove_event_handler(WIN_KEY_RELEASED, released, widget);
    win_remove_timer(blink, widget);
    cancel(widget);
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct progress *bar = data(widget);
    double requested = (bar->maximum - bar->minimum + (bar->increment - 1)) / bar->increment;
    if (!isfinite(requested) || requested > INT_MAX) {
        hpclang_win_fail(EOVERFLOW);
        return;
    }
    int major = requested < 20 ? 20 : (int)requested;
    *width = (bar->orientation & 1) ? 5 : major;
    *height = (bar->orientation & 1) ? major : 5;
}
win_widget_t *win_progress_new(int slider)
{
    struct progress *bar = calloc(1, sizeof(*bar));
    if (!bar) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    bar->maximum = 100;
    bar->increment = 10;
    bar->first = 1000;
    bar->next = 200;
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred, slider, bar);
    if (!widget)
        free(bar);
    return widget;
}
