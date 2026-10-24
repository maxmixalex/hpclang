#include <wincore.h>
#include <winprogress.h>
#include <wintext.h>
#include <string.h>

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
    win_widget_t *slider = win_progress_new(1);
    if (!slider)
        return 2;
    win_progress_set_range(slider, -10, 10);
    win_progress_set_inc(slider, 0.5);
    win_progress_set_value(slider, 3.5);
    win_progress_set_orient(slider, WIN_PROGRESS_VERT_TOP);
    win_progress_set_delay(slider, 400, 80);
    win_progress_set_event(slider, -1);
    win_widget_pack(slider);
    win_add_widget(slider);
    double value = win_progress_get_value(slider);
    win_remove_widget(slider);
    win_widget_destroy(slider);
    char title[] = "Text entry", initial[] = "Hello";
    win_widget_t *label = win_text_new_label(title);
    win_widget_t *entry = win_text_new_entry(12);
    if (!label || !entry) {
        if (label)
            win_widget_destroy(label);
        if (entry)
            win_widget_destroy(entry);
        return 3;
    }
    win_text_set_text(entry, initial);
    win_add_widget(label);
    win_add_widget(entry);
    int correct = !strcmp(win_text_get_text(entry), initial);
    win_remove_widget(entry);
    win_remove_widget(label);
    win_widget_destroy(entry);
    win_widget_destroy(label);
    return !focused || !destroyed || !correct || value != 3.5 || hpclang_win_error();
}
