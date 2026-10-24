#include <wincore.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#ifndef REFERENCE
#include "win-internal.h"
struct hpclang_win_focus hpclang_win_widgets;
unsigned hpclang_win_widget_callbacks;
static int failure;
int hpclang_win_fail(int error)
{
    if (!failure)
        failure = error;
    errno = failure;
    return -1;
}
int hpclang_win_error(void)
{
    return failure;
}
int hpclang_win_dispatch_active(void)
{
    return hpclang_win_widget_callbacks;
}
#endif

static int fonts[2];
hpg_t *hpg_stdscreen;
void win_init(void)
{
}
void win_repaint(void)
{
    puts("repaint");
}
hpg_font_t *hpg_get_minifont(void)
{
    return (hpg_font_t *)&fonts[0];
}
void hpg_clip_reset(hpg_t *g)
{
    assert(g == hpg_stdscreen);
    puts("clip-reset");
}
void hpg_clip(hpg_t *g, int x, int y, int right, int bottom)
{
    assert(g == hpg_stdscreen);
    printf("clip %d %d %d %d\n", x, y, right, bottom);
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    assert(g == hpg_stdscreen);
    printf("color %u\n", color);
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
    printf("fill %d %d %d %d\n", x, y, right, bottom);
}
void hpg_set_font(hpg_t *g, hpg_font_t *font)
{
    assert(g == hpg_stdscreen);
    printf("font %d\n", font == (hpg_font_t *)&fonts[1]);
}
void hpg_set_pattern(hpg_t *g, hpg_pattern_t *pattern)
{
    assert(g == hpg_stdscreen && !pattern);
    puts("solid");
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    assert(g == hpg_stdscreen);
    printf("mode %u\n", mode);
}
static int id(win_widget_t *widget)
{
    return *(int *)win_wcore_get_data(widget);
}
static void install(win_widget_t *widget)
{
    printf("install %d\n", id(widget));
}
static void deinstall(win_widget_t *widget)
{
    printf("remove %d\n", id(widget));
}
static void destroy(win_widget_t *widget)
{
    printf("destroy %d\n", id(widget));
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    *width = 10 + id(widget);
    *height = 6 + id(widget);
    printf("preferred %d\n", id(widget));
}
static void lost(win_widget_t *widget)
{
    printf("lost %d %d\n", id(widget), win_widget_has_focus(widget));
}
static void gained(win_widget_t *widget)
{
    printf("gained %d %d\n", id(widget), win_widget_has_focus(widget));
}
static void state(win_widget_t **widgets, unsigned live)
{
    printf("focus");
    for (unsigned i = 0; i < 4; ++i)
        if (live & (1u << i))
            printf(" %u:%d", i, win_widget_has_focus(widgets[i]));
    puts("");
}
int main(void)
{
    int ids[] = {0, 1, 2, 3};
    for (unsigned mask = 0; mask < 16; ++mask) {
        for (unsigned order = 0; order < 24; ++order) {
            printf("scenario %u %u\n", mask, order);
            win_widget_t *widgets[4];
            for (unsigned i = 0; i < 4; ++i) {
                win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred,
                                                       mask & (1u << i), &ids[i]);
                assert(widget);
                widgets[i] = widget;
                win_wcore_notify_focus(widget, lost, gained);
                win_widget_set_location(widget, -(int)i, (int)i * 2);
                win_widget_set_size(widget, 20, 15);
                win_widget_pack(widget);
                win_widget_set_colors(widget, (unsigned char)(i * 17), 255);
                win_widget_set_font(widget, (hpg_font_t *)&fonts[i % 2]);
                int x, y, width, height;
                unsigned char fg, bg;
                win_widget_get_location(widget, &x, &y);
                win_widget_get_size(widget, &width, &height);
                win_widget_get_colors(widget, &fg, &bg);
                printf("properties %d %d %d %d %u %u %d\n", x, y, width, height, fg, bg,
                       win_widget_get_font(widget) == (hpg_font_t *)&fonts[1]);
                for (unsigned transparent = 0; transparent < 4; ++transparent) {
                    win_widget_set_transparent(widget, transparent);
                    win_wcore_paint_base(widget);
                }
            }
            unsigned live = 0, code = order;
            for (unsigned n = 4; n; --n) {
                unsigned choice = code % n;
                code /= n;
                for (unsigned i = 0; i < 4; ++i) {
                    if (live & (1u << i))
                        continue;
                    if (choice--)
                        continue;
                    win_add_widget(widgets[i]);
                    live |= 1u << i;
                    break;
                }
                state(widgets, live);
            }
            for (unsigned n = 0; n < 5; ++n) {
                win_focus_next();
                state(widgets, live);
            }
            for (unsigned n = 0; n < 5; ++n) {
                win_focus_prev();
                state(widgets, live);
            }
            /* Remove current nodes too when another focusable node survives.
             * Otherwise avoid the original dangling-cursor UB by removing a
             * noncurrent node. Unit sanitizers cover the repaired UB case. */
            for (unsigned n = 4; n; --n) {
                unsigned focusable_live = mask & live;
                int move_current = (order & 1) && (focusable_live & (focusable_live - 1));
                for (unsigned i = 0; i < 4; ++i) {
                    if (!(live & (1u << i)))
                        continue;
                    if (n > 1 && win_widget_has_focus(widgets[i]) != !!move_current)
                        continue;
                    win_remove_widget(widgets[i]);
                    win_widget_destroy(widgets[i]);
                    live &= ~(1u << i);
                    break;
                }
                state(widgets, live);
            }
#ifndef REFERENCE
            assert(!failure && !hpclang_win_widgets.current);
#endif
        }
    }
    return 0;
}
