#include <assert.h>
#include "hp39-portal-stack.h"

int main(void)
{
    uint32_t top = 0u, count = 0u;
    /* Original stack: top=0x90000, editline=0x9000f. A store has moved
     * USEROB downward by 23 nibbles and left a value and success marker. */
    assert(hpclang_hp39_portal_stack_plan(15u, 0x90000u - 23u - 10u, 0x9000fu - 23u, &top,
                                          &count) == 0);
    assert(top == 0x90000u - 23u && count == 2u);
    /* Shrinkage can move the stack upward, including after an error leaves
     * the value/ID operands plus the error number. */
    assert(hpclang_hp39_portal_stack_plan(15u, 0x90000u + 41u - 15u, 0x9000fu + 41u, &top,
                                          &count) == 0);
    assert(top == 0x90000u + 41u && count == 3u);
    assert(hpclang_hp39_portal_stack_plan(15u, 0x90000u, 0x9000fu, &top, &count) == -8);
    assert(hpclang_hp39_portal_stack_plan(15u, 0x90000u, 0x90010u, &top, &count) == -10);
    assert(hpclang_hp39_portal_stack_plan(15u, 0x90000u, 0x8ffffu, &top, &count) == -10);
    assert(hpclang_hp39_portal_stack_plan(15u, 0x7ffffu, 0x80010u, &top, &count) == -10);
    assert(hpclang_hp39_portal_stack_plan(15u, 0xffff0u, 0x100001u, &top, &count) == -10);
    assert(hpclang_hp39_portal_stack_plan(0u, 0x90000u, 0x90005u, &top, &count) == -10);
    /* Rejected shapes never publish a cleanup target. */
    assert(top == 0x90000u + 41u && count == 3u);
    return 0;
}
