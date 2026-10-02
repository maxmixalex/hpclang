#ifndef HPCLANG_HP39_PORTAL_STACK_H
#define HPCLANG_HP39_PORTAL_STACK_H
#include <stdint.h>

/* Stack shape is relative to EDITLINE: Stock may relocate both ends while
 * storing a variable. A trapped native error can leave private operands. */
static inline int hpclang_hp39_portal_stack_plan(uint32_t before_span, uint32_t top,
                                                 uint32_t editline, uint32_t *restored_top,
                                                 uint32_t *dropped)
{
    if (before_span < 5u || before_span % 5u != 0u || top < 0x80000u || editline > 0x100000u ||
        editline < top)
        return -10;
    uint32_t span = editline - top;
    if (span == before_span)
        return -8;
    if (span < before_span || (span - before_span) % 5u != 0u)
        return -10;
    *restored_top = editline - before_span;
    *dropped = (span - before_span) / 5u;
    return 0;
}
#endif
