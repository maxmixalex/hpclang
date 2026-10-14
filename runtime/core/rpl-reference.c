#include "rpl-reference.h"
#include "rpl-backup.h"
#include "rpl-port.h"
#include <stddef.h>

struct traversal
{
    hpclang_rpl_reference_visit_fn visit;
    void *context;
};

static int emit(struct traversal *walk, enum hpclang_rpl_reference_kind kind, uint32_t field,
                uint32_t target, uint32_t extent, uint32_t auxiliary)
{
    const struct hpclang_rpl_reference edge = {kind, field, target, extent, auxiliary};
    return walk->visit(walk->context, &edge);
}

static int sequence(void *context, uint32_t field, uint32_t target, uint32_t span)
{
    return emit(context, span ? HPCLANG_RPL_REFERENCE_INLINE : HPCLANG_RPL_REFERENCE_DIRECT, field,
                target, span, 0);
}

static int array(void *context, uint32_t index, uint32_t prologue, uint32_t body, uint32_t span)
{
    (void)index;
    return emit(context, HPCLANG_RPL_REFERENCE_COMPACT, body, body, span, prologue);
}

static int library(void *context, enum hpclang_rpl_library_reference_kind kind, uint32_t field,
                   uint32_t target, uint32_t limit)
{
    return emit(context, HPCLANG_RPL_REFERENCE_DIRECT, field, target, limit, (uint32_t)kind + 1);
}

static int word(const struct hpclang_rpl_reader *reader, uint32_t at, unsigned count,
                uint32_t *value)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result |= (uint32_t)nibble << (4 * i);
    }
    *value = result;
    return 0;
}

struct compact_reader
{
    const struct hpclang_rpl_reader *source;
    uint32_t prefix, prologue;
};

static int compact_read(void *context, uint32_t address, uint8_t *value)
{
    const struct compact_reader *body = context;
    if (address >= body->prefix && address - body->prefix < 5) {
        *value = (uint8_t)((body->prologue >> (4 * (address - body->prefix))) & 15);
        return 1;
    }
    return body->source->read_nibble(body->source->context, address, value);
}

int hpclang_rpl_reference_visit(const struct hpclang_rpl_reader *source, uint32_t address,
                                uint32_t limit, uint32_t body_prologue,
                                hpclang_rpl_reference_visit_fn visit, void *context)
{
    if (!source || !source->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct compact_reader body = {source, 0, body_prologue};
    struct hpclang_rpl_reader synthetic = {compact_read, &body};
    const struct hpclang_rpl_reader *reader = source;
    if (body_prologue) {
        if (!hpclang_rpl_object_prologue_known(body_prologue) || body_prologue == 0x2a96)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (address < 5)
            return HPCLANG_RPL_SPAN_BOUNDS;
        body.prefix = address - 5;
        address -= 5;
        limit += 5;
        reader = &synthetic;
    }
    uint32_t span, prologue;
    int result = hpclang_rpl_object_span(reader, address, limit, &span);
    if (result)
        return result;
    result = word(reader, address, 5, &prologue);
    if (result)
        return result;
    struct traversal walk = {visit, context};
    switch (prologue) {
    case 0x2b62: {
        struct hpclang_rpl_backup_view backup;
        result = hpclang_rpl_backup_view(reader, address, span, &backup);
        if (result)
            return result;
        return emit(&walk, HPCLANG_RPL_REFERENCE_INLINE, backup.value_address, backup.value_address,
                    backup.value_span, 0);
    }
    case 0x2b40:
        return hpclang_rpl_library_visit(reader, address, span, library, &walk);
    case 0x2a74: /* list */
    case 0x2ab8: /* symbolic */
    case 0x2ada: /* unit */
    case 0x2d9d: /* program */
    case 0x2686: /* matrix */
        return hpclang_rpl_sequence_visit(reader, address, span, sequence, &walk);
    case 0x2afc:
        return hpclang_rpl_tagged_visit(reader, address, span, sequence, &walk);
    case 0x29e8:
    case 0x2a0a:
        return hpclang_rpl_array_visit(reader, address, span, array, &walk);
    case 0x2baa:
    case 0x2e92:
    case 0x26ac: {
        unsigned width = prologue == 0x2baa ? 5 : 3;
        unsigned second_width = prologue == 0x26ac ? 4 : width;
        uint32_t target, auxiliary;
        result = word(reader, address + 5, width, &target);
        if (result)
            return result;
        result = word(reader, address + 5 + width, second_width, &auxiliary);
        if (result)
            return result;
        enum hpclang_rpl_reference_kind kind = prologue == 0x2baa   ? HPCLANG_RPL_REFERENCE_ACPTR
                                               : prologue == 0x2e92 ? HPCLANG_RPL_REFERENCE_ROMP
                                                                    : HPCLANG_RPL_REFERENCE_FLASHP;
        return emit(&walk, kind, address + 5, target, 0, auxiliary);
    }
    case 0x2614: /* integer */
    case 0x263a: /* long real */
    case 0x2660: /* long complex */
    case 0x26fe: /* minifont */
    case 0x2911: /* binary integer */
    case 0x2933: /* real */
    case 0x2955: /* extended real */
    case 0x2977: /* complex */
    case 0x299d: /* extended complex */
    case 0x29bf: /* character */
    case 0x2a2c: /* string */
    case 0x2b1e: /* graphics */
    case 0x2e48: /* global name */
    case 0x2e6d: /* local name */
        return 0;
    default:
        /* In particular, CODE and binary data may encode references not
         * described by ordinary object structure. Do not certify them empty. */
        return emit(&walk, HPCLANG_RPL_REFERENCE_OPAQUE, address + (body_prologue ? 5 : 0),
                    address + (body_prologue ? 5 : 0), span - (body_prologue ? 5 : 0), prologue);
    }
}
