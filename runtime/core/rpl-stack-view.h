#ifndef HPCLANG_RUNTIME_RPL_STACK_VIEW_H
#define HPCLANG_RUNTIME_RPL_STACK_VIEW_H

#include <stdint.h>

#include "rpl-object-span.h"

#ifdef __cplusplus
extern "C"
{
#endif

    enum hpclang_rpl_stack_result
    {
        HPCLANG_RPL_STACK_OK = 0,
        HPCLANG_RPL_STACK_UNAVAILABLE = -1,
        HPCLANG_RPL_STACK_MALFORMED = -2,
        HPCLANG_RPL_STACK_LEVEL = -3
    };

    /*
     * Observe the Stock RPL stack without mutation.
     *
     * DSKTOP stores D1, the Saturn address of level 1's five-nibble pointer cell.
     * EDITLINE points one cell beyond the bottom-of-stack marker. Therefore:
     *
     *   depth = ([EDITLINE] - [DSKTOP]) / 5 - 1
     *
     * A malformed/unmapped Stock state is rejected instead of being normalized.
     */
    int hpclang_rpl_stack_depth(const struct hpclang_rpl_reader *reader, uint32_t *depth);

    typedef int (*hpclang_rpl_stack_inspect_fn)(const struct hpclang_rpl_reader *, uint32_t address,
                                                uint32_t prologue, uint32_t first, uint32_t end);

    /* Return 1 if a stack slot or either field of its top-level ACPTR points into [first, end), 0
     * if none do, or a negative error. Includes launcher-private levels, excludes the bottom
     * marker. An optional inspector checks the decoded object for additional references
     * and returns 1 for overlap, zero to continue, or a negative error. */
    int hpclang_rpl_stack_reference_overlap(const struct hpclang_rpl_reader *reader, uint32_t first,
                                            uint32_t end, hpclang_rpl_stack_inspect_fn inspect);

    /* Resolve a 1-based stack level to its Saturn object address. */
    int hpclang_rpl_stack_object_address(const struct hpclang_rpl_reader *reader, uint32_t level,
                                         uint32_t *object_address);

#ifdef __cplusplus
}
#endif

#endif
