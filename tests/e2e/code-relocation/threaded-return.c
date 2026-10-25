/* Offline original-ROM continuation fixture; not public registration. */
#include "rpl-mutation.h"
#include "stage-stack.h"
#include <stddef.h>

struct fixture
{
    uint8_t *nibbles;
    uint32_t writes;
};
static int read_nibble(void *context, uint32_t at, uint8_t *out)
{
    struct fixture *f = context;
    if (at < 0x80000 || at >= 0x100000)
        return 0;
    *out = f->nibbles[at - 0x80000];
    return 1;
}
static int write_nibble(void *context, uint32_t at, uint8_t value)
{
    struct fixture *f = context;
    if (at < 0x80000 || at >= 0x100000)
        return 0;
    f->nibbles[at - 0x80000] = value;
    ++f->writes;
    return 1;
}
static int stage_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    if (!width || width > 5 || at > 0x100000u - width)
        return 0;
    *value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t nibble;
        if (!read_nibble(context, at + i, &nibble))
            return 0;
        *value |= (uint32_t)nibble << (4 * i);
    }
    return 1;
}

int bank_catalog_grow(uint8_t *nibbles, uint32_t *root, uint32_t *entry, uint32_t position,
                      unsigned mode, unsigned cursor, uint32_t *writes)
{
    struct fixture f = {nibbles, 0};
    uint32_t stack[8], *discovered[8], *caller = root;
    if (cursor >= 8)
        return -1;
    for (unsigned i = 0; i < 8; ++i)
        stack[i] = 0xa1000 + 0x101 * i;
    stack[(cursor + 7) & 7] = *root;
    if (entry && *entry >= 0x80000) {
        const struct hpclang_stage_stack_io stage = {&f, stage_word, NULL};
        if (hpclang_stage_return_roots(&stage, *entry, stack, cursor, discovered) != 1 ||
            discovered[0] != stack + ((cursor + 7) & 7))
            return -2;
        caller = discovered[0];
    }
    uint32_t *roots[2];
    unsigned count = 0;
    if (mode != 1)
        roots[count++] = caller;
    if (entry && mode != 2)
        roots[count++] = entry;
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    struct hpclang_rpl_mutation_io io = {read_nibble, write_nibble, &f,      0x100000, 0x80000, 0,
                                         roots,       count,        0x8611d, display,  4};
    int result = hpclang_rpl_mutation_catalog_grow_repaired(&io, position, NULL);
    *root = *caller;
    *writes = f.writes;
    return result;
}
