#include <wincore.h>
#include <winprogress.h>
#include <wintext.h>
#include <winimg.h>
#include <winsoftmenu.h>
#include <wincheckbox.h>
#include <winchoose.h>
#include <winbox.h>
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
    win_widget_t *picture = win_img_new(NULL, NULL);
    if (!picture)
        return 4;
    win_add_widget(picture);
    win_remove_widget(picture);
    win_widget_destroy(picture);
    win_widget_t *menu = win_softmenu_new();
    if (!menu)
        return 5;
    char action[] = "Close";
    win_softmenu_add_item(menu, action, -1, NULL);
    win_softmenu_insert_item(menu, 0, action, -1, NULL);
    win_softmenu_set_item(menu, 6, action, -1, NULL);
    win_softmenu_set_colors(menu, 0, 0, 255);
    win_softmenu_set_disable_colors(menu, 0, 0, 119);
    win_softmenu_set_enabled(menu, 0, 0);
    win_softmenu_delete_item(menu, 0, 29);
    win_add_widget(menu);
    win_widget_t *check = win_checkbox_new(action);
    if (!check)
        return 6;
    win_checkbox_set_label(check, action);
    win_checkbox_set_selected(check, 1);
    win_checkbox_set_menu(check, menu, 0);
    win_add_widget(check);
    int checked = win_checkbox_is_selected(check) && win_checkbox_get_label(check) == action;
    win_remove_widget(check);
    win_widget_destroy(check);
    win_widget_t *choice = win_choose_new();
    if (!choice)
        return 7;
    win_choose_add(choice, action);
    win_choose_add_with_label(choice, initial, title);
    win_choose_set_choice(choice, initial);
    int chosen = win_choose_get_index(choice) == 1 && win_choose_get_choice(choice) == initial;
    win_choose_set_index(choice, 0);
    win_choose_remove(choice, initial);
    win_choose_set_menu(choice, menu, 0);
    win_add_widget(choice);
    win_remove_widget(choice);
    win_widget_destroy(choice);
    win_remove_widget(menu);
    win_widget_destroy(menu);
    win_widget_t *group = win_box_new_padding(2, 2), *nested = win_box_new();
    win_widget_t *group_label = win_text_new_label(action), *group_check = win_checkbox_new(action);
    if (!group || !nested || !group_label || !group_check)
        return 8;
    win_box_add(group, nested);
    win_box_add_at(nested, group_label, 0, 0);
    win_box_add_with(nested, group_check, 0, 1, 1, 1, -1, -1, WIN_BOX_FILL, WIN_BOX_CENTER);
    win_widget_pack(nested);
    win_widget_pack(group);
    win_box_layout(group);
    win_box_layout(nested);
    win_add_widget(group);
    int grouped = win_widget_has_focus(group_check);
    win_remove_widget(group);
    win_box_remove(group, nested);
    win_widget_destroy(group);
    win_widget_destroy(nested);
    win_widget_destroy(group_label);
    win_widget_destroy(group_check);
    return !focused || !destroyed || !correct || !checked || !chosen || !grouped || value != 3.5 ||
           hpclang_win_error();
}
