/* Offline original-ROM continuation fixture; not public registration. */
#include "rpl-mutation.h"
#include "rpl-reference.h"
#include "stage-stack.h"
#include <stddef.h>

struct fixture
{
    uint8_t *nibbles;
    uint32_t writes;
};

struct allocation_query
{
    uint32_t address, extent;
};

static int allocation_match(void *context, uint32_t first, uint32_t nibbles)
{
    struct allocation_query *query = context;
    if (first == query->address)
        query->extent = nibbles;
    return 0;
}

static int reject_write(void *context, uint32_t at, uint8_t value)
{
    (void)context;
    (void)at;
    (void)value;
    return 0;
}

/* Read-only fixture query: validate the entire TEMP chain before qualifying an
 * exact allocation start. Containment or matching bytes are not sufficient. */
int bank_temp_allocation(const struct hpclang_rpl_reader *reader, uint32_t address,
                         uint32_t *extent)
{
    if (!reader || !extent)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    struct hpclang_rpl_mutation_io io = {.read_nibble = reader->read_nibble,
                                         .write_nibble = reject_write,
                                         .context = reader->context,
                                         .temp_limit = 0x100000,
                                         .user_limit = 0x80000};
    struct allocation_query query = {address, 0};
    int result = hpclang_rpl_mutation_visit_temp(&io, allocation_match, &query);
    if (result)
        return result;
    if (!query.extent)
        return HPCLANG_RPL_MUTATION_NOT_FOUND;
    *extent = query.extent;
    return 0;
}
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

/* Controlled alternate command-4 return: caller proves R0's low address use.
 * This fixture supplies that obligation; it does not discover production roots.
 */
int bank_catalog_grow_r0(uint8_t *nibbles, uint32_t *entry, uint32_t *r0, unsigned repair_r0,
                         uint32_t *writes)
{
    struct fixture f = {nibbles, 0};
    uint32_t *roots[] = {entry};
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    struct hpclang_rpl_mutation_io io = {
        read_nibble, write_nibble, &f, 0x100000, 0x80000, 0, roots, 1, 0x8611d, display, 4};
    const struct hpclang_rpl_register_repair reg = {r0, *r0};
    const struct hpclang_rpl_catalog_repairs repairs = {
        .main_mapping = 1,
        .registers = repair_r0 ? &reg : NULL,
        .register_count = repair_r0 ? 1u : 0u,
    };
    int result = hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x86500, &repairs);
    *writes = f.writes;
    return result;
}

/* Controlled DUP continuation: A.A and R1.A point inside one TEMP allocation.
 * The mask deliberately omits roots in negative controls; no discovery claim.
 */
int bank_catalog_grow_dispatch_guarded(uint8_t *nibbles, uint32_t *a, uint32_t *r1, unsigned mask,
                                       uint32_t *writes,
                                       const struct hpclang_rpl_reference_reader *guard_reader,
                                       const struct hpclang_rpl_reference_guard *guards,
                                       uint32_t guard_count)
{
    struct fixture f = {nibbles, 0};
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    struct hpclang_rpl_mutation_io io = {
        read_nibble, write_nibble, &f, 0x100000, 0x80000, 0, NULL, 0, 0x8611d, display, 4};
    struct hpclang_rpl_register_repair registers[2];
    unsigned count = 0;
    if (mask & 1u)
        registers[count++] = (struct hpclang_rpl_register_repair){a, *a};
    if (mask & 2u)
        registers[count++] = (struct hpclang_rpl_register_repair){r1, *r1};
    const struct hpclang_rpl_catalog_repairs repairs = {.main_mapping = 1,
                                                        .registers = registers,
                                                        .register_count = count,
                                                        .guard_reader = guard_reader,
                                                        .guards = guards,
                                                        .guard_count = guard_count};
    uint32_t libraries;
    if (!stage_word(&f, io.library_table, 3, &libraries))
        return -1;
    int result = hpclang_rpl_mutation_catalog_grow_repaired(
        &io, io.library_table + 3u + libraries * 16u, &repairs);
    *writes = f.writes;
    return result;
}

int bank_catalog_grow_dispatch(uint8_t *nibbles, uint32_t *a, uint32_t *r1, unsigned mask,
                               uint32_t *writes)
{
    return bank_catalog_grow_dispatch_guarded(nibbles, a, r1, mask, writes, NULL, NULL, 0);
}
