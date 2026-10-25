#ifndef HPCLANG_WINCHECKBOX_H
#define HPCLANG_WINCHECKBOX_H
#include <winwidget.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Label storage is borrowed and must remain valid through replacement or
     * destruction. A bound soft menu must outlive the binding; unbind with NULL
     * before destroying it. Focus installs a CHK action at the bound index.
     * Rebinding a focused control moves that action immediately. Selection
     * setters retain historical behavior: no repaint or repack is implied.
     */
    win_widget_t *win_checkbox_new(char *label);
    char *win_checkbox_get_label(win_widget_t *);
    void win_checkbox_set_label(win_widget_t *, char *label);
    int win_checkbox_is_selected(win_widget_t *);
    void win_checkbox_set_selected(win_widget_t *, int selected);
    void win_checkbox_set_menu(win_widget_t *, win_widget_t *menu, int index);
#ifdef __cplusplus
}
#endif
#endif
