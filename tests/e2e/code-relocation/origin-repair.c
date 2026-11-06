/* Offline trace-qualified source repair; not production source discovery. */
#include "threaded-return.c"
#include "rpl-code.h"

static int resolve_origin(void *context, uint32_t token, struct hpclang_rpl_code_origin_cell *cell)
{
    uint8_t value;
    if (!token || !read_nibble(context, token - 1, &value))
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    *cell = (struct hpclang_rpl_code_origin_cell){1, token - 1, value};
    return 0;
}

int bank_catalog_grow_origins(uint8_t *nibbles, uint32_t *root, uint32_t *entry, uint32_t position,
                              unsigned cursor, uint32_t *writes, const uint32_t *tokens,
                              const uint32_t *values)
{
    struct fixture f = {nibbles, 0};
    *writes = 0;
    uint32_t stack[8], *discovered[8], *caller = root;
    if (cursor >= 8)
        return -1;
    for (unsigned i = 0; i < 8; ++i)
        stack[i] = 0xa1000 + 0x101 * i;
    stack[(cursor + 7) & 7] = *root;
    if (*entry >= 0x80000) {
        const struct hpclang_stage_stack_io stage = {&f, stage_word, NULL};
        if (hpclang_stage_return_roots(&stage, *entry, stack, cursor, discovered) != 1 ||
            discovered[0] != stack + ((cursor + 7) & 7))
            return -2;
        caller = discovered[0];
    }
    struct hpclang_rpl_reference_edit edits[10];
    unsigned first, second;
    int result = hpclang_rpl_code_origins_plan(tokens, values[0], values[0] + 16, resolve_origin,
                                               &f, edits, &first);
    if (!result)
        result = hpclang_rpl_code_origins_plan(tokens + 5, values[1], values[1], resolve_origin, &f,
                                               edits + first, &second);
    if (result)
        return result;
    uint32_t *roots[] = {caller, entry};
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    struct hpclang_rpl_mutation_io io = {
        read_nibble, write_nibble, &f, 0x100000, 0x80000, 0, roots, 2, 0x8611d, display, 4, NULL};
    const struct hpclang_rpl_catalog_repairs repairs = {
        .main_mapping = 1, .edits = edits, .edit_count = first + second};
    result = hpclang_rpl_mutation_catalog_grow_repaired(&io, position, &repairs);
    *root = *caller;
    *writes = f.writes;
    return result;
}
