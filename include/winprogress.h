#ifndef HPCLANG_WINPROGRESS_H
#define HPCLANG_WINPROGRESS_H
#include <winwidget.h>
#define WIN_PROGRESS_HORIZ 0
#define WIN_PROGRESS_VERT 1
#define WIN_PROGRESS_HORIZ_RIGHT 2
#define WIN_PROGRESS_VERT_TOP 3
#ifdef __cplusplus
extern "C"
{
#endif
    /* Nonzero slider enables focus. Defaults: range 0..100, value 0, increment
     * 10, horizontal, repeat delays 1000/200 ms. Setters do not repack the widget.
     * Finite bounds/increment are required, with a positive increment and finite
     * range span. Reversed/equal bounds preserve the range, matching HPGCC.
     * Values clamp at the bounds; NaN is rejected. Delays must be positive.
     * Changes from keys/repeat post the configured event with the widget as data;
     * setters alone do not post it. Default event is WIN_IDLE_EVENT; -1 disables.
     */
    win_widget_t *win_progress_new(int slider);
    void win_progress_set_value(win_widget_t *, double value);
    double win_progress_get_value(win_widget_t *);
    void win_progress_set_inc(win_widget_t *, double increment);
    void win_progress_set_orient(win_widget_t *, int orientation);
    void win_progress_set_range(win_widget_t *, double minimum, double maximum);
    void win_progress_set_delay(win_widget_t *, int first, int next);
    void win_progress_set_event(win_widget_t *, int event);
#ifdef __cplusplus
}
#endif
#endif
