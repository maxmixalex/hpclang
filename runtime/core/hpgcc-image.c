#include <winimg.h>
#include <wincore.h>
#include "win-internal.h"
#include <errno.h>
#include <stdlib.h>

struct image_control
{
    hpg_pattern_t *pattern;
    hpg_t *image;
};
static int paint(void *event, void *context)
{
    (void)event;
    win_widget_t *widget = context;
    if (!hpclang_win_widget_installed(widget))
        return 0;
    struct image_control *image = win_wcore_get_data(widget);
    win_wcore_paint_base(widget);
    if (hpclang_win_error())
        return 0;
    int x, y, width, height;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &width, &height);
    int saved = errno;
    errno = 0;
    if (width && height) {
        if (image->pattern) {
            hpg_set_pattern(NULL, image->pattern);
            hpg_fill_rect(x, y, x + width, y + height);
            hpg_set_pattern(NULL, NULL);
        }
        if (image->image)
            hpg_blit(image->image, 0, 0, width, height, NULL, x, y);
    }
    if (errno)
        hpclang_win_fail(errno);
    else
        errno = saved;
    return 0;
}
static void install(win_widget_t *widget)
{
    win_add_event_handler(WIN_REPAINT_EVENT, 2000000000, paint, widget);
}
static void deinstall(win_widget_t *widget)
{
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
}
static void destroy(win_widget_t *widget)
{
    free(win_wcore_get_data(widget));
}
static void preferred_size(win_widget_t *widget, int *width, int *height)
{
    struct image_control *image = win_wcore_get_data(widget);
    *width = image->image ? hpg_get_width(image->image) : 0;
    *height = image->image ? hpg_get_height(image->image) : 0;
}
win_widget_t *win_img_new(hpg_pattern_t *pattern, hpg_t *image)
{
    struct image_control *data = malloc(sizeof(*data));
    if (!data) {
        hpclang_win_fail(ENOMEM);
        return NULL;
    }
    *data = (struct image_control){pattern, image};
    win_widget_t *widget = win_wcore_alloc(install, deinstall, destroy, preferred_size, 0, data);
    if (!widget)
        free(data);
    return widget;
}
