#include <winsoftmenu.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include <hpclang/device.h>
#include "win-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct menu_item
{
    char label[6];
    unsigned char foreground, background, disabled_foreground, disabled_background;
    int enabled, event;
    void *extra;
};
struct softmenu
{
    struct menu_item items[WIN_SOFTMENU_MAX_ITEMS];
    unsigned occupied, start;
};
static void destroy(win_widget_t *widget)
{
    free(win_wcore_get_data(widget));
}
static struct softmenu *data(win_widget_t *widget)
{
    return hpclang_win_widget_data(widget, destroy);
}
static struct softmenu *slot(win_widget_t *widget, int index)
{
    struct softmenu *menu = data(widget);
    if (menu && (index < 0 || index >= WIN_SOFTMENU_MAX_ITEMS)) {
        hpclang_win_fail(EINVAL);
        return NULL;
    }
    return menu;
}
void win_softmenu_set_item(win_widget_t *widget, int index, char *label, int event, void *extra)
{
    struct softmenu *menu = slot(widget, index);
    if (!menu)
        return;
    if (!label) {
        hpclang_win_fail(EINVAL);
        return;
    }
    struct menu_item item = {0};
    for (unsigned i = 0; i < 5 && label[i]; ++i)
        item.label[i] = label[i];
    win_widget_get_colors(widget, &item.background, &item.foreground);
    item.disabled_background = item.background / 2;
    item.disabled_foreground = item.foreground / 2;
    item.enabled = 1;
    item.event = event;
    item.extra = extra;
    menu->items[index] = item;
    menu->occupied |= 1u << index;
    win_repaint();
}
int win_softmenu_add_item(win_widget_t *widget, char *label, int event, void *extra)
{
    struct softmenu *menu = data(widget);
    if (!menu)
        return -1;
    for (int i = 0; i < WIN_SOFTMENU_MAX_ITEMS; ++i)
        if (!(menu->occupied & (1u << i))) {
            win_softmenu_set_item(widget, i, label, event, extra);
            return hpclang_win_error() ? -1 : i;
        }
    return -1;
}
void win_softmenu_insert_item(win_widget_t *widget, int index, char *label, int event, void *extra)
{
    struct softmenu *menu = slot(widget, index);
    if (!menu)
        return;
    if (!label) {
        hpclang_win_fail(EINVAL);
        return;
    }
    int end = index;
    while (end < WIN_SOFTMENU_MAX_ITEMS - 1 && (menu->occupied & (1u << end)))
        ++end;
    for (int i = end; i > index; --i) {
        menu->items[i] = menu->items[i - 1];
        menu->occupied = (menu->occupied & ~(1u << i)) | ((menu->occupied & (1u << (i - 1))) << 1);
    }
    win_softmenu_set_item(widget, index, label, event, extra);
}
void win_softmenu_delete_item(win_widget_t *widget, int index, int pack)
{
    struct softmenu *menu = slot(widget, index);
    if (!menu)
        return;
    if (pack >= WIN_SOFTMENU_MAX_ITEMS)
        pack = WIN_SOFTMENU_MAX_ITEMS - 1;
    while (index < pack) {
        menu->items[index] = menu->items[index + 1];
        menu->occupied =
            (menu->occupied & ~(1u << index)) | ((menu->occupied & (1u << (index + 1))) >> 1);
        ++index;
    }
    menu->occupied &= ~(1u << index);
    win_repaint();
}
void win_softmenu_set_colors(win_widget_t *widget, int index, unsigned char fg, unsigned char bg)
{
    struct softmenu *menu = slot(widget, index);
    if (menu) {
        menu->items[index].foreground = fg;
        menu->items[index].background = bg;
        win_repaint();
    }
}
void win_softmenu_set_disable_colors(win_widget_t *widget, int index, unsigned char fg,
                                     unsigned char bg)
{
    struct softmenu *menu = slot(widget, index);
    if (menu) {
        menu->items[index].disabled_foreground = fg;
        menu->items[index].disabled_background = bg;
        win_repaint();
    }
}
void win_softmenu_set_enabled(win_widget_t *widget, int index, int enabled)
{
    struct softmenu *menu = slot(widget, index);
    if (menu) {
        menu->items[index].enabled = enabled;
        win_repaint();
    }
}
static int key(void *event, void *context)
{
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    if (!event) {
        hpclang_win_fail(EINVAL);
        return 0;
    }
    const win_keytyped_t *pressed = event;
    struct softmenu *menu = data(widget);
    if (pressed->alpha)
        return 0;
    if (pressed->keycode == KB_L) {
        if (pressed->rshift)
            return 0;
        if (pressed->lshift)
            menu->start = menu->start ? menu->start - 6 : 24;
        else {
            menu->start += 6;
            if (menu->start >= WIN_SOFTMENU_MAX_ITEMS || !(menu->occupied >> menu->start))
                menu->start = 0;
        }
        win_repaint();
        return 1;
    }
    const unsigned keys[] = {KB_A, KB_B, KB_C, KB_D, KB_E, KB_F};
    for (unsigned i = 0; i < 6; ++i)
        if (pressed->keycode == keys[i]) {
            unsigned index = menu->start + i;
            const struct menu_item *item = &menu->items[index];
            if (!(menu->occupied & (1u << index)) || !item->enabled)
                return 0;
            win_softmenu_event_t payload = {pressed->lshift, pressed->rshift, item->extra};
            return win_post_event(item->event, &payload);
        }
    return 0;
}
static int paint(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct softmenu *menu = data(widget);
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int saved = errno;
    errno = 0;
    hpg_clip_reset(NULL);
    int bottom = hpg_get_height(NULL) - 1;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned index = menu->start + i;
        if (!(menu->occupied & (1u << index)))
            continue;
        struct menu_item *item = &menu->items[index];
        int x = (int)i * 22;
        hpg_set_color(NULL, item->enabled ? item->background : item->disabled_background);
        hpg_fill_rect(x, bottom - 6, x + 20, bottom);
        hpg_set_color(NULL, item->enabled ? item->foreground : item->disabled_foreground);
        hpg_draw_text(item->label, x + (22 - (int)strlen(item->label) * 4) / 2, bottom - 5);
    }
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static void install(win_widget_t *widget)
{
    win_add_event_handler(WIN_KEY_TYPED, 1000000000, key, widget);
    win_add_event_handler(WIN_REPAINT_EVENT, 2000000000, paint, widget);
}
static void deinstall(win_widget_t *widget)
{
    win_remove_event_handler(WIN_KEY_TYPED, key, widget);
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    (void)widget;
    const struct hpclang_device_info *device = hpclang_device_info();
    *width = (int)device->display_width;
    *height = (int)device->display_height;
    if (!*width || !*height)
        hpclang_win_fail(ENODEV);
}
win_widget_t *win_softmenu_new(void)
{
    struct softmenu *menu = calloc(1, sizeof(*menu));
    if (!menu) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred, 0, menu);
    if (widget)
        win_widget_set_transparent(widget, 1);
    else
        free(menu);
    return widget;
}
