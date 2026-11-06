#include <hpclang/port.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "object-snapshot.h"
#include "rpl-stock-memory.h"
#include "rpl-object-store.h"
#include "stage-stack.h"
#include "rpl-reference.h"
#include "rpl-port.h"
#include "rpl-port-mutation.h"
#include "launch-services.h"
#include "rpl-execution.h"
#include "rpl-stock-view.h"
#include "rpl-bank-effects.h"
extern volatile uint32_t hpclang_e2e_result;
static int prepare_call(struct hpclang_rpl_mutation_io *, uint32_t **, int *);

/* Controlled Stock fixture only: the clean test catalog and fixture programs
 * supply the input provenance. Opaque execution coverage stays unresolved;
 * this is not a public registration or recovery implementation. */
struct probe_scratch
{
    uint32_t roots[2048];
    struct hpclang_rpl_reference_node work[512];
};
struct live_probe
{
    struct hpclang_rpl_mutation_io *io;
    uint32_t first, end, root_count;
    struct probe_scratch *scratch;
    int32_t delta;
    struct hpclang_rpl_reference_edit_builder builder;
    struct hpclang_rpl_reference_edit *edits;
    struct hpclang_rpl_reference_guard_builder guard_builder;
    struct hpclang_rpl_reference_guard *guards;
};
static void probe_buffers_free(struct live_probe *p)
{
    free(p->edits);
    free(p->guards);
    p->edits = NULL;
    p->guards = NULL;
}
static void probe_destroy(struct live_probe *p)
{
    if (p) {
        probe_buffers_free(p);
        free(p);
    }
}
static int probe_read_word(struct hpclang_rpl_mutation_io *io, uint32_t at, unsigned width,
                           uint32_t *out)
{
    *out = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t n;
        if (!io->read_nibble(io->context, at + i, &n) || n > 15)
            return -1;
        *out |= (uint32_t)n << (4 * i);
    }
    return 0;
}
static int probe_write_word(struct hpclang_rpl_mutation_io *io, uint32_t at, uint32_t value,
                            unsigned width)
{
    for (unsigned i = 0; i < width; ++i)
        if (!io->write_nibble(io->context, at + i, (value >> (4 * i)) & 15))
            return -1;
    return 0;
}
static int probe_root(void *context, enum hpclang_rpl_root_source source, uint32_t slot,
                      uint32_t value)
{
    (void)value;
    struct live_probe *p = context;
    if (source == HPCLANG_RPL_ROOT_NATIVE)
        return 0; /* catalog_grow owns these. */
    if (!slot || p->root_count == 2048)
        return -1;
    p->scratch->roots[p->root_count++] = slot;
    return 0;
}
static int probe_edge(void *context, const struct hpclang_rpl_reference *edge)
{
    struct live_probe *p = context;
    if (edge->kind == HPCLANG_RPL_REFERENCE_OPAQUE ||
        edge->kind == HPCLANG_RPL_REFERENCE_EXECUTION ||
        edge->kind == HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY) {
        return 0;
    }
    if (edge->kind != HPCLANG_RPL_REFERENCE_DIRECT && edge->kind != HPCLANG_RPL_REFERENCE_ACPTR &&
        edge->kind != HPCLANG_RPL_REFERENCE_ATTACHMENT)
        return 0;
    for (uint32_t i = 0; i < p->root_count; ++i)
        if (p->scratch->roots[i] == edge->field)
            return 0; /* Do not shift a known root twice. */
    uint32_t target = edge->target >= 0x80000 ? 1 : 0;
    if (edge->kind == HPCLANG_RPL_REFERENCE_ACPTR && edge->auxiliary) {
        if (edge->target < 0x40000 || edge->target >= 0x80000)
            return -1;
        target = 2; /* Nonmoving bank window, disjoint from this main-RAM move. */
    }
    struct hpclang_rpl_reference_mappings maps = {1, target, 1};
    struct hpclang_rpl_reference_patch patch;
    int result =
        hpclang_rpl_reference_plan_qualified_shift(edge, &maps, p->first, p->end, p->delta, &patch);
    if (result <= 0)
        return result;
    uint32_t old;
    if (probe_read_word(p->io, patch.field, 5, &old))
        return -1;
    struct hpclang_rpl_reference_edit edit = {1, patch.field, old, patch.value, 5};
    return hpclang_rpl_reference_edits_add(&p->builder, &edit);
}
static int probe_mapped_read(void *ctx, uint32_t map, uint32_t at, uint8_t *out)
{
    struct hpclang_rpl_mutation_io *io = ctx;
    return map == 1 && io->read_nibble(io->context, at, out);
}
static int probe_dependency(void *context, uint32_t at, uint8_t value)
{
    struct live_probe *p = context;
    return hpclang_rpl_reference_guards_observe(&p->guard_builder, 1, at, value);
}

static int probe_node(struct live_probe *p, struct hpclang_rpl_reader *reader, uint32_t at,
                      uint32_t span)
{
    struct hpclang_rpl_reference_node node = {at, span, 0};
    return hpclang_rpl_reference_visit_guarded_nested(reader, &node, p->scratch->work, 512, 10000,
                                                      probe_edge, probe_dependency, p);
}
static int probe_temp(void *context, uint32_t first, uint32_t span)
{
    struct live_probe *p = context;
    struct hpclang_rpl_reader reader = {p->io->read_nibble, p->io->context};
    uint32_t stop = first + span;
    while (first < stop) {
        uint32_t object_span;
        if (hpclang_rpl_object_span(&reader, first, stop - first, &object_span) || !object_span ||
            probe_node(p, &reader, first, object_span))
            return 7;
        first += object_span;
    }
    return 0;
}
static int probe_collect_once(struct live_probe *p, struct hpclang_rpl_reader *reader,
                              uint32_t home, unsigned edit_capacity, unsigned guard_capacity)
{
    p->root_count = 0;
    hpclang_rpl_reference_edits_begin(&p->builder, p->edits, edit_capacity);
    hpclang_rpl_reference_guards_begin(&p->guard_builder, p->guards, guard_capacity);
    if (hpclang_rpl_mutation_visit_guarded_roots(p->io, probe_root, probe_dependency, p))
        return 6;
    if (probe_node(p, reader, home, 0x100000 - home) ||
        hpclang_rpl_mutation_visit_guarded_temp(p->io, probe_temp, probe_dependency, p))
        return 7;
    const struct hpclang_rpl_reference_reader mapped = {probe_mapped_read, p->io};
    return hpclang_rpl_reference_edits_validate(&mapped, p->edits, p->builder.count) ? 8 : 0;
}
static int probe_collect(struct live_probe *p, struct hpclang_rpl_reader *reader, uint32_t home)
{
    p->scratch = malloc(sizeof *p->scratch);
    if (!p->scratch)
        return 5;
    int result = 5;
    /* Keep the full capacity ceiling without reserving two maximum-size tables
     * for every case. Discovery is read-only; a capacity retry starts afresh. */
    unsigned edit_capacity = 512, guard_capacity = 512;
    for (;;) {
        probe_buffers_free(p);
        p->edits = malloc(edit_capacity * sizeof *p->edits);
        p->guards = malloc(guard_capacity * sizeof *p->guards);
        if (!p->edits || !p->guards) {
            result = 5;
            break;
        }
        result = probe_collect_once(p, reader, home, edit_capacity, guard_capacity);
        if (!result)
            break;
        int edits_full = p->builder.error == HPCLANG_RPL_SPAN_BOUNDS;
        int guards_full = p->guard_builder.error == HPCLANG_RPL_SPAN_BOUNDS;
        if ((!edits_full && !guards_full) || (edits_full && edit_capacity == 4096) ||
            (guards_full && guard_capacity == 4096))
            break;
        /* Read dependencies can outnumber changed fields substantially. Grow
         * only the exhausted table; preserve both original capacity ceilings. */
        if (edits_full)
            edit_capacity *= 2;
        if (guards_full)
            guard_capacity *= 2;
    }
    free(p->scratch);
    p->scratch = NULL;
    return result;
}

struct rejected_removal
{
    const struct hpclang_rpl_mutation_io *source;
    unsigned writes;
};
static int rejected_read(void *context, uint32_t at, uint8_t *value)
{
    const struct rejected_removal *probe = context;
    return probe->source->read_nibble(probe->source->context, at, value);
}
static int rejected_write(void *context, uint32_t at, uint8_t value)
{
    (void)at;
    (void)value;
    struct rejected_removal *probe = context;
    ++probe->writes;
    return 0;
}
static int probe_attached_rejection(const struct hpclang_rpl_mutation_io *io)
{
    /* The actual clean Stock fixture has library 275 attached to HOME. Do not
     * alter its binding or attachment table to manufacture the rejection. */
    const struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_catalog_removal removal;
    if (hpclang_rpl_catalog_plan_remove(&reader, io->library_table, 0x100000 - io->library_table,
                                        275, &removal) != 1)
        return -1;
    uint32_t roots[3];
    for (unsigned i = 0; i < 3; ++i)
        roots[i] = *io->native_roots[i];
    struct rejected_removal probe = {io, 0};
    struct hpclang_rpl_mutation_io readonly = *io;
    readonly.read_nibble = rejected_read;
    readonly.write_nibble = rejected_write;
    readonly.context = &probe;
    int result = hpclang_rpl_catalog_remove_repaired(&readonly, 275, &removal, NULL);
    if (result != HPCLANG_RPL_MUTATION_BUSY || probe.writes)
        return -1;
    for (unsigned i = 0; i < 3; ++i)
        if (*io->native_roots[i] != roots[i])
            return -1;
    return 0;
}

/* This synchronous fixture keeps the native mapping stable. Intercept every
 * nibble write instead of retaining another entire HOME body in the tiny heap.
 * An identical write is harmless; any attempted value change is a failure. */
struct body_guard
{
    const struct hpclang_rpl_mutation_io *original;
    uint32_t first, end, writes, rejected;
};

static int body_guard_read(void *context, uint32_t at, uint8_t *value)
{
    struct body_guard *guard = context;
    return guard->original->read_nibble(guard->original->context, at, value);
}

static int body_guard_write(void *context, uint32_t at, uint8_t value)
{
    struct body_guard *guard = context;
    if (at >= guard->first && at < guard->end) {
        uint8_t original;
        if (!body_guard_read(guard, at, &original) || original != value) {
            ++guard->rejected;
            return 0;
        }
    }
    ++guard->writes;
    return guard->original->write_nibble(guard->original->context, at, value);
}

static int probe_detach(struct hpclang_rpl_mutation_io *io, int reattach)
{
    uint32_t home, count, port;
    if (probe_read_word(io, 0x80711, 5, &home) || probe_read_word(io, home + 5, 3, &count) ||
        probe_read_word(io, 0x80716, 5, &port))
        return -1;
    uint32_t body = home + 8 + 13 * count;
    if (!count || body >= port || port > 0x100000)
        return -1;
    uint32_t first = 0, second = 0, matches = 0;
    if (reattach) {
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t at = home + 8 + 13 * i, id;
            if (probe_read_word(io, at, 3, &id))
                return -1;
            if (id == 275) {
                ++matches;
                if (probe_read_word(io, at + 3, 5, &first) ||
                    probe_read_word(io, at + 8, 5, &second))
                    return -1;
            }
        }
        /* This controlled fixture's two library fields are below the moving
         * USEROB prefix. Do not assume arbitrary references stay fixed. */
        uint32_t stack;
        if (matches != 1 || probe_read_word(io, 0x806f8, 5, &stack) || first >= stack ||
            second >= stack)
            return -1;
    }
    struct body_guard guard = {io, body, port, 0, 0};
    struct hpclang_rpl_mutation_io guarded = *io;
    guarded.context = &guard;
    guarded.read_nibble = body_guard_read;
    guarded.write_nibble = body_guard_write;
    /* Validate every original nibble; unchanged-write interception proves that
     * all remain identical throughout the operation, without a body-sized copy. */
    for (uint32_t at = body; at < port; ++at) {
        uint8_t value;
        if (!body_guard_read(&guard, at, &value) || value > 15u)
            return -1;
    }
    if (hpclang_rpl_mutation_home_detach(&guarded, 275))
        return -1;
    uint32_t value;
    if (probe_read_word(io, 0x80711, 5, &value) || value != home + 13 ||
        probe_read_word(io, value + 5, 3, &value) || value != count - 1 ||
        probe_read_word(io, 0x80716, 5, &value) || value != port)
        return -1;
    if (reattach && (hpclang_rpl_mutation_home_attach(&guarded, 275, first, second) ||
                     probe_read_word(io, 0x80711, 5, &value) || value != home ||
                     probe_read_word(io, value + 5, 3, &value) || value != count ||
                     probe_read_word(io, 0x80716, 5, &value) || value != port ||
                     probe_attached_rejection(&guarded)))
        return -1;
    return !guard.rejected && guard.writes ? 0 : -1;
}

static int probe_cache_move(struct hpclang_rpl_mutation_io *io, uint32_t position, uint32_t target,
                            uint32_t bank, int remove)
{
    uint32_t bottom, top, end, home;
    if (probe_read_word(io, 0x806e9, 5, &bottom) || probe_read_word(io, 0x806ee, 5, &top) ||
        probe_read_word(io, 0x806f3, 5, &end) || probe_read_word(io, 0x80711, 5, &home))
        return -1;
    struct live_probe *p = calloc(1, sizeof *p);
    if (!p)
        return -1;
    p->io = io;
    p->first = position + (remove ? 15 : 0);
    p->end = end;
    p->delta = remove ? -15 : 15;
    struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    int result = probe_collect(p, &reader, home);
    if (!result) {
        const struct hpclang_rpl_catalog_repairs repairs = {
            1,    p->edits, p->builder.count, p->guards, p->guard_builder.count, NULL, 0, NULL,
            NULL, 0};
        result =
            remove ? hpclang_rpl_mutation_cache_acptr_remove(io, position, target, bank, &repairs)
                   : hpclang_rpl_mutation_cache_acptr_insert(io, position, target, bank, &repairs);
    }
    probe_destroy(p);
    return result;
}

static int probe_cache_roundtrip(struct hpclang_rpl_mutation_io *io, int keep)
{
    uint32_t home, count, bottom, end, fields[2] = {0}, targets[2], banks[2];
    const uint32_t boundaries[] = {0x806e9, 0x806ee, 0x806f3, 0x80711, 0x80e9b};
    uint32_t before[5], native[3];
    for (unsigned i = 0; i < 5; ++i)
        if (probe_read_word(io, boundaries[i], 5, before + i))
            return -1;
    bottom = before[0];
    end = before[2];
    home = before[3];
    for (unsigned i = 0; i < 3; ++i)
        native[i] = *io->native_roots[i];
    if (probe_read_word(io, home + 5, 3, &count))
        return -1;
    unsigned matches = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t slot = home + 8 + 13 * i, id;
        if (probe_read_word(io, slot, 3, &id))
            return -1;
        if (id == 275) {
            ++matches;
            if (probe_read_word(io, slot + 3, 5, fields) ||
                probe_read_word(io, slot + 8, 5, fields + 1))
                return -1;
        }
    }
    if (matches != 1 || end < bottom)
        return -1;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t prologue;
        if (fields[i] < io->library_table || fields[i] + 15 > bottom ||
            probe_read_word(io, fields[i], 5, &prologue) || prologue != 0x2baa ||
            probe_read_word(io, fields[i] + 5, 5, targets + i) ||
            probe_read_word(io, fields[i] + 10, 5, banks + i) || !banks[i])
            return -1;
    }
    uint32_t size = end - io->library_table;
    uint8_t *snapshot = calloc(((size_t)size + 1u) / 2u, 1u);
    if (!snapshot)
        return -1;
    int result = -1;
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t value;
        if (!io->read_nibble(io->context, io->library_table + i, &value) || value > 15u)
            goto done;
        snapshot[i / 2u] |= (uint8_t)(value << (4u * (i & 1u)));
    }
    if (hpclang_rpl_mutation_home_detach(io, 275) ||
        probe_cache_move(io, bottom, targets[0], banks[0], 0) ||
        probe_cache_move(io, bottom + 15, targets[1], banks[1], 0) ||
        hpclang_rpl_mutation_home_attach(io, 275, bottom, bottom + 15))
        goto done;
    /* Prove HOME now owns the new wrappers, not the original cached pointers. */
    uint32_t value;
    if (probe_read_word(io, 0x80711, 5, &value) || value != home ||
        probe_read_word(io, home + 8 + 13 * (count - 1) + 3, 5, &value) || value != bottom ||
        probe_read_word(io, home + 8 + 13 * (count - 1) + 8, 5, &value) || value != bottom + 15)
        goto done;
    for (unsigned i = 0; i < 2; ++i) {
        struct rejected_removal probe = {io, 0};
        struct hpclang_rpl_mutation_io readonly = *io;
        readonly.context = &probe;
        readonly.read_nibble = rejected_read;
        readonly.write_nibble = rejected_write;
        if (hpclang_rpl_mutation_cache_acptr_remove(&readonly, bottom + 15 * i, targets[i],
                                                    banks[i], NULL) != HPCLANG_RPL_MUTATION_BUSY ||
            probe.writes)
            goto done;
    }
    if (keep) {
        result = 0;
        goto done;
    }
    if (hpclang_rpl_mutation_home_detach(io, 275) ||
        probe_cache_move(io, bottom + 15, targets[1], banks[1], 1) ||
        probe_cache_move(io, bottom, targets[0], banks[0], 1) ||
        hpclang_rpl_mutation_home_attach(io, 275, fields[0], fields[1]))
        goto done;
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t value;
        if (!io->read_nibble(io->context, io->library_table + i, &value) ||
            value != ((snapshot[i / 2u] >> (4u * (i & 1u))) & 15u))
            goto done;
    }
    for (unsigned i = 0; i < 5; ++i)
        if (probe_read_word(io, boundaries[i], 5, &value) || value != before[i])
            goto done;
    for (unsigned i = 0; i < 3; ++i)
        if (*io->native_roots[i] != native[i])
            goto done;
    result = 0;
done:
    free(snapshot);
    return result;
}

static int probe_register(void)
{
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory))
        return 1;
    uint32_t *roots[10];
    int remove = 0;
    if (prepare_call(&io, roots, &remove))
        return 61;
    if (probe_attached_rejection(&io)) {
        hpclang_e2e_result = 0x46410ffc;
        for (;;)
            __asm__ volatile("nop");
    }
    struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    struct hpclang_rpl_catalog_insertion insertion;
    if (hpclang_rpl_catalog_plan_insert(&reader, io.library_table, 0x100000 - io.library_table, 123,
                                        &insertion) != 1)
        return 2;
    uint32_t end, bottom, top, home, port;
    if (probe_read_word(&io, 0x806f3, 5, &end) || probe_read_word(&io, 0x806e9, 5, &bottom) ||
        probe_read_word(&io, 0x806ee, 5, &top) || probe_read_word(&io, 0x80711, 5, &home) ||
        probe_read_word(&io, 0x80716, 5, &port))
        return 3;
    struct hpclang_rpl_port_cursor cursor = {port, 0x100000, 0};
    struct hpclang_rpl_port_entry entry;
    uint32_t binding = 0;
    int next;
    while ((next = hpclang_rpl_port_next(&reader, &cursor, &entry)) == 1)
        if (entry.prologue == 0x2b40 && entry.library_number == 123)
            binding = entry.address + 14 + 2 * entry.name_length;
    if (next || !binding || binding < end)
        return 4;
    struct live_probe *p = calloc(1, sizeof *p);
    if (!p)
        return 5;
    uint32_t snapshot_size = end - io.library_table;
    uint8_t *snapshot = remove ? calloc((snapshot_size + 1u) / 2u, 1) : NULL;
    uint32_t native_before[3], avmem_before;
    if (remove) {
        if (!snapshot || probe_read_word(&io, 0x80e9b, 5, &avmem_before)) {
            free(snapshot);
            probe_destroy(p);
            return 9;
        }
        for (uint32_t i = 0; i < snapshot_size; ++i) {
            uint8_t value;
            if (!io.read_nibble(io.context, io.library_table + i, &value) || value > 15u) {
                free(snapshot);
                probe_destroy(p);
                return 9;
            }
            snapshot[i / 2u] |= (uint8_t)(value << (4u * (i & 1u)));
        }
        for (unsigned i = 0; i < 3; ++i)
            native_before[i] = *io.native_roots[i];
    }
    p->io = &io;
    p->first = insertion.slot;
    p->end = end;
    p->delta = 16;
    int result = probe_collect(p, &reader, home);
    if (result)
        goto done;
    const struct hpclang_rpl_catalog_repairs repairs = {
        1, p->edits, p->builder.count, p->guards, p->guard_builder.count, NULL, 0, NULL, NULL, 0};
    /* After this boundary an error must halt the test, never return Stock. */
    const struct hpclang_rpl_library_binding added = {binding, 0, 0};
    if (hpclang_rpl_catalog_insert_repaired(&io, 123, &added, &insertion, &repairs)) {
        hpclang_e2e_result = 0x46410ffe;
        for (;;)
            __asm__ volatile("nop");
    }
    if (remove) {
        struct hpclang_rpl_catalog_removal removal;
        probe_buffers_free(p);
        memset(p, 0, sizeof *p);
        p->io = &io;
        p->first = insertion.slot + 16;
        p->end = end + 16;
        p->delta = -16;
        uint32_t current_home;
        if (probe_read_word(&io, 0x80711, 5, &current_home) ||
            probe_collect(p, &reader, current_home) ||
            hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 0x100000 - io.library_table,
                                            123, &removal) != 1)
            goto failed_roundtrip;
        const struct hpclang_rpl_catalog_repairs undo = {
            1,    p->edits, p->builder.count, p->guards, p->guard_builder.count, NULL, 0, NULL,
            NULL, 0};
        if (hpclang_rpl_catalog_remove_repaired(&io, 123, &removal, &undo))
            goto failed_roundtrip;
        for (uint32_t i = 0; i < snapshot_size; ++i) {
            uint8_t value;
            if (!io.read_nibble(io.context, io.library_table + i, &value) ||
                value != ((snapshot[i / 2u] >> (4u * (i & 1u))) & 15u))
                goto failed_roundtrip;
        }
        for (unsigned i = 0; i < 3; ++i)
            if (*io.native_roots[i] != native_before[i])
                goto failed_roundtrip;
        const uint32_t fields[] = {0x806e9, 0x806ee, 0x806f3, 0x80711, 0x80e9b};
        const uint32_t values[] = {bottom, top, end, home, avmem_before};
        for (unsigned i = 0; i < 5; ++i) {
            uint32_t value;
            if (probe_read_word(&io, fields[i], 5, &value) || value != values[i])
                goto failed_roundtrip;
        }
    }
    if (remove >= 2) {
        /* The relocation plan and round-trip snapshot are no longer live.
         * Release them before taking the separate HOME-body snapshot. */
        free(snapshot);
        snapshot = NULL;
        probe_destroy(p);
        p = NULL;
        if (probe_detach(&io, remove >= 3))
            goto failed_roundtrip;
    }
    if (remove >= 4 && probe_cache_roundtrip(&io, remove == 5))
        goto failed_roundtrip;
    result = 0;
done:
    free(snapshot);
    probe_destroy(p);
    return result;
failed_roundtrip:
    hpclang_e2e_result = 0x46410ffd;
    for (;;)
        __asm__ volatile("nop");
}

static int stage_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    return !probe_read_word(context, at, width, value);
}

static int prepare_call(struct hpclang_rpl_mutation_io *io, uint32_t **roots, int *remove)
{
    if (io->native_root_count != 3 || !io->native_roots[1])
        return -1;
    uint32_t top, object, prologue, length, opcode, encoded;
    if (probe_read_word(io, 0x806f8, 5, &top) || probe_read_word(io, top, 5, &object) ||
        probe_read_word(io, object, 5, &prologue) || prologue != 0x2a2c ||
        probe_read_word(io, object + 5, 5, &length))
        return -1;
    static const uint8_t magic[] = "HPCSCALL";
    for (unsigned i = 0; i < 8; ++i)
        if (probe_read_word(io, object + 10 + 2 * i, 2, &encoded) || encoded != magic[i])
            return -1;
    /* The magic-only fixture keeps the real launcher's suspended PC and
     * original return roots. Do not manufacture a call or rewrite its payload. */
    if (length == 21)
        return 0;
    if (length == 23 && !probe_read_word(io, object + 26, 2, &encoded) && encoded <= 4) {
        *remove = 1 + encoded;
        return 0;
    }
    if (probe_read_word(io, object + 26, 2, &encoded))
        return -1;
    unsigned conditional = encoded == 0x37;
    unsigned size = encoded == 0x27 || conditional ? 4
                    : encoded == 0xe8              ? 6
                    : encoded == 0xf8              ? 7
                                                   : 0;
    if (!size || length != (conditional ? 31u : size == 4 ? 29u : size == 6 ? 31u : 33u))
        return -1;
    unsigned absolute = size == 7;
    if (probe_read_word(io, object + 26 + (absolute ? 2 : 0), absolute ? 5 : size, &encoded) ||
        encoded != (conditional ? 0x37u
                    : absolute  ? 0u
                    : size == 4 ? 0x27u
                                : 0x2e8u))
        return -1;
    uint32_t tail = object + 26 + size;
    if (probe_read_word(io, *io->native_roots[1], 2, &opcode) ||
        (opcode != 0x10 && opcode != 0x20 && opcode != 0x30))
        return -1;
    if (conditional) {
        uint32_t padding;
        /* GOSUB -> RTNCC -> RTNNC must preserve the actual caller's clear carry. */
        if (opcode != 0x30 || probe_read_word(io, tail, 5, &encoded) || encoded != 0x30005 ||
            probe_read_word(io, tail + 5, 1, &padding) || padding)
            return -1;
    } else if (probe_read_word(io, tail, 4, &encoded) || (encoded != 0x1010 && encoded != 0x0010)) {
        return -1;
    }
    uint32_t *cpu = (uint32_t *)(uintptr_t)io->native_roots[1][1];
    if (cpu[0x998 / 4] >= 8)
        return -1;
    /* Preserve the selected callee return (RTN or RTNSXM), while restoring the
     * original continuation's flag-setting return at the call fallthrough. */
    if ((!conditional &&
         probe_write_word(io, tail - 1, (opcode << 4) | ((encoded & 0xff00) << 4), 5)) ||
        (absolute && probe_write_word(io, object + 28, tail + 2, 5)))
        return -1;
    roots[0] = io->native_roots[0];
    roots[1] = io->native_roots[1];
    *roots[1] = object + 26;
    const struct hpclang_stage_stack_io stage = {io, stage_word, NULL};
    unsigned count =
        hpclang_stage_return_roots(&stage, *roots[1], cpu + 0x978 / 4, cpu[0x998 / 4], roots + 2);
    if (count != 1 || roots[2] != cpu + 0x978 / 4 + ((cpu[0x998 / 4] + 7) & 7))
        return -1;
    io->native_roots = roots;
    io->native_root_count = 3;
    return 0;
}

/* Library 123, title X, one packed string-array message; relative tables and CRC. */
static const char library_encoding[] =
    "04b2027000108510b7000000f000000000000008e92074000c2a20100001000033000860736c616e67602c69626271"
    "6279702d6563737167656f17d";
static int library_nibble(void *context, uint32_t at, uint8_t *out)
{
    (void)context;
    if (at >= sizeof library_encoding - 1)
        return 0;
    *out =
        library_encoding[at] <= '9' ? library_encoding[at] - '0' : library_encoding[at] - 'a' + 10;
    return 1;
}

struct execution_probe
{
    struct hpclang_rpl_code_execution state;
    struct hpclang_rpl_reference_guard guards[1536];
    struct hpclang_rpl_reference_guard_builder builder;
    struct hpclang_rpl_stock_view view;
    uint32_t *pc;
    uint32_t observations[4], uses, indirect_targets, derived_uses, derived_source, adjustment;
    uint32_t generated_uses;
    struct hpclang_rpl_execution_subtraction expression, payload_expression;
    uint32_t expression_count, expression_uses, expression_pc, expression_sources;
    struct hpclang_rpl_reference_edit edits[192];
    struct hpclang_rpl_reference_edit_builder plan;
    struct hpclang_rpl_code_step_uses first_use;
    uint32_t move_first, move_end, planned_uses, branches, changed_cells, rejected_plans;
    struct hpclang_rpl_code_store store, store_records[6];
    uint32_t stores, stored_sources, checked_reads, d0_derived;
    struct
    {
        uint32_t field, value, origin;
    } shadow[32];
    uint8_t native_before[0xc60], native_after[0xc60];
    unsigned configurations, payload_limit, payload_depth;
    struct hpclang_rpl_code_constant constants[2];
    unsigned constant_count, constant_limit, predicates;
    struct hpclang_rpl_code_unit units[1];
    unsigned unit_count, unit_limit;
};

_Static_assert(sizeof(struct execution_probe) <= 40 * 1024, "execution fixture heap budget");

static int execution_shadow_value(void *context, uint32_t at, uint8_t *value, uint32_t *token);
static int execution_shadow_read(void *context, uint32_t at, uint8_t *value);

static int execution_observe(void *context, unsigned space, uint32_t at, uint8_t value)
{
    struct execution_probe *probe = context;
    if (space > HPCLANG_RPL_EXECUTION_PC)
        return HPCLANG_RPL_SPAN_MALFORMED;
    ++probe->observations[space];
    /* Code/data use the same current Saturn mapping; do not invent disjoint
     * identities for two readers of that same address space. */
    unsigned mapping = space < 2 ? 1 : space;
    return hpclang_rpl_reference_guards_observe(&probe->builder, mapping, at, value);
}

static int execution_token(void *context, unsigned data, uint32_t at, uint8_t value,
                           uint32_t *token)
{
    (void)data;
    uint8_t expected;
    uint32_t origin;
    int result = execution_shadow_value(context, at, &expected, &origin);
    if (result)
        return result;
    if (value != expected)
        return HPCLANG_RPL_SPAN_MALFORMED;
    *token = origin;
    return 0;
}

static int execution_plan_read(void *context, uint32_t at, uint8_t *value)
{
    struct execution_probe *probe = context;
    ++probe->observations[HPCLANG_RPL_EXECUTION_CODE];
    return execution_shadow_read(context, at, value);
}

static int execution_derive(void *context, uint32_t pc,
                            const struct hpclang_rpl_execution_subtraction *expression,
                            uint32_t tokens[5])
{
    struct execution_probe *probe = context;
    if (probe->expression_count >= 2)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (!probe->expression_count) {
        probe->expression = *expression;
        probe->expression_pc = pc;
    } else {
        if (pc != 0x2a19d)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        probe->payload_expression = *expression;
    }
    for (unsigned digit = 0; digit < 5; ++digit)
        tokens[digit] = 0x400001 + 5 * probe->expression_count + digit;
    ++probe->expression_count;
    return 0;
}

static int execution_native_read(void *context, uint32_t at, uint8_t *value)
{
    struct execution_probe *probe = context;
    return hpclang_rpl_stock_view_native(&probe->view, at, value) &&
           !execution_observe(context, HPCLANG_RPL_EXECUTION_NATIVE, at, *value);
}

static int execution_source(void *context, uint32_t token,
                            struct hpclang_rpl_code_origin_cell *cell)
{
    struct execution_probe *probe = context;
    if (token >= 0x200000 && token < 0x400000) {
        const struct hpclang_rpl_reader native = {execution_native_read, probe};
        const struct hpclang_rpl_stock_execution_source source = {&native, 0x200000, 1,
                                                                  probe->view.state_offset * 2};
        return hpclang_rpl_stock_execution_origin((void *)&source, token, cell);
    }
    if (!token || token > 0x100000)
        return HPCLANG_RPL_SPAN_UNSUPPORTED; /* Never treat expression IDs as memory. */
    /* Guard Read[] too. Logical SRAM aliases and native-state tokens must share
     * physical coordinates before they enter the common constraint builder. */
    uint32_t page = 0, at = token - 1;
    for (unsigned digit = 0; digit < 8; ++digit) {
        uint8_t value;
        if (!execution_native_read(probe, (at >> 12) * 8 + digit, &value))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        page |= (uint32_t)value << (4 * digit);
    }
    uint8_t value;
    if (!hpclang_rpl_stock_view_saturn(&probe->view, at, &value))
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    int result = execution_observe(context, HPCLANG_RPL_EXECUTION_DATA, at, value);
    if (!result) {
        uint32_t physical = page + (at & 0xfff) / 2;
        uint32_t mapping = physical >= 0x08000000 ? 1 : 4 + physical / 0x80000;
        uint32_t field = physical >= 0x08000000 ? physical - 0x08000000 : physical % 0x80000;
        *cell = (struct hpclang_rpl_code_origin_cell){mapping, field * 2 + (at & 1), value};
    }
    return result;
}

static uint32_t execution_shift(const struct execution_probe *probe, uint32_t value)
{
    return value >= probe->move_first && value < probe->move_end ? value + 16 : value;
}

/* Normalize aliases through the guarded original map. Store destinations are
 * SRAM only and exclude native CPU state. Original dependency readers never
 * consult this overlay: simulated values must not become original-memory guards. */
static int execution_shadow_value(void *context, uint32_t at, uint8_t *value, uint32_t *token)
{
    struct execution_probe *probe = context;
    if (at >= 0x100000)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_code_origin_cell cell;
    int result = execution_source(context, at + 1, &cell);
    if (result)
        return result;
    uint32_t origin = at + 1;
    if (probe->stores && cell.mapping == 1)
        for (unsigned i = 0; i < probe->stored_sources; ++i)
            if (probe->shadow[i].field == cell.field) {
                cell.expected = probe->shadow[i].value;
                origin = probe->shadow[i].origin;
                break;
            }
    *value = (uint8_t)cell.expected;
    *token = origin;
    return 0;
}

static int execution_shadow_read(void *context, uint32_t at, uint8_t *value)
{
    uint32_t token;
    return !execution_shadow_value(context, at, value, &token);
}

static int execution_collect_observe(void *context, unsigned space, uint32_t at, uint8_t value)
{
    struct execution_probe *probe = context;
    if (space == HPCLANG_RPL_EXECUTION_CODE || space == HPCLANG_RPL_EXECUTION_DATA) {
        /* shadow_read already guarded the original cell and mapping. */
        ++probe->observations[space];
        return 0;
    }
    return execution_observe(context, space, at, value);
}

/* The native ROM store path has its own Write[] table and two I/O
 * dispatch windows. A readable SRAM alias alone does not qualify a write. */
static int execution_write_cell(struct execution_probe *probe, uint32_t at,
                                struct hpclang_rpl_code_origin_cell *cell)
{
    uint32_t words[3] = {0};
    const uint32_t offsets[3] = {0x404 + 4 * (at >> 12), 0xba0, 0xbb0};
    for (unsigned word = 0; word < 3; ++word)
        for (unsigned digit = 0; digit < 8; ++digit) {
            uint8_t value;
            if (!execution_native_read(probe, offsets[word] * 2 + digit, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            words[word] |= (uint32_t)value << (4 * digit);
        }
    if (words[1] == (at >> 6) || words[2] == (at >> 12) || words[0] < 0x08000000 ||
        words[0] > 0x0807f800)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    int result = execution_source(probe, at + 1, cell);
    if (result)
        return result;
    uint32_t physical = (words[0] - 0x08000000) * 2 + (at & 0xfff);
    if (cell->mapping != 1 || cell->field != physical)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return 0;
}

static int execution_payload(void *context, struct hpclang_rpl_reference_edit_builder *builder,
                             uint32_t token, uint8_t expected, uint8_t replacement)
{
    struct execution_probe *probe = context;
    if (token >= 0x700001) {
        unsigned index = (token - 0x700001) / 16, digit = (token - 0x700001) % 16;
        if (index >= probe->unit_count || index >= probe->unit_limit || replacement != expected)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_unit *fact = &probe->units[index];
        if (digit >= fact->nibbles ||
            ((fact->after >> (4 * (fact->first + digit))) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        unsigned limit = probe->unit_limit;
        probe->unit_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        int result = hpclang_rpl_execution_plan_unit(builder, fact, fact->before, fact->after,
                                                     &instructions, execution_token,
                                                     execution_source, execution_payload, probe);
        probe->unit_limit = limit;
        return result;
    }
    if (token >= 0x600001) {
        unsigned index = (token - 0x600001) / 16, digit = (token - 0x600001) % 16;
        if (index >= probe->constant_count || index >= probe->constant_limit)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        unsigned limit = probe->constant_limit;
        probe->constant_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        int result = hpclang_rpl_execution_plan_constant(
            builder, &probe->constants[index], digit, expected, replacement, &instructions,
            execution_token, execution_source, execution_payload, probe);
        probe->constant_limit = limit;
        return result;
    }
    if (token >= 0x500001) {
        unsigned index = (token - 0x500001) / 16, digit = (token - 0x500001) % 16;
        if (index >= probe->payload_limit || digit >= 5 || replacement != expected ||
            probe->payload_depth >= 6)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_store *producer = &probe->store_records[index];
        if (!producer->adjustment || digit < producer->first ||
            digit >= producer->first + producer->nibbles ||
            ((producer->value >> (4 * digit)) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        unsigned limit = probe->payload_limit;
        probe->payload_limit = index;
        ++probe->payload_depth;
        int result = hpclang_rpl_execution_plan_payload(builder, producer, producer->value,
                                                        execution_source, execution_payload, probe);
        --probe->payload_depth;
        probe->payload_limit = limit;
        return result;
    }
    if (probe->expression_count > 2 || token < 0x400001 ||
        token >= 0x400001 + 5 * probe->expression_count || replacement != expected)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned index = (token - 0x400001) / 5, digit = (token - 0x400001) % 5;
    const struct hpclang_rpl_execution_subtraction *fact =
        index ? &probe->payload_expression : &probe->expression;
    if (((fact->value >> (4 * digit)) & 15) != expected)
        return HPCLANG_RPL_SPAN_MALFORMED;
    return hpclang_rpl_execution_plan_subtraction(
        builder, fact, fact->left.value, fact->right.value, fact->value, execution_source, probe);
}

static int execution_store(void *context, const struct hpclang_rpl_code_store *store)
{
    struct execution_probe *probe = context;
    const unsigned count = store->nibbles;
    /* Six effects observed in the original ROM continuation. This finite
     * fixture permits only the final same-destination overlap, without granting
     * general store authority. The overlay is published only after validation. */
    static const uint32_t pcs[6] = {0x2c3c, 0x2a1ba, 0x2a1c1, 0x2a1e0, 0x2da2, 0xb265};
    static const unsigned widths[6] = {5, 8, 4, 5, 5, 10};
    if (!count || count > 10 || probe->stores >= 6 || store->instruction != pcs[probe->stores] ||
        store->first || count != widths[probe->stores] || store->address.value > 0x100000 - count ||
        (probe->stores == 5 && store->address.value != probe->store_records[4].address.value))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t fields[10], origins[10], slots[10];
    unsigned added = 0;
    for (unsigned digit = 0; digit < count; ++digit) {
        struct hpclang_rpl_code_origin_cell cell;
        int result = execution_write_cell(probe, store->address.value + digit, &cell);
        if (result)
            return result;
        if (cell.mapping != 1 || cell.field >= 0x100000 ||
            (cell.field >= probe->view.state_offset * 2 &&
             cell.field < (probe->view.state_offset + 0xc60) * 2))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        fields[digit] = cell.field;
        for (unsigned prior = 0; prior < digit; ++prior)
            if (fields[prior] == cell.field)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        slots[digit] = probe->stored_sources + added;
        unsigned overlap = 0;
        for (unsigned prior = 0; prior < probe->stored_sources; ++prior)
            if (probe->shadow[prior].field == cell.field) {
                if (probe->stores != 5 || digit >= 5)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                slots[digit] = prior;
                overlap = 1;
            }
        if ((probe->stores == 5 && digit < 5) != overlap)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (!overlap && ++added + probe->stored_sources > 32)
            return HPCLANG_RPL_SPAN_BOUNDS;
        origins[digit] = store->origins[digit];
    }
    uint64_t replacement = store->value;
    if (probe->stores == 2) {
        const struct hpclang_rpl_execution_subtraction *fact = &probe->payload_expression;
        if (probe->expression_count != 2 || fact->decimal || store->adjustment != 1 ||
            ((fact->value + 1) & 0xfffff) != (store->value & 0xfffff))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        for (unsigned digit = 0; digit < 5; ++digit)
            if (store->origins[digit] != 0x400006 + digit)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        for (unsigned digit = 0; digit < count; ++digit)
            origins[digit] = 0x500001 + 16 * probe->stores + digit;
    } else if (probe->stores == 4) {
        if (store->adjustment != 5)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        for (unsigned digit = 0; digit < count; ++digit)
            origins[digit] = 0x500001 + 16 * probe->stores + digit;
    } else if (store->adjustment) {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (!probe->stores) {
        /* This first address payload already has a qualified replacement from
         * its consumed use. Preserve that proposal in the common payload plan. */
        for (unsigned digit = 0; digit < count; ++digit) {
            struct hpclang_rpl_code_origin_cell cell;
            int result = execution_source(probe, store->origins[digit], &cell);
            if (result)
                return result;
            uint32_t value = (uint32_t)(store->value >> (4 * digit)) & 15;
            if (cell.expected != value)
                return HPCLANG_RPL_SPAN_MALFORMED;
            unsigned found = 0;
            for (unsigned j = 0; j < probe->plan.count; ++j) {
                const struct hpclang_rpl_reference_edit *prior = &probe->edits[j];
                if (prior->mapping == cell.mapping && prior->field == cell.field &&
                    prior->width == 1 && prior->expected == value) {
                    replacement = (replacement & ~(UINT64_C(15) << (4 * digit))) |
                                  ((uint64_t)prior->value << (4 * digit));
                    ++found;
                }
            }
            if (found != 1)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    }
    probe->payload_limit = probe->stores;
    int result = hpclang_rpl_execution_plan_payload(&probe->plan, store, replacement,
                                                    execution_source, execution_payload, probe);
    if (result)
        return result;
    if (!probe->stores)
        probe->store = *store;
    probe->store_records[probe->stores] = *store;
    for (unsigned digit = 0; digit < count; ++digit) {
        unsigned at = slots[digit];
        probe->shadow[at].field = fields[digit];
        probe->shadow[at].value = (uint32_t)(store->value >> (4 * digit)) & 15;
        probe->shadow[at].origin = origins[digit];
    }
    probe->stored_sources += added;
    ++probe->stores;
    return 0;
}

static int execution_constant(void *context, const struct hpclang_rpl_code_constant *fact,
                              uint32_t tokens[16])
{
    struct execution_probe *probe = context;
    if (probe->constant_count >= 2 || !fact->assignment.nibbles || fact->assignment.nibbles > 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    unsigned index = probe->constant_count;
    probe->constants[index] = *fact;
    for (unsigned i = 0; i < fact->assignment.nibbles; ++i)
        tokens[i] = 0x600001 + 16 * index + i;
    probe->constant_limit = ++probe->constant_count;
    return 0;
}

static int execution_unit(void *context, const struct hpclang_rpl_code_unit *fact,
                          uint32_t tokens[16])
{
    struct execution_probe *probe = context;
    if (probe->unit_count >= 1 || !fact->nibbles || fact->nibbles > 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
    int result = hpclang_rpl_execution_plan_unit(&probe->plan, fact, fact->before, fact->after,
                                                 &instructions, execution_token, execution_source,
                                                 execution_payload, probe);
    if (result)
        return result;
    unsigned index = probe->unit_count;
    probe->units[index] = *fact;
    for (unsigned i = 0; i < fact->nibbles; ++i)
        tokens[i] = 0x700001 + 16 * index + i;
    probe->unit_limit = ++probe->unit_count;
    return 0;
}

static int execution_predicate(void *context, const struct hpclang_rpl_code_predicate *predicate,
                               const struct hpclang_rpl_code_execution *before)
{
    struct execution_probe *probe = context;
    if (probe->predicates || predicate->instruction != 0xb283 ||
        predicate->left != HPCLANG_RPL_CODE_C || predicate->right != HPCLANG_RPL_CODE_CONSTANT ||
        predicate->first || predicate->nibbles != 3)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const uint64_t replacements[2] = {before->values.registers[HPCLANG_RPL_CODE_C], 0};
    int result = hpclang_rpl_execution_plan_predicate(&probe->plan, predicate, before, replacements,
                                                      execution_source, execution_payload, probe);
    if (!result)
        ++probe->predicates;
    return result;
}

static int execution_stale_source(void *context, uint32_t token,
                                  struct hpclang_rpl_code_origin_cell *cell)
{
    int result = execution_source(context, token, cell);
    if (!result)
        cell->expected ^= 1;
    return result;
}

static int execution_visit(void *context, uint32_t pc,
                           const struct hpclang_rpl_code_execution *state,
                           const struct hpclang_rpl_code_step_uses *uses)
{
    struct execution_probe *probe = context;
    probe->uses += uses->count;
    if (uses->has_branch) {
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        const struct hpclang_rpl_reference_mappings mappings = {1, 1, 1};
        struct hpclang_rpl_code_patch patch;
        int result =
            hpclang_rpl_code_branch_plan_shift(&instructions, pc, 0x100000 - pc, &mappings,
                                               probe->move_first, probe->move_end, 16, &patch);
        /* This fixture's branches keep their encodings. A changed operand
         * needs its own source constraints, never silent omission. */
        if (result)
            return result < 0 ? result : HPCLANG_RPL_SPAN_UNSUPPORTED;
        ++probe->branches;
    }
    for (unsigned i = 0; i < uses->count; ++i) {
        const struct hpclang_rpl_code_address_use *use = &uses->addresses[i];
        unsigned expression_digits = 0;
        for (unsigned digit = 0; digit < 5; ++digit)
            expression_digits += use->origins[digit] >= 0x400000;
        if (expression_digits) {
            if (expression_digits != 5 || probe->expression_count != 1 || use->adjustment ||
                use->generated || use->value != probe->expression.value)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            for (unsigned digit = 0; digit < 5; ++digit)
                if (use->origins[digit] != 0x400001 + digit)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
            struct hpclang_rpl_reference_edit edits[7];
            struct hpclang_rpl_reference_edit_builder builder;
            int result = hpclang_rpl_reference_edits_begin(&builder, edits, 7);
            if (!result)
                result = hpclang_rpl_execution_plan_subtraction(
                    &builder, &probe->expression, probe->expression.left.value,
                    probe->expression.right.value, use->value, execution_source, probe);
            if (result)
                return result;
            if (builder.count != 7)
                return HPCLANG_RPL_SPAN_MALFORMED;
            for (unsigned cell = 0; cell < builder.count; ++cell)
                if (edits[cell].expected != edits[cell].value)
                    return HPCLANG_RPL_SPAN_MALFORMED;
            ++probe->expression_uses;
            probe->expression_sources = builder.count;
            result = hpclang_rpl_execution_plan_subtraction(
                &probe->plan, &probe->expression,
                execution_shift(probe, probe->expression.left.value), probe->expression.right.value,
                execution_shift(probe, use->value), execution_source, probe);
            if (result)
                return result;
        }
        if (use->generated) {
            const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
            const struct hpclang_rpl_reference_mappings mappings = {1, 1, 1};
            struct hpclang_rpl_code_patch patch;
            int result = hpclang_rpl_code_generated_return_plan(
                &instructions, use, &mappings, probe->move_first, probe->move_end, 16,
                execution_shift(probe, use->value), &patch);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            ++probe->generated_uses;
        }
        if (!expression_digits && !use->generated) {
            const struct hpclang_rpl_code_step_uses single = {.count = 1, .addresses = {*use}};
            uint32_t replacement = execution_shift(probe, use->value);
            int result = hpclang_rpl_execution_plan_uses(&probe->plan, &single, &replacement,
                                                         execution_source, probe);
            if (result)
                return result;
            if (!probe->first_use.count)
                probe->first_use = single;
        }
        ++probe->planned_uses;
        if (!use->adjustment)
            continue;
        if (probe->stores && !memcmp(use->origins, probe->store.origins, sizeof use->origins)) {
            if (use->adjustment != 10)
                return HPCLANG_RPL_SPAN_MALFORMED;
            ++probe->d0_derived;
            continue;
        }
        if (probe->stores >= 2)
            continue; /* Later D1/stack offsets were constrained above too. */
        uint32_t first = use->origins[0];
        if (!first || (probe->derived_uses && probe->derived_source != first - 1))
            return HPCLANG_RPL_SPAN_MALFORMED;
        for (unsigned digit = 0; digit < 5; ++digit)
            if (use->origins[digit] != first + digit)
                return HPCLANG_RPL_SPAN_MALFORMED;
        ++probe->derived_uses;
        probe->derived_source = first - 1;
        probe->adjustment = use->adjustment;
    }
    return state->control.pc == 0xb2ab && probe->expression_uses == 1 && probe->stores == 6 &&
           probe->configurations == 1 && probe->predicates == 1 && probe->constant_count == 2 &&
           probe->unit_count == 1;
    /* This selected boundary is not a complete relocation certificate. */
}

/* This continuation configures the bank phase without changing any address map.
 * Predict the full state and reject any other effect; readers keep the original
 * map only after this explicit equality check. Buffers and guards are bounded. */
static int execution_config(void *context, const struct hpclang_rpl_code_configuration *config)
{
    struct execution_probe *probe = context;
    if (probe->configurations || config->instruction != 0xb247)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const struct hpclang_rpl_code_store source = {
        .nibbles = 5, .value = config->value, .adjustment = config->adjustment};
    struct hpclang_rpl_code_store input = source;
    memcpy(input.origins, config->origins, sizeof config->origins);
    probe->payload_limit = probe->stores;
    int result = hpclang_rpl_execution_plan_payload(&probe->plan, &input, input.value,
                                                    execution_source, execution_payload, probe);
    if (result)
        return result;
    for (unsigned at = 0; at < sizeof probe->native_before; ++at) {
        uint8_t low, high;
        if (!execution_native_read(probe, at * 2, &low) ||
            !execution_native_read(probe, at * 2 + 1, &high))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        probe->native_before[at] = low | (high << 4);
    }
    result = hpclang_rpl_config_predict(probe->native_before, config->value, probe->native_after);
    if (result)
        return result;
    if (probe->native_before[0xb9c] != 3 || probe->native_after[0xb9c] != 35)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    for (unsigned at = 0; at < sizeof probe->native_before; ++at)
        if (at != 0xb9c && probe->native_before[at] != probe->native_after[at])
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    ++probe->configurations;
    return 0;
}

static int execution_guard_read(void *context, uint32_t mapping, uint32_t at, uint8_t *value)
{
    struct execution_probe *probe = context;
    if (mapping == 1)
        return hpclang_rpl_stock_view_saturn(&probe->view, at, value);
    if (mapping == 2)
        return hpclang_rpl_stock_view_native(&probe->view, at, value);
    if (mapping == 3 && at < 8) {
        *value = (*probe->pc >> (4 * at)) & 15;
        return 1;
    }
    return 0;
}

static int capture_execution(struct hpclang_rpl_mutation_io *io, uint32_t home)
{
    struct execution_probe *probe = calloc(1, sizeof(*probe));
    if (!probe)
        return -1;
    int result = -1;
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    probe->pc = hpclang_launch_execution_pc(boot);
    if (!probe->pc || hpclang_rpl_stock_view_reader(&probe->view) ||
        hpclang_rpl_reference_guards_begin(&probe->builder, probe->guards, 1536) ||
        hpclang_rpl_reference_edits_begin(&probe->plan, probe->edits, 192))
        goto done;
    uint32_t catalog_count;
    if (probe_read_word(io, io->library_table, 3, &catalog_count) ||
        probe_read_word(io, 0x806f3, 5, &probe->move_end))
        goto done;
    probe->move_first = io->library_table + 3 + catalog_count * 16;
    if (probe->move_first >= probe->move_end || probe->move_end > 0xffff0)
        goto done;
    for (unsigned digit = 0; digit < 3; ++digit)
        if (execution_observe(probe, HPCLANG_RPL_EXECUTION_DATA, io->library_table + digit,
                              (catalog_count >> (4 * digit)) & 15))
            goto done;
    for (unsigned digit = 0; digit < 5; ++digit)
        if (execution_observe(probe, HPCLANG_RPL_EXECUTION_DATA, 0x806f3 + digit,
                              (probe->move_end >> (4 * digit)) & 15))
            goto done;
    const struct hpclang_rpl_execution_io execution = {
        {hpclang_rpl_stock_view_native, &probe->view},
        {execution_shadow_read, probe},
        {execution_shadow_read, probe},
        execution_collect_observe,
        execution_token,
        execution_visit,
        probe,
        execution_derive};
    uint32_t steps;
    const struct hpclang_rpl_code_callbacks callbacks = {execution_derive,   execution_store,
                                                         execution_config,   execution_predicate,
                                                         execution_constant, execution_unit};
    int stop = hpclang_rpl_execution_collect_observed(boot, &execution, 0x200000, 92, &probe->state,
                                                      &steps, &callbacks);
    if (stop != HPCLANG_RPL_EXECUTION_BOUNDARY || !probe->plan.count ||
        probe->planned_uses != probe->uses || !probe->first_use.count)
        goto done;
    /* Validate the final overlay with last-writer-wins semantics. Earlier
     * overwritten payloads remain constrained by their retained producers. */
    for (unsigned effect = 0; effect < probe->stores; ++effect) {
        const struct hpclang_rpl_code_store *store = &probe->store_records[effect];
        for (unsigned digit = 0; digit < store->nibbles; ++digit) {
            if (effect == 4)
                continue; /* All five digits are replaced by store six. */
            uint8_t code, data;
            uint32_t token, at = store->address.value + digit;
            uint32_t expected = store->adjustment && digit < 5 ? 0x500001 + 16 * effect + digit
                                                               : store->origins[digit];
            if (!execution.code.read_nibble(execution.code.context, at, &code) ||
                !execution.data.read_nibble(execution.data.context, at, &data) || code != data ||
                data != ((store->value >> (4 * digit)) & 15) ||
                execution_token(probe, 1, at, data, &token) || token != expected)
                goto done;
            ++probe->checked_reads;
        }
    }
    for (unsigned i = 0; i < probe->plan.count; ++i) {
        const struct hpclang_rpl_reference_edit *edit = &probe->edits[i];
        if (edit->expected != edit->value) {
            if (edit->mapping != 1)
                goto done; /* A proposed ROM write invalidates this test plan. */
            ++probe->changed_cells;
        }
    }
    struct hpclang_rpl_reference_edit_builder rejected = probe->plan;
    struct hpclang_rpl_reference_edit conflict = probe->edits[0];
    conflict.value ^= 1;
    if (hpclang_rpl_reference_edits_add(&rejected, &conflict) >= 0 || rejected.count ||
        !rejected.error)
        goto done;
    ++probe->rejected_plans;
    rejected = probe->plan;
    uint32_t replacement = execution_shift(probe, probe->first_use.addresses[0].value);
    if (hpclang_rpl_execution_plan_uses(&rejected, &probe->first_use, &replacement,
                                        execution_stale_source, probe) >= 0 ||
        rejected.count || !rejected.error)
        goto done;
    ++probe->rejected_plans;
    const struct hpclang_rpl_reference_reader guards = {execution_guard_read, probe};
    if (stop != HPCLANG_RPL_EXECUTION_BOUNDARY ||
        hpclang_rpl_reference_guards_validate(&guards, probe->guards, probe->builder.count))
        goto done;
    struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_directory_entry_view entry;
    uint32_t length;
    if (hpclang_rpl_directory_find(&reader, home, (const uint8_t *)"EXECOBS", 7, &entry) ||
        entry.prologue != 0x2a2c || probe_read_word(io, entry.object_address + 5, 5, &length) ||
        length != 237)
        goto done;
    uint32_t words[29] = {9,
                          (uint32_t)stop,
                          steps,
                          probe->state.control.pc,
                          probe->uses,
                          probe->builder.count,
                          probe->observations[0],
                          probe->observations[1],
                          probe->observations[2],
                          probe->observations[3],
                          *probe->pc,
                          0x45584543,
                          probe->derived_uses,
                          probe->derived_source,
                          probe->adjustment,
                          probe->generated_uses,
                          probe->expression_count,
                          probe->expression_uses,
                          probe->expression_pc,
                          probe->expression_sources,
                          probe->planned_uses,
                          probe->plan.count,
                          probe->changed_cells,
                          probe->branches,
                          probe->rejected_plans,
                          probe->stores,
                          probe->stored_sources,
                          probe->checked_reads,
                          probe->d0_derived};
    for (unsigned i = 0; i < 29; ++i)
        if (probe_write_word(io, entry.object_address + 10 + i * 8, words[i], 8))
            goto done;
    result = 0;
done:
    free(probe);
    return result;
}

/* Record entry before library creation and controlled return-path rewrites.
 * ENTRYOBS is a fixed-size test-owned string: no geometry changes are needed. */
static int capture_entry(struct hpclang_rpl_mutation_io *io, uint32_t home)
{
    if (io->native_root_count != 3 || !io->native_roots[1])
        return -1;
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    uint32_t *pc = hpclang_launch_execution_pc(boot);
    if (!pc || pc != io->native_roots[1])
        return -1;
    uint32_t *cpu = (uint32_t *)boot->saturn_state;
    if (!cpu || cpu[0x998 / 4] >= 8)
        return -1;
    /* C still names the ROM helper; R3.H retains the launcher source. */
    uint32_t target = cpu[0x948 / 4];
    if ((target & 3) || target < 0x08000000u || target > 0x08080000u - 32)
        return -1;
    struct hpclang_rpl_reader reader = {io->read_nibble, io->context};
    struct hpclang_rpl_directory_entry_view entry;
    uint32_t length;
    if (hpclang_rpl_directory_find(&reader, home, (const uint8_t *)"ENTRYOBS", 8, &entry) ||
        entry.prologue != 0x2a2c || probe_read_word(io, entry.object_address + 5, 5, &length) ||
        length != 261)
        return -1;
    uint32_t words[32] = {1,
                          (uint32_t)(uintptr_t)cpu,
                          *pc,
                          cpu[0x998 / 4],
                          cpu[0x914 / 4],
                          cpu[0x918 / 4],
                          target,
                          cpu[0x920 / 4],
                          io->native_root_count,
                          *io->native_roots[0],
                          cpu[0x978 / 4 + ((cpu[0x998 / 4] + 7) & 7)]};
    if (probe_read_word(io, 0x8076b, 5, words + 11))
        return -1;
    for (unsigned i = 0; i < 10; ++i)
        words[12 + i] = cpu[0x92c / 4 + i];
    memcpy(words + 22, (const void *)(uintptr_t)target, 32);
    words[30] = 0x454e5452u;
    words[31] = cpu[0x91c / 4];
    for (unsigned i = 0; i < 32; ++i)
        if (probe_write_word(io, entry.object_address + 10 + i * 8, words[i], 8))
            return -1;
    return 0;
}

int hpclang_call_roots_probe(void)
{
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory))
        return 60;
    struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    struct hpclang_rpl_directory_entry_view entry;
    uint32_t home;
    if (probe_read_word(&io, 0x80711, 5, &home))
        return 60;
    int found = hpclang_rpl_directory_find(&reader, home, (const uint8_t *)"CALLROOT", 8, &entry);
    if (found == HPCLANG_RPL_STORE_NOT_FOUND)
        return -1;
    if (found || entry.prologue != 0x2d9d)
        return 60;
    if (capture_entry(&io, home) || capture_execution(&io, home))
        return 64;
    struct hpclang_rpl_reader encoded = {library_nibble, NULL};
    struct hpclang_object *source = NULL, *existing = NULL;
    if (hpclang_object_snapshot_copy(&encoded, 0, sizeof library_encoding - 1, &source))
        return 62;
    existing = hpclang_port_library_get(0, 123);
    int valid = existing ? existing->nibble_count == source->nibble_count &&
                               !memcmp(existing->data, source->data, (source->nibble_count + 1) / 2)
                         : errno == ENOENT && !hpclang_port_library_create(0, source);
    hpclang_object_release(existing);
    hpclang_object_release(source);
    return valid ? probe_register() : 63;
}
