#ifndef HPCLANG_WIN_BOX_AXIS_H
#define HPCLANG_WIN_BOX_AXIS_H
#include <stddef.h>
struct hpclang_win_box_cell
{
    int preferred;
    double weight;
};
struct hpclang_win_box_extent
{
    int position, size;
};
/* Checked legacy axis allocation. Zero-weight cells retain their natural size.
 * Deficits consume padding first. Historical signed (share + .5) truncation is
 * retained, so a deficit may leave a one-pixel overhang. Output is transactional.
 */
int hpclang_win_box_axis(const struct hpclang_win_box_cell *, size_t count, int padding,
                         int available, int origin, struct hpclang_win_box_extent *output);
/* Alignment uses the historical -2 fill, -1 leading, 0 center, 1 trailing. */
int hpclang_win_box_align(struct hpclang_win_box_extent cell, int preferred, int alignment,
                          struct hpclang_win_box_extent *output);
#endif
