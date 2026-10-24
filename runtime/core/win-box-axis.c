#include "win-box-axis.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct working_cell
{
    struct hpclang_win_box_extent extent;
    double remaining_weight;
};
static int fail(int error)
{
    errno = error;
    return -1;
}
int hpclang_win_box_axis(const struct hpclang_win_box_cell *cells, size_t count, int padding,
                         int available, int origin, struct hpclang_win_box_extent *output)
{
    if (padding < 0 || available < 0 || (count && (!cells || !output)))
        return fail(EINVAL);
    if (count > INT_MAX || count > SIZE_MAX / sizeof(struct working_cell))
        return fail(EOVERFLOW);
    if (!count)
        return 0;
    int64_t natural = 0;
    for (size_t i = 0; i < count; ++i) {
        if (cells[i].preferred < 0 || !isfinite(cells[i].weight) || cells[i].weight < 0)
            return fail(EINVAL);
        natural += cells[i].preferred;
        if (natural > INT_MAX)
            return fail(EOVERFLOW);
    }
    int64_t gaps = (int64_t)(count - 1) * padding;
    if (natural + gaps > INT_MAX)
        return fail(EOVERFLOW);
    struct working_cell *work = malloc(count * sizeof(*work));
    if (!work)
        return fail(ENOMEM);
    double weight = 0;
    for (size_t i = count; i; --i) {
        weight += cells[i - 1].weight;
        if (!isfinite(weight)) {
            free(work);
            return fail(EOVERFLOW);
        }
        /* Recompute suffix sums instead of subtracting a dominant weight and
         * losing a very small remaining weight to floating point cancellation. */
        work[i - 1].remaining_weight = weight;
    }
    int64_t surplus = (int64_t)available - natural - gaps;
    if (surplus < 0) {
        int64_t reduction = -surplus < gaps ? -surplus : gaps;
        gaps -= reduction;
        surplus += reduction;
    }
    int64_t position = origin;
    for (size_t i = 0; i < count; ++i) {
        double remaining = work[i].remaining_weight;
        /* Divide weights before multiplication to avoid a finite large weight
         * overflowing the intermediate product. No 0/0 for fixed/empty cells. */
        double share = remaining ? (double)surplus * (cells[i].weight / remaining) : 0;
        int64_t delta = (int64_t)(share + 0.5);
        if (delta < -(int64_t)cells[i].preferred)
            delta = -(int64_t)cells[i].preferred;
        int64_t size = cells[i].preferred + delta;
        if (size > INT_MAX || position < INT_MIN || position > INT_MAX ||
            position + size > INT_MAX) {
            free(work);
            return fail(EOVERFLOW);
        }
        work[i].extent = (struct hpclang_win_box_extent){(int)position, (int)size};
        surplus -= delta;
        position += size;
        if (i + 1 < count) {
            int64_t gap = gaps / (int64_t)(count - i - 1);
            position += gap;
            gaps -= gap;
        }
    }
    for (size_t i = 0; i < count; ++i)
        output[i] = work[i].extent;
    free(work);
    return 0;
}
int hpclang_win_box_align(struct hpclang_win_box_extent cell, int preferred, int alignment,
                          struct hpclang_win_box_extent *output)
{
    if (!output || cell.size < 0 || preferred < 0 || alignment < -2 || alignment > 1)
        return fail(EINVAL);
    int size = preferred < cell.size ? preferred : cell.size;
    int extra = cell.size - size;
    int64_t position = cell.position;
    if (alignment == -2)
        size = cell.size;
    else if (alignment == 0)
        position += extra / 2;
    else if (alignment == 1)
        position += extra;
    if (position > INT_MAX || position + size > INT_MAX)
        return fail(EOVERFLOW);
    *output = (struct hpclang_win_box_extent){(int)position, size};
    return 0;
}
