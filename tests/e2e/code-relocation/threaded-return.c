/* Offline original-ROM continuation fixture; not public registration. */
#include "rpl-mutation.h"
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
int bank_catalog_grow(uint8_t *nibbles, uint32_t *root, uint32_t position, unsigned mode,
                      uint32_t *writes)
{
    struct fixture f = {nibbles, 0};
    uint32_t *roots[] = {root};
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    struct hpclang_rpl_mutation_io io = {
        read_nibble, write_nibble,      &f,      0x100000, 0x80000, 0,
        roots,       mode == 1 ? 0 : 1, 0x8611d, display,  4};
    int result = hpclang_rpl_mutation_catalog_grow_repaired(&io, position, NULL);
    *writes = f.writes;
    return result;
}
