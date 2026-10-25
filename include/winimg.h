#ifndef HPCLANG_WINIMG_H
#define HPCLANG_WINIMG_H
#include <winwidget.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Pattern and image are borrowed until the widget is destroyed. Either may
     * be NULL. Preferred size comes from the image (zero for pattern-only).
     * Painting fills the pattern first, then blits the image within the clip.
     */
    win_widget_t *win_img_new(hpg_pattern_t *pattern, hpg_t *image);
#ifdef __cplusplus
}
#endif
#endif
