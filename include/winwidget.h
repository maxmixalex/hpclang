#ifndef HPCLANG_WINWIDGET_H
#define HPCLANG_WINWIDGET_H
#include <hpgraphics.h>
#include <winevent.h>
#define WIN_REPAINT_EVENT 7
#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct win_widget win_widget_t;
    /* Widgets own their descriptor, but borrow their font and application data.
     * Remove before destroy, and destroy only after event/timer dispatch returns:
     * existing synchronous snapshots can still contain the removed widget.
     * Reset rejects installed widgets. Cleanup/removal remains available after a
     * latched window error. Lifecycle callback mutation of the focus ring reports
     * EBUSY; destroying from any lifecycle/event/timer callback also reports EBUSY.
     * Invalid arguments and allocation errors use the window error latch.
     */
    void win_widget_destroy(win_widget_t *);
    void win_add_widget(win_widget_t *);
    void win_remove_widget(win_widget_t *);
    void win_focus_next(void);
    void win_focus_prev(void);
    int win_widget_has_focus(win_widget_t *);
    void win_widget_set_location(win_widget_t *, int x, int y);
    void win_widget_get_location(win_widget_t *, int *x, int *y);
    void win_widget_set_size(win_widget_t *, int width, int height);
    void win_widget_get_size(win_widget_t *, int *width, int *height);
    void win_widget_pack(win_widget_t *);
    void win_widget_set_colors(win_widget_t *, unsigned char foreground, unsigned char background);
    void win_widget_get_colors(win_widget_t *, unsigned char *foreground,
                               unsigned char *background);
    void win_widget_set_font(win_widget_t *, hpg_font_t *);
    hpg_font_t *win_widget_get_font(win_widget_t *);
    void win_widget_set_transparent(win_widget_t *, unsigned value);
#ifdef __cplusplus
}
#endif
#endif
