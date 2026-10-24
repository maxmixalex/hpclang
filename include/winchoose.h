#ifndef HPCLANG_WINCHOOSE_H
#define HPCLANG_WINCHOOSE_H
#include <winwidget.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Options and labels are borrowed. The menu must outlive its binding; unbind
     * with NULL before destroying it. Focus binds CHOOS. Plain W cycles choices;
     * CHOOS opens a scrolling list, arrows move, F5 cancels and F6 confirms.
     * The modal overlay consumes keyboard input without removing the bound menu.
     * Successful list mutations, setters, rebinding and focus loss cancel a popup.
     * Setters do not otherwise repaint/repack. A negative index means no choice.
     */
    win_widget_t *win_choose_new(void);
    void win_choose_add(win_widget_t *, char *option);
    void win_choose_add_with_label(win_widget_t *, char *option, char *label);
    void win_choose_remove(win_widget_t *, char *option);
    char *win_choose_get_choice(win_widget_t *);
    int win_choose_get_index(win_widget_t *);
    void win_choose_set_choice(win_widget_t *, char *option);
    void win_choose_set_index(win_widget_t *, int index);
    void win_choose_set_menu(win_widget_t *, win_widget_t *menu, int index);
#ifdef __cplusplus
}
#endif
#endif
