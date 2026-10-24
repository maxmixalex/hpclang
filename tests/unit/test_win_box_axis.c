#undef malloc
#include <stdlib.h>
#include "win-box-axis.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
static int fail_allocation;
void *hpclang_test_malloc(size_t size)
{
    return fail_allocation ? NULL : malloc(size);
}
static void extent(struct hpclang_win_box_extent value, int position, int size)
{
    assert(value.position == position && value.size == size);
}
int main(void)
{
    struct hpclang_win_box_cell cells[] = {{10, 1}, {20, 2}, {0, 0}};
    struct hpclang_win_box_extent out[3];
    assert(!hpclang_win_box_axis(cells, 3, 2, 64, 5, out));
    extent(out[0], 5, 20);
    extent(out[1], 27, 40);
    extent(out[2], 69, 0);
    assert(!hpclang_win_box_axis(cells, 2, 4, 32, 0, out));
    extent(out[0], 0, 10);
    extent(out[1], 12, 20); /* deficit removed from padding */
    assert(!hpclang_win_box_axis(cells, 2, 4, 20, 0, out));
    extent(out[0], 0, 8);
    extent(out[1], 8, 13); /* legacy negative rounding */
    cells[0].weight = cells[1].weight = 0;
    assert(!hpclang_win_box_axis(cells, 3, 2, 64, 0, out));
    extent(out[0], 0, 10);
    extent(out[1], 12, 20);
    extent(out[2], 34, 0);
    cells[0].weight = DBL_MAX;
    cells[1].weight = DBL_MIN;
    assert(!hpclang_win_box_axis(cells, 2, 0, 60, 0, out));
    extent(out[0], 0, 40);
    extent(out[1], 40, 20);
    struct hpclang_win_box_extent before[3];
    memcpy(before, out, sizeof(out));
    cells[1].weight = DBL_MAX;
    assert(hpclang_win_box_axis(cells, 2, 0, 60, 0, out) == -1 && errno == EOVERFLOW);
    assert(!memcmp(before, out, sizeof(out)));
    cells[0].weight = 1;
    cells[1].weight = NAN;
    assert(hpclang_win_box_axis(cells, 2, 0, 60, 0, out) == -1 && errno == EINVAL);
    cells[1].weight = 1;
    fail_allocation = 1;
    assert(hpclang_win_box_axis(cells, 2, 0, 60, 0, out) == -1 && errno == ENOMEM);
    fail_allocation = 0;
    assert(hpclang_win_box_axis(cells, 2, 0, 60, INT_MAX - 2, out) == -1 && errno == EOVERFLOW);
    assert(!memcmp(before, out, sizeof(out)));
    assert(!hpclang_win_box_axis(NULL, 0, 0, 0, 0, NULL));
    assert(hpclang_win_box_axis(cells, 2, -1, 60, 0, out) == -1 && errno == EINVAL);
    struct hpclang_win_box_extent cell = {-5, 20}, result;
    for (int align = -2; align <= 1; ++align) {
        assert(!hpclang_win_box_align(cell, 8, align, &result));
        extent(result, align == 0 ? 1 : align == 1 ? 7 : -5, align == -2 ? 20 : 8);
    }
    assert(!hpclang_win_box_align(cell, 40, 0, &result));
    extent(result, -5, 20);
    assert(hpclang_win_box_align(cell, 1, 2, &result) == -1 && errno == EINVAL);
    return 0;
}
