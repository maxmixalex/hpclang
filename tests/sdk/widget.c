#include <wincore.h>

static int paint(void *event, void *data)
{
    (void)event;
    win_widget_t *widget = (win_widget_t *)data;
    win_wcore_paint_base(widget);
    return 0;
}
static void install(win_widget_t *widget)
{
    win_add_event_handler(WIN_REPAINT_EVENT, 0, paint, widget);
}
static void remove_widget(win_widget_t *widget)
{
    win_remove_event_handler(WIN_REPAINT_EVENT, paint, widget);
}
static void destroy(win_widget_t *widget)
{
    int *destroyed = (int *)win_wcore_get_data(widget);
    *destroyed = 1;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    (void)widget;
    *width = 20;
    *height = 10;
}
int main(void)
{
    int destroyed = 0;
    win_widget_t *widget =
        win_wcore_alloc(install, remove_widget, destroy, preferred, 1, &destroyed);
    if (!widget)
        return 1;
    win_widget_set_location(widget, 4, 8);
    win_widget_set_colors(widget, HPG_COLOR_BLACK, HPG_COLOR_GRAY_4);
    win_add_widget(widget);
    win_focus_next();
    win_focus_prev();
    int focused = win_widget_has_focus(widget);
    win_remove_widget(widget);
    win_widget_destroy(widget);
    return !focused || !destroyed || hpclang_win_error();
}
