#include <wincore.h>
#include <wintimer.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

/* Device operations are observed independently; event, timer, widget and
 * focus implementations are the production implementations. */
hpg_t *hpg_stdscreen;
static int font_token;
static int draw[16], draw_count, fills, draw_error;
static void record(int value)
{
    draw[draw_count++] = value;
}
hpg_font_t *hpg_get_minifont(void)
{
    return (hpg_font_t *)&font_token;
}
void hpg_clip_reset(hpg_t *g)
{
    assert(g == hpg_stdscreen);
    record(1);
}
void hpg_clip(hpg_t *g, int x, int y, int right, int bottom)
{
    assert(g == hpg_stdscreen && x == 2 && y == 3 && right == 12 && bottom == 9);
    record(2);
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    assert(g == hpg_stdscreen);
    record(color == 17 ? 3 : 5);
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
    assert(x == 2 && y == 3 && right == 12 && bottom == 9);
    ++fills;
    record(4);
    if (draw_error)
        errno = EIO;
}
void hpg_set_font(hpg_t *g, hpg_font_t *font)
{
    assert(g == hpg_stdscreen && font == hpg_get_minifont());
    record(6);
}
void hpg_set_pattern(hpg_t *g, hpg_pattern_t *pattern)
{
    assert(g == hpg_stdscreen && !pattern);
    record(7);
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    assert(g == hpg_stdscreen && mode == HPG_MODE_PAINT);
    record(8);
}
int hpclang_hpg_indicator_set(unsigned char indicator, unsigned char color)
{
    (void)indicator;
    (void)color;
    return 0;
}
int hpclang_hpg_frame_begin(void *data)
{
    (void)data;
    return 0;
}
int hpclang_hpg_frame_present(void *data)
{
    (void)data;
    return 0;
}
int hpclang_hpgcc_keymatrix_read(keymatrix *matrix)
{
    matrix->full = 0;
    return 0;
}
int hpclang_time_read_us(uint64_t *value)
{
    *value = 0;
    return 0;
}

struct custom
{
    int installed, removed, destroyed, lost, gained, events;
};
static int event_id, forbid_destroy, bad_size, bad_construct;
static int event(void *payload, void *data)
{
    win_widget_t *widget = data;
    struct custom *custom = win_wcore_get_data(widget);
    ++custom->events;
    if (payload) {
        win_remove_widget(widget);
        win_widget_destroy(widget);
        assert(!custom->destroyed && hpclang_win_error() == EBUSY);
    }
    return 0;
}
static int timer(void *data)
{
    (void)data;
    return 0;
}
static void install(win_widget_t *widget)
{
    struct custom *custom = win_wcore_get_data(widget);
    ++custom->installed;
    win_add_event_handler(event_id, 0, event, widget);
    win_add_timer(timer, widget, 10, 0);
}
static void deinstall(win_widget_t *widget)
{
    struct custom *custom = win_wcore_get_data(widget);
    ++custom->removed;
    win_remove_event_handler(event_id, event, widget);
    win_remove_timer(timer, widget);
}
static void destroy(win_widget_t *widget)
{
    struct custom *custom = win_wcore_get_data(widget);
    assert(!forbid_destroy);
    ++custom->destroyed;
    assert(hpclang_win_reset() == -1 && errno == EBUSY);
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    assert(win_wcore_get_data(widget));
    *width = bad_size ? -1 : 10;
    *height = 6;
    if (bad_construct)
        win_add_widget(widget);
}
static void lost(win_widget_t *widget)
{
    struct custom *custom = win_wcore_get_data(widget);
    assert(win_widget_has_focus(widget));
    ++custom->lost;
}
static void gained(win_widget_t *widget)
{
    struct custom *custom = win_wcore_get_data(widget);
    assert(win_widget_has_focus(widget));
    ++custom->gained;
}
static win_widget_t *make(struct custom *data)
{
    return win_wcore_alloc(install, deinstall, destroy, preferred, 1, data);
}
int main(void)
{
    struct custom a = {0}, b = {0};
    event_id = win_register_event();
    win_widget_t *first = make(&a), *second = make(&b);
    assert(first && second);
    win_wcore_notify_focus(first, lost, gained);
    win_wcore_notify_focus(second, lost, gained);
    int x, y;
    win_widget_get_size(first, &x, &y);
    assert(x == 10 && y == 6);
    win_widget_get_location(first, &x, &y);
    assert(!x && !y);
    assert(win_widget_get_font(first) == hpg_get_minifont());
    win_widget_set_location(first, 2, 3);
    win_widget_set_colors(first, 255, 17);
    unsigned char fg, bg;
    win_widget_get_colors(first, &fg, &bg);
    assert(fg == 255 && bg == 17);
    errno = ERANGE;
    win_wcore_paint_base(first);
    assert(errno == ERANGE && !hpclang_win_error() && draw_count == 8 && fills == 1);
    for (int i = 0; i < 8; ++i)
        assert(draw[i] == i + 1);
    draw_count = 0;
    win_widget_set_transparent(first, 3);
    win_wcore_paint_base(first);
    assert(draw_count == 6 && fills == 1);
    win_add_widget(first);
    win_add_widget(second);
    assert(a.installed == 1 && a.gained == 1 && b.installed == 1 && b.gained == 0);
    assert(hpclang_win_reset() == -1 && errno == EBUSY && !hpclang_win_error());
    win_focus_next();
    assert(a.lost == 1 && b.gained == 1 && win_widget_has_focus(second));
    win_focus_prev();
    assert(b.lost == 1 && a.gained == 2);
    assert(!win_post_event(event_id, NULL) && a.events == 1 && b.events == 1);
    forbid_destroy = 1;
    win_widget_destroy(first);
    assert(hpclang_win_error() == EBUSY && !a.destroyed);
    /* Sticky errors must not make removal/cleanup impossible. */
    win_remove_widget(first);
    win_remove_widget(second);
    assert(a.removed == 1 && b.removed == 1);
    forbid_destroy = 0;
    win_widget_destroy(first);
    win_widget_destroy(second);
    assert(a.destroyed == 1 && b.destroyed == 1);
    assert(!hpclang_win_reset());
    event_id = win_register_event();
    a = (struct custom){0};
    first = make(&a);
    win_add_widget(first);
    assert(win_post_event(event_id, &a) == -1 && hpclang_win_error() == EBUSY);
    assert(a.events == 1 && a.removed == 1 && !a.destroyed);
    win_widget_destroy(first);
    assert(a.destroyed == 1 && !hpclang_win_reset());
    first = make(&a);
    win_widget_set_location(first, INT_MAX, 0);
    assert(hpclang_win_error() == EOVERFLOW);
    win_widget_get_location(first, &x, &y);
    assert(!x && !y);
    win_widget_destroy(first);
    assert(!hpclang_win_reset());
    bad_size = 1;
    assert(!make(&a) && hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    bad_size = 0;
    bad_construct = 1;
    assert(!make(&a) && hpclang_win_error() == EBUSY);
    assert(!hpclang_win_reset());
    bad_construct = 0;
    first = make(&a);
    win_widget_set_location(first, 2, 3);
    draw_count = 0;
    draw_error = 1;
    win_wcore_paint_base(first);
    assert(hpclang_win_error() == EIO);
    win_widget_destroy(first);
    assert(!hpclang_win_reset());
    return 0;
}
