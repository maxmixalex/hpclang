#ifndef HPCLANG_WINCORE_H
#define HPCLANG_WINCORE_H
#include <winwidget.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* All four callbacks are required. Allocation immediately calls preferred-size
     * with the supplied data available. Destroy releases application data through
     * its callback, then frees the descriptor; the callback must not free it.
     * Negative sizes and overflowing right/bottom coordinates are rejected.
     */
    win_widget_t *win_wcore_alloc(void (*install)(win_widget_t *),
                                  void (*deinstall)(win_widget_t *),
                                  void (*destroy)(win_widget_t *),
                                  void (*preferred_size)(win_widget_t *, int *, int *),
                                  int focusable, void *data);
    void *win_wcore_get_data(win_widget_t *);
    void win_wcore_pref_size(win_widget_t *, int *width, int *height);
    /* Historical painting order: clip, optional background using the preceding
     * pattern/mode, then set foreground/font, solid pattern and PAINT mode.
     * The clip excludes x+w and y+h; the fill uses those same endpoints, preserving
     * the original HPG clipping convention.
     */
    void win_wcore_paint_base(win_widget_t *);
    void win_wcore_notify_focus(win_widget_t *, void (*lost)(win_widget_t *),
                                void (*gained)(win_widget_t *));
#ifdef __cplusplus
}
#endif
#endif
