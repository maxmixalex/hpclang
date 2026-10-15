#include "rpl-reference.h"
#include "rpl-backup.h"
#include "rpl-port.h"
#include "rpl-object-store.h"
#include <stddef.h>

struct traversal
{
    hpclang_rpl_reference_visit_fn visit;
    void *context;
};

static int plan_shift(const struct hpclang_rpl_reference *reference, uint32_t first, uint32_t end,
                      int32_t delta, int field_mapping, int target_mapping, int qualified,
                      struct hpclang_rpl_reference_patch *patch)
{
    if (!reference || !patch)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (first >= end || end > 0x100000u || (int64_t)first + delta < 0 ||
        (int64_t)end + delta > 0x100000)
        return HPCLANG_RPL_SPAN_BOUNDS;
    switch (reference->kind) {
    case HPCLANG_RPL_REFERENCE_INLINE:
    case HPCLANG_RPL_REFERENCE_COMPACT:
    case HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY:
    case HPCLANG_RPL_REFERENCE_ROMP:
    case HPCLANG_RPL_REFERENCE_FLASHP:
        return 0;
    case HPCLANG_RPL_REFERENCE_OPAQUE:
    case HPCLANG_RPL_REFERENCE_EXECUTION:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    case HPCLANG_RPL_REFERENCE_ACPTR:
        if (reference->auxiliary && !qualified)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        break;
    case HPCLANG_RPL_REFERENCE_DIRECT:
    case HPCLANG_RPL_REFERENCE_ATTACHMENT:
        break;
    default:
        return HPCLANG_RPL_SPAN_MALFORMED;
    }
    uint32_t field = reference->field, target = reference->target;
    if (field > 0xffffbu || target >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int field_moves = field_mapping && field >= first && field < end;
    if ((field_moves && end - field < 5) || (field_mapping && field < first && first - field < 5))
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t shifted_field = (uint32_t)((int64_t)field + (field_moves ? delta : 0));
    uint32_t shifted_target =
        (uint32_t)((int64_t)target +
                   (target_mapping && target >= first && target < end ? delta : 0));
    int relative = reference->kind == HPCLANG_RPL_REFERENCE_DIRECT && reference->auxiliary;
    uint32_t old_value = relative ? (target - field) & 0xfffffu : target;
    uint32_t value = relative ? (shifted_target - shifted_field) & 0xfffffu : shifted_target;
    if (value == old_value)
        return 0;
    *patch = (struct hpclang_rpl_reference_patch){field, value};
    return 1;
}

int hpclang_rpl_reference_plan_shift(const struct hpclang_rpl_reference *reference, uint32_t first,
                                     uint32_t end, int32_t delta,
                                     struct hpclang_rpl_reference_patch *patch)
{
    return plan_shift(reference, first, end, delta, 1, 1, 0, patch);
}

int hpclang_rpl_reference_plan_qualified_shift(
    const struct hpclang_rpl_reference *reference,
    const struct hpclang_rpl_reference_mappings *mappings, uint32_t first, uint32_t end,
    int32_t delta, struct hpclang_rpl_reference_patch *patch)
{
    if (!mappings)
        return HPCLANG_RPL_SPAN_MALFORMED;
    return plan_shift(reference, first, end, delta, mappings->field == mappings->moving,
                      mappings->target == mappings->moving, 1, patch);
}

static int emit(struct traversal *walk, enum hpclang_rpl_reference_kind kind, uint32_t field,
                uint32_t target, uint32_t extent, uint32_t auxiliary)
{
    const struct hpclang_rpl_reference edge = {kind, field, target, extent, auxiliary};
    return walk->visit(walk->context, &edge);
}

int hpclang_rpl_reference_guards_validate(const struct hpclang_rpl_reference_reader *reader,
                                          const struct hpclang_rpl_reference_guard *guards,
                                          uint32_t count)
{
    if (!count)
        return HPCLANG_RPL_SPAN_OK;
    if (!reader || !reader->read_nibble || !guards)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    for (uint32_t i = 0; i < count; ++i) {
        const struct hpclang_rpl_reference_guard *guard = &guards[i];
        if (!guard->width || guard->width > 5)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (guard->field > 0x100000u - guard->width ||
            guard->expected >= (1u << (4 * guard->width)))
            return HPCLANG_RPL_SPAN_BOUNDS;
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t actual = 0;
        for (uint32_t j = 0; j < guards[i].width; ++j) {
            uint8_t nibble;
            if (!reader->read_nibble(reader->context, guards[i].mapping, guards[i].field + j,
                                     &nibble))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (nibble > 15)
                return HPCLANG_RPL_SPAN_MALFORMED;
            actual |= (uint32_t)nibble << (4 * j);
        }
        if (actual != guards[i].expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
    }
    return HPCLANG_RPL_SPAN_OK;
}

static int edits_metadata(const struct hpclang_rpl_reference_edit *edits, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        const struct hpclang_rpl_reference_edit *edit = &edits[i];
        if (edit->field > 0xffffbu || edit->expected > 0xfffffu || edit->value > 0xfffffu)
            return HPCLANG_RPL_SPAN_BOUNDS;
        for (uint32_t j = 0; j < i; ++j)
            if (edit->mapping == edits[j].mapping && edit->field < edits[j].field + 5u &&
                edits[j].field < edit->field + 5u)
                return HPCLANG_RPL_SPAN_MALFORMED;
    }
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_reference_edits_validate(const struct hpclang_rpl_reference_reader *reader,
                                         const struct hpclang_rpl_reference_edit *edits,
                                         uint32_t count)
{
    if (!count)
        return HPCLANG_RPL_SPAN_OK;
    if (!reader || !reader->read_nibble || !edits)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    int result = edits_metadata(edits, count);
    if (result)
        return result;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t actual = 0;
        for (unsigned j = 0; j < 5; ++j) {
            uint8_t nibble;
            if (!reader->read_nibble(reader->context, edits[i].mapping, edits[i].field + j,
                                     &nibble))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (nibble > 15)
                return HPCLANG_RPL_SPAN_MALFORMED;
            actual |= (uint32_t)nibble << (4u * j);
        }
        if (actual != edits[i].expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
    }
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_reference_edits_replay(const struct hpclang_rpl_reference_writer *writer,
                                       const struct hpclang_rpl_reference_edit *edits,
                                       uint32_t count)
{
    if (!count)
        return HPCLANG_RPL_SPAN_OK;
    if (!writer || !writer->reader.read_nibble || !writer->write_nibble || !edits)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    int result = edits_metadata(edits, count);
    if (result)
        return result;
    const struct hpclang_rpl_reference_reader *reader = &writer->reader;
    /* Pass zero preflights the complete batch before any mutation. */
    for (unsigned pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < count; ++i) {
            for (unsigned j = 0; j < 5; ++j) {
                uint8_t actual, expected = (uint8_t)((edits[i].expected >> (4u * j)) & 15u);
                uint8_t value = (uint8_t)((edits[i].value >> (4u * j)) & 15u);
                uint32_t at = edits[i].field + j;
                if (!reader->read_nibble(reader->context, edits[i].mapping, at, &actual))
                    return HPCLANG_RPL_SPAN_UNAVAILABLE;
                if (actual != expected && actual != value)
                    return HPCLANG_RPL_SPAN_MALFORMED;
                if (!pass || actual == value)
                    continue;
                if (!writer->write_nibble(reader->context, edits[i].mapping, at, value) ||
                    !reader->read_nibble(reader->context, edits[i].mapping, at, &actual))
                    return HPCLANG_RPL_SPAN_UNAVAILABLE;
                if (actual != value)
                    return HPCLANG_RPL_SPAN_MALFORMED;
            }
        }
    }
    return HPCLANG_RPL_SPAN_OK;
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

int hpclang_rpl_dispatch_reference_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                                         uint32_t limit,
                                         const struct hpclang_rpl_dispatch_descriptor *descriptor,
                                         hpclang_rpl_reference_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !descriptor || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 5u ||
        descriptor->dispatch >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (hpclang_rpl_object_prologue_known(descriptor->dispatch))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned width;
    switch (descriptor->schema) {
    case HPCLANG_RPL_DISPATCH_DUPLICATE_THEN_ENTRY:
        width = 1;
        break;
    case HPCLANG_RPL_DISPATCH_TYPE_PREDICATE:
        width = 5;
        break;
    default:
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    uint32_t dispatch, operand;
    int result = word(reader, address, 5, &dispatch);
    if (result)
        return result;
    if (dispatch != descriptor->dispatch)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (limit - 5u < width)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = word(reader, address + 5u, width, &operand);
    if (result)
        return result;
    struct traversal walk = {visit, context};
    result = emit(&walk, HPCLANG_RPL_REFERENCE_EXECUTION, address, dispatch, 0, 0);
    if (result || descriptor->schema == HPCLANG_RPL_DISPATCH_TYPE_PREDICATE)
        return result;
    return emit(&walk, HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY, address + 5u, address + 5u, 0, 0);
}

int hpclang_rpl_entry_inspect(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, struct hpclang_rpl_entry *entry)
{
    if (!reader || !reader->read_nibble || !entry)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_entry next = {HPCLANG_RPL_ENTRY_OBJECT, 0, 0};
    int result = word(reader, address, 5u, &next.dispatch);
    if (result)
        return result;
    if (hpclang_rpl_object_prologue_known(next.dispatch)) {
        result = hpclang_rpl_object_span(reader, address, limit, &next.span);
        if (result)
            return result;
    } else {
        if (next.dispatch != address + 5u)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (limit < 6u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t first;
        result = word(reader, address + 5u, 1u, &first);
        if (result)
            return result;
        next.kind = HPCLANG_RPL_ENTRY_SELF_DISPATCH;
    }
    *entry = next;
    return HPCLANG_RPL_SPAN_OK;
}

struct directory_reader
{
    const struct hpclang_rpl_reader *source;
    uint32_t first, end, home;
    int outside;
};

static int directory_read(void *context, uint32_t address, uint8_t *value)
{
    struct directory_reader *view = context;
    /* Cursor validation also reads USEROB and HOME's prologue. These two
     * fixed metadata reads do not authorize following links outside the node. */
    if ((address >= view->first && address < view->end) ||
        (address >= 0x80711 && address < 0x80716) ||
        (address >= view->home && address - view->home < 5))
        return view->source->read_nibble(view->source->context, address, value);
    view->outside = 1;
    return 0;
}

static int directory_error(int result, const struct directory_reader *view)
{
    if (view->outside || result == HPCLANG_RPL_STORE_LIMIT)
        return HPCLANG_RPL_SPAN_BOUNDS;
    return result == HPCLANG_RPL_STORE_UNAVAILABLE ? HPCLANG_RPL_SPAN_UNAVAILABLE
                                                   : HPCLANG_RPL_SPAN_MALFORMED;
}

static int directory(const struct hpclang_rpl_reader *source, uint32_t address, uint32_t span,
                     struct traversal *walk)
{
    uint32_t home, header;
    int result = word(source, 0x80711, 5, &home);
    if (result)
        return result;
    struct directory_reader view = {source, address, address + span, home, 0};
    const struct hpclang_rpl_reader reader = {directory_read, &view};
    struct hpclang_rpl_directory_cursor cursor;
    result = hpclang_rpl_directory_cursor_open(&reader, address, &cursor);
    if (result)
        return directory_error(result, &view);
    result = word(&reader, address + 5, 3, &header);
    if (result)
        return view.outside ? HPCLANG_RPL_SPAN_BOUNDS : result;
    if (address == home) {
        for (uint32_t i = 0; i < header; ++i) {
            uint32_t record = address + 8 + i * 13, number;
            result = word(&reader, record, 3, &number);
            if (result)
                return view.outside ? HPCLANG_RPL_SPAN_BOUNDS : result;
            for (uint32_t offset = 3; offset <= 8; offset += 5) {
                uint32_t target;
                result = word(&reader, record + offset, 5, &target);
                if (result)
                    return view.outside ? HPCLANG_RPL_SPAN_BOUNDS : result;
                if (target) {
                    result = emit(walk, HPCLANG_RPL_REFERENCE_ATTACHMENT, record + offset, target,
                                  0, number);
                    if (result)
                        return result;
                }
            }
        }
    } else if (header && header != 0x7ff) {
        /* HPGCC3's empty-directory constructor writes 7FF here; the native
         * saved subdirectories use the same marker. Other metadata remains
         * explicit pending work, rather than an assumed pointer-free header. */
        result = emit(walk, HPCLANG_RPL_REFERENCE_OPAQUE, address + 5, address + 5, 3, 0x2a96);
        if (result)
            return result;
    }
    uint32_t boundary = address + span;
    while (!cursor.finished) {
        uint32_t item = cursor.item;
        struct hpclang_rpl_directory_entry_view entry;
        result = hpclang_rpl_directory_cursor_next(&reader, &cursor, &entry);
        if (result != 1)
            return directory_error(result, &view);
        if (entry.object_address < address || entry.object_address >= boundary)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t child_span;
        result = hpclang_rpl_object_span(&reader, entry.object_address,
                                         boundary - entry.object_address, &child_span);
        if (result)
            return view.outside ? HPCLANG_RPL_SPAN_BOUNDS : result;
        result = emit(walk, HPCLANG_RPL_REFERENCE_INLINE, entry.object_address,
                      entry.object_address, child_span, 0);
        if (result)
            return result;
        boundary = item - 5;
    }
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
    case 0x2a96:
        return directory(reader, address, span, &walk);
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
