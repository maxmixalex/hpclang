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
    struct hpclang_rpl_reference_guard guards[1152];
    struct hpclang_rpl_reference_guard_builder builder;
    struct hpclang_rpl_stock_view view;
    uint32_t *pc;
    uint32_t observations[4], uses, indirect_targets, derived_uses, derived_source, adjustment;
    uint32_t generated_uses;
    struct hpclang_rpl_execution_subtraction expression, payload_expression, stack_expression;
    uint32_t expression_count, expression_uses, expression_pc, expression_sources;
    struct hpclang_rpl_reference_edit edits[160];
    struct hpclang_rpl_reference_edit_builder plan;
    struct hpclang_rpl_code_step_uses first_use;
    uint32_t move_first, move_end, planned_uses, branches, changed_cells, rejected_plans;
    struct hpclang_rpl_code_store store, store_records[19];
    uint32_t stores, stored_sources, checked_reads, d0_derived;
    struct
    {
        uint32_t field, value, origin;
    } shadow[85];
    uint8_t native_after[0xc60];
    unsigned configurations, unconfigurations, bank_writes, payload_limit, payload_depth;
    struct hpclang_rpl_code_constant constants[7];
    unsigned constant_count, constant_limit, predicates;
    struct hpclang_rpl_code_unit units[3];
    uint64_t unit_inputs[3], unit_outputs[3];
    unsigned unit_count, unit_limit;
    struct hpclang_rpl_code_sum_source sums[16];
    unsigned sum_count, sum_limit;
    struct hpclang_rpl_code_logical_source logical;
    unsigned logical_count, logical_limit;
    struct hpclang_rpl_code_bitshift_source bitshifts[2];
    unsigned bitshift_count, bitshift_limit;
    struct hpclang_rpl_code_native_word native_words[16];
    unsigned native_word_count, native_word_limit, dispatch_roots_checked;
    struct hpclang_rpl_code_store native_saves[2];
    uint32_t native_save_values[2], native_save_count, scan_reads;
};

_Static_assert(sizeof(struct execution_probe) <= 40 * 1024, "execution fixture heap budget");

static int execution_shadow_value(void *context, uint32_t at, uint8_t *value, uint32_t *token);
static int execution_shadow_read(void *context, uint32_t at, uint8_t *value);
static int execution_dispatch_roots(struct execution_probe *probe);

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
    if (probe->expression_count >= 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (!probe->expression_count) {
        probe->expression = *expression;
        probe->expression_pc = pc;
    } else if (probe->expression_count == 1) {
        if (pc != 0x2a19d)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        probe->payload_expression = *expression;
    } else {
        if (pc != 0x2046e)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        probe->stack_expression = *expression;
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

static int execution_address_replacement(const struct execution_probe *probe,
                                         const struct hpclang_rpl_code_address_use *use,
                                         uint32_t *replacement)
{
    unsigned boundary_digits = 0;
    for (unsigned digit = 0; digit < 5; ++digit)
        boundary_digits += use->origins[digit] == 0x806f4 + digit;
    if (boundary_digits == 5) {
        /* catalog_prepare advances RSKTOP explicitly. Its one-past endpoint
         * follows that root even though it is outside the moved byte range.
         * Equal numeric values with other provenance do not gain this rule. */
        if (use->generated || use->adjustment > 0xfffff || probe->move_end >= 0xffff0 ||
            use->value != ((probe->move_end + use->adjustment) & 0xfffff))
            return HPCLANG_RPL_SPAN_MALFORMED;
        *replacement = (probe->move_end + 16 + use->adjustment) & 0xfffff;
    } else {
        *replacement = execution_shift(probe, use->value);
    }
    return 0;
}

/* Normalize aliases through the guarded original map. Store destinations are
 * SRAM only and exclude native CPU state. Original dependency readers never
 * consult this overlay: simulated values must not become original-memory guards. */
static int execution_shadow_value(void *context, uint32_t at, uint8_t *value, uint32_t *token)
{
    struct execution_probe *probe = context;
    /* The second qualified CONFIG activates this I/O window. Its contents
     * cannot be read through the original ordinary-memory mapping. */
    if ((probe->configurations == 2 * (probe->unconfigurations + 1)) && probe->configurations &&
        (at >> 12) == 0x7f)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
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
    if ((probe->configurations == 2 * (probe->unconfigurations + 1)) && probe->configurations &&
        (at >> 12) == 0x7f)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
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
    if (token >= 0xc00001) {
        unsigned index = (token - 0xc00001) / 16, digit = (token - 0xc00001) % 16;
        if (index >= probe->bitshift_count || index >= probe->bitshift_limit ||
            replacement != expected)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_bitshift_source *fact = &probe->bitshifts[index];
        uint32_t affected = ((1u << fact->shift.nibbles) - 1) << fact->shift.first;
        if (fact->adjustment && fact->shift.first < 5)
            affected |= 31u;
        if (!(affected & (1u << digit)) || ((fact->shift.after >> (4 * digit)) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        unsigned limit = probe->bitshift_limit;
        probe->bitshift_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        int result = hpclang_rpl_execution_plan_bitshift(
            builder, fact, fact->shift.before, fact->shift.after, &instructions, execution_token,
            execution_source, execution_payload, probe);
        probe->bitshift_limit = limit;
        return result;
    }
    if (token >= 0xb00001) {
        unsigned digit = token - 0xb00001;
        const struct hpclang_rpl_code_logical_source *fact = &probe->logical;
        if (probe->logical_count != 1 || probe->logical_limit != 1 ||
            digit >= fact->logical.nibbles || replacement != expected)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (((fact->logical.result >> (4 * (fact->logical.first + digit))) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        probe->logical_limit = 0;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        const uint64_t inputs[2] = {fact->logical.left_value, fact->logical.right_value};
        int result = hpclang_rpl_execution_plan_logical(builder, fact, inputs, fact->logical.result,
                                                        &instructions, execution_token,
                                                        execution_source, execution_payload, probe);
        probe->logical_limit = 1;
        return result;
    }
    if (token >= 0xa00001) {
        unsigned index = (token - 0xa00001) / 16, digit = (token - 0xa00001) % 16;
        if (index >= probe->native_word_count || index >= probe->native_word_limit || digit >= 8)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_native_word *fact = &probe->native_words[index];
        if ((fact->reg == HPCLANG_RPL_CODE_B && digit < 5) ||
            ((fact->after >> (4 * digit)) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        uint32_t input = fact->before;
        if (fact->reg == HPCLANG_RPL_CODE_B) {
            struct hpclang_rpl_code_address_use use = {.value = input & 0xfffff,
                                                       .adjustment = fact->adjustment};
            memcpy(use.origins, fact->origins, sizeof use.origins);
            uint32_t address;
            int result = execution_address_replacement(probe, &use, &address);
            if (result)
                return result;
            input = (input & 0xfff00000) | address;
        }
        uint32_t output = input + (uint32_t)fact->delta;
        if (((output >> (4 * digit)) & 15) != replacement)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        unsigned limit = probe->native_word_limit;
        probe->native_word_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        int result = hpclang_rpl_execution_plan_native_word(
            builder, fact, input, output, &instructions, execution_token, execution_source,
            execution_payload, probe);
        probe->native_word_limit = limit;
        return result;
    }
    if (token >= 0x900001) {
        unsigned index = (token - 0x900001) / 16, digit = (token - 0x900001) % 16;
        if (index >= probe->native_save_count || digit >= 8)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_store *fact = &probe->native_saves[index];
        if (((fact->value >> (4 * digit)) & 15) != expected ||
            ((probe->native_save_values[index] >> (4 * digit)) & 15) != replacement)
            return HPCLANG_RPL_SPAN_MALFORMED;
        /* D1 is a canonical 20-bit native register. Its saved upper digits
         * are zero extension, backed by the retained five-digit operand. */
        if (!index && (fact->value > 0xfffff || probe->native_save_values[index] > 0xfffff))
            return HPCLANG_RPL_SPAN_MALFORMED;
        return hpclang_rpl_execution_plan_payload(builder, fact, probe->native_save_values[index],
                                                  execution_source, execution_payload, probe);
    }
    if (token >= 0x800001) {
        unsigned index = (token - 0x800001) / 16, digit = (token - 0x800001) % 16;
        if (index >= probe->sum_count || index >= probe->sum_limit || replacement != expected)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_sum_source *fact = &probe->sums[index];
        if (digit >= fact->sum.nibbles ||
            ((fact->sum.result >> (4 * (fact->sum.first + digit))) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        unsigned limit = probe->sum_limit;
        probe->sum_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        const uint64_t inputs[2] = {fact->sum.left_value, fact->sum.right_value};
        int result = hpclang_rpl_execution_plan_sum(builder, fact, inputs, fact->sum.result,
                                                    &instructions, execution_token,
                                                    execution_source, execution_payload, probe);
        probe->sum_limit = limit;
        return result;
    }
    if (token >= 0x700001) {
        unsigned index = (token - 0x700001) / 16, digit = (token - 0x700001) % 16;
        if (index >= probe->unit_count || index >= probe->unit_limit)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_unit *fact = &probe->units[index];
        if (digit >= fact->nibbles ||
            ((fact->after >> (4 * (fact->first + digit))) & 15) != expected)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (((probe->unit_outputs[index] >> (4 * (fact->first + digit))) & 15) != replacement)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        unsigned limit = probe->unit_limit;
        probe->unit_limit = index;
        const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
        int result = hpclang_rpl_execution_plan_unit(
            builder, fact, probe->unit_inputs[index], probe->unit_outputs[index], &instructions,
            execution_token, execution_source, execution_payload, probe);
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
    if (probe->expression_count > 3 || token < 0x400001 ||
        token >= 0x400001 + 5 * probe->expression_count || replacement != expected)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned index = (token - 0x400001) / 5, digit = (token - 0x400001) % 5;
    const struct hpclang_rpl_execution_subtraction *fact = index == 2 ? &probe->stack_expression
                                                           : index    ? &probe->payload_expression
                                                                      : &probe->expression;
    if (((fact->value >> (4 * digit)) & 15) != expected)
        return HPCLANG_RPL_SPAN_MALFORMED;
    return hpclang_rpl_execution_plan_subtraction(
        builder, fact, fact->left.value, fact->right.value, fact->value, execution_source, probe);
}

static int execution_bank_write(struct execution_probe *probe,
                                const struct hpclang_rpl_code_store *store)
{
    if (probe->bank_writes >= 2 || probe->configurations != 2 * (probe->bank_writes + 1) ||
        probe->unconfigurations != probe->bank_writes ||
        store->instruction != (probe->bank_writes ? 0xb37cu : 0xb2d7u) || store->first ||
        store->nibbles != 2 || execution_shift(probe, store->address.value) != store->address.value)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const uint32_t offsets[] = {0xba0, 0xbb0, 0xbcc, 0xbd0};
    for (unsigned word = 0; word < 4; ++word)
        for (unsigned digit = 0; digit < 8; ++digit) {
            uint8_t value;
            if (!execution_native_read(probe, offsets[word] * 2 + digit, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
        }
    struct hpclang_rpl_bank_write effect;
    int result = hpclang_rpl_bank_write_inspect(probe->native_after, store->address.value, &effect);
    if (result || !effect.unchanged)
        return result ? result : HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_store address = {
        .nibbles = 5, .value = store->address.value, .adjustment = store->address.adjustment};
    memcpy(address.origins, store->address.origins, sizeof store->address.origins);
    result = hpclang_rpl_execution_plan_payload(&probe->plan, &address, address.value,
                                                execution_source, execution_payload, probe);
    if (!result)
        ++probe->bank_writes;
    return result;
}

static int execution_store(void *context, const struct hpclang_rpl_code_store *store)
{
    struct execution_probe *probe = context;
    if (store->instruction == 0xb2d7 || store->instruction == 0xb37c)
        return execution_bank_write(probe, store);
    const unsigned count = store->nibbles;
    /* Nineteen effects observed in the original ROM continuation. This finite
     * fixture permits only five proven same-destination overlaps, without granting
     * general store authority. The overlay is published only after validation. */
    static const uint32_t pcs[19] = {0x2c3c, 0x2a1ba, 0x2a1c1, 0x2a1e0, 0x2da2,  0xb265, 0xb2ce,
                                     0x2da2, 0x2da2,  0x20476, 0x20482, 0x153eb, 0x92f0, 0xb35d,
                                     0xb367, 0x291c,  0x8d1f,  0x2da2,  0x291c};
    static const unsigned widths[19] = {5, 8, 4, 5, 5, 10, 5, 5, 5, 5, 1, 5, 1, 5, 5, 5, 5, 5, 5};
    unsigned overlap_effect = probe->stores == 5    ? 4
                              : probe->stores == 13 ? 5
                              : probe->stores == 14 ? 6
                              : probe->stores == 15 ? 3
                              : probe->stores == 17 ? 13
                                                    : UINT32_MAX;
    if (!count || count > 10 || probe->stores >= 19 || store->instruction != pcs[probe->stores] ||
        store->first != ((probe->stores == 10 || probe->stores == 12) ? 15u : 0u) ||
        count != widths[probe->stores] || store->address.value > 0x100000 - count ||
        (overlap_effect != UINT32_MAX &&
         store->address.value != probe->store_records[overlap_effect].address.value) ||
        (probe->stores >= 13 && (probe->native_save_count != 2 ||
                                 probe->stored_sources != (probe->stores >= 17 ? 80u : 75u))))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (probe->stores == 16) {
        /* This captured clear writes a fixed OS scratch word, after the native
         * saves. Keep those saves in their existing slots and append new cells. */
        if (store->address.value != 0x8082e || (store->value & 0xfffff) ||
            probe->constant_count != 7 || !probe->dispatch_roots_checked)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        for (unsigned digit = 0; digit < 5; ++digit)
            if (store->origins[digit] != 0x600061 + digit)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    if (probe->stores == 18 &&
        (!probe->dispatch_roots_checked || probe->store_records[15].address.value < 5 ||
         store->address.value != probe->store_records[15].address.value - 5))
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
                if (overlap_effect == UINT32_MAX || digit >= 5)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                if (probe->stores >= 13 &&
                    (probe->shadow[prior].origin !=
                         probe->store_records[overlap_effect].origins[digit] ||
                     probe->shadow[prior].value !=
                         ((probe->store_records[overlap_effect].value >> (4 * digit)) & 15)))
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
                slots[digit] = prior;
                overlap = 1;
            }
        if ((overlap_effect != UINT32_MAX && digit < 5) != overlap)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (!overlap && ++added + probe->stored_sources > (probe->stores == 18   ? 85u
                                                           : probe->stores == 16 ? 80u
                                                                                 : 59u))
            return HPCLANG_RPL_SPAN_BOUNDS;
        origins[digit] = store->origins[store->first + digit];
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
    } else if (probe->stores == 4 || probe->stores == 7 || probe->stores == 8 ||
               probe->stores == 17) {
        if (store->adjustment != (probe->stores == 4 ? 5u : probe->stores == 17 ? 15u : 10u))
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
                unsigned offset = cell.field - prior->field;
                if (prior->mapping == cell.mapping && offset < prior->width &&
                    ((prior->expected >> (4 * offset)) & 15) == value) {
                    replacement = (replacement & ~(UINT64_C(15) << (4 * digit))) |
                                  ((uint64_t)((prior->value >> (4 * offset)) & 15) << (4 * digit));
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
        probe->shadow[at].value = (uint32_t)(store->value >> (4 * (store->first + digit))) & 15;
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
    if (probe->constant_count >= 7 || !fact->assignment.nibbles || fact->assignment.nibbles > 16)
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
    if (probe->unit_count >= 3 || !fact->nibbles || fact->nibbles > 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
    uint64_t input = fact->before, output = fact->after;
    if (fact->length == 6) {
        if (fact->instruction != 0xb347 || fact->reg != HPCLANG_RPL_CODE_B || fact->first ||
            fact->nibbles != 5)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        struct hpclang_rpl_code_address_use use = {.value = (uint32_t)input & 0xfffff,
                                                   .adjustment = fact->adjustment};
        memcpy(use.origins, fact->origins, sizeof use.origins);
        uint32_t address;
        int result = execution_address_replacement(probe, &use, &address);
        if (result)
            return result;
        input = (input & ~UINT64_C(0xfffff)) | address;
        struct hpclang_rpl_code_values values = {.decimal = fact->decimal};
        values.registers[fact->reg] = input;
        values.registers[HPCLANG_RPL_CODE_P] = fact->p;
        struct hpclang_rpl_code_unit proposed;
        result = hpclang_rpl_code_unit_inspect(&instructions, fact->instruction,
                                               0x100000 - fact->instruction, &values, &proposed);
        if (result)
            return result;
        output = proposed.after;
    }
    int result = hpclang_rpl_execution_plan_unit(&probe->plan, fact, input, output, &instructions,
                                                 execution_token, execution_source,
                                                 execution_payload, probe);
    if (result)
        return result;
    unsigned index = probe->unit_count;
    probe->units[index] = *fact;
    probe->unit_inputs[index] = input;
    probe->unit_outputs[index] = output;
    for (unsigned i = 0; i < fact->nibbles; ++i)
        tokens[i] = 0x700001 + 16 * index + i;
    probe->unit_limit = ++probe->unit_count;
    return 0;
}

static int execution_sum(void *context, const struct hpclang_rpl_code_sum_source *fact,
                         uint32_t tokens[16])
{
    struct execution_probe *probe = context;
    if (probe->sum_count >= sizeof probe->sums / sizeof probe->sums[0] || !fact->sum.nibbles ||
        fact->sum.nibbles > 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
    const uint64_t inputs[2] = {fact->sum.left_value, fact->sum.right_value};
    int result =
        hpclang_rpl_execution_plan_sum(&probe->plan, fact, inputs, fact->sum.result, &instructions,
                                       execution_token, execution_source, execution_payload, probe);
    if (result)
        return result;
    unsigned index = probe->sum_count;
    probe->sums[index] = *fact;
    for (unsigned i = 0; i < fact->sum.nibbles; ++i)
        tokens[i] = 0x800001 + 16 * index + i;
    probe->sum_limit = ++probe->sum_count;
    return 0;
}

static int execution_bitshift(void *context, const struct hpclang_rpl_code_bitshift_source *fact,
                              uint32_t tokens[16])
{
    struct execution_probe *probe = context;
    static const uint32_t pcs[2] = {0xbc22, 0xbc27};
    if (probe->bitshift_count >= 2 || fact->shift.instruction != pcs[probe->bitshift_count] ||
        fact->shift.reg != HPCLANG_RPL_CODE_A || fact->shift.first || fact->shift.nibbles != 2)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
    int result = hpclang_rpl_execution_plan_bitshift(
        &probe->plan, fact, fact->shift.before, fact->shift.after, &instructions, execution_token,
        execution_source, execution_payload, probe);
    if (result)
        return result;
    unsigned index = probe->bitshift_count;
    probe->bitshifts[index] = *fact;
    unsigned count = fact->adjustment ? 5 : fact->shift.nibbles;
    for (unsigned i = 0; i < count; ++i)
        tokens[i] = 0xc00001 + 16 * index + i;
    probe->bitshift_limit = ++probe->bitshift_count;
    return 0;
}

static int execution_logical(void *context, const struct hpclang_rpl_code_logical_source *fact,
                             uint32_t tokens[16])
{
    struct execution_probe *probe = context;
    if (probe->logical_count || fact->logical.instruction != 0xbc1b ||
        fact->logical.left != HPCLANG_RPL_CODE_C || fact->logical.right != HPCLANG_RPL_CODE_A ||
        fact->logical.first || fact->logical.nibbles != 2 || fact->logical.disjunction)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
    const uint64_t inputs[2] = {fact->logical.left_value, fact->logical.right_value};
    int result = hpclang_rpl_execution_plan_logical(
        &probe->plan, fact, inputs, fact->logical.result, &instructions, execution_token,
        execution_source, execution_payload, probe);
    if (result)
        return result;
    probe->logical = *fact;
    for (unsigned i = 0; i < fact->logical.nibbles; ++i)
        tokens[i] = 0xb00001 + i;
    probe->logical_count = probe->logical_limit = 1;
    return 0;
}

static int execution_native_word(void *context, const struct hpclang_rpl_code_native_word *fact,
                                 uint32_t tokens[8])
{
    struct execution_probe *probe = context;
    if (probe->native_word_count >= 16 ||
        (fact->instruction != 0x3130 && fact->instruction != 0x2da2))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned index = probe->native_word_count;
    probe->native_words[index] = *fact;
    for (unsigned digit = fact->reg == HPCLANG_RPL_CODE_B ? 5 : 0; digit < 8; ++digit)
        tokens[digit] = 0xa00001 + 16 * index + digit;
    probe->native_word_limit = ++probe->native_word_count;
    return 0;
}

static int execution_predicate(void *context, const struct hpclang_rpl_code_predicate *predicate,
                               const struct hpclang_rpl_code_execution *before)
{
    struct execution_probe *probe = context;
    static const uint32_t pcs[] = {0xb283, 0xb2c2, 0xb2e5, 0xb1e9, 0xbc04, 0xbc10};
    if (probe->predicates >= 6 || predicate->instruction != pcs[probe->predicates] ||
        predicate->left != (probe->predicates == 4 ? HPCLANG_RPL_CODE_A : HPCLANG_RPL_CODE_C) ||
        predicate->right != ((probe->predicates < 2 || probe->predicates == 4)
                                 ? HPCLANG_RPL_CODE_CONSTANT
                                 : HPCLANG_RPL_CODE_A) ||
        predicate->first || predicate->nibbles != (probe->predicates ? 5u : 3u))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const uint64_t replacements[2] = {before->values.registers[predicate->left],
                                      predicate->right == HPCLANG_RPL_CODE_CONSTANT
                                          ? 0
                                          : before->values.registers[predicate->right]};
    probe->payload_limit = probe->stores;
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
    unsigned p = (unsigned)state->values.registers[HPCLANG_RPL_CODE_P];
    if (probe->native_after[0x95c] != p) {
        int result =
            hpclang_rpl_stock_p_predict(probe->native_after, sizeof probe->native_after, p);
        if (result)
            return result;
    }
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
        uint32_t replacement;
        int address_result = execution_address_replacement(probe, use, &replacement);
        if (address_result)
            return address_result;
        unsigned sum_digits = 0, primary_digits = 0;
        for (unsigned digit = 0; digit < 5; ++digit) {
            sum_digits += use->origins[digit] >= 0x400001;
            primary_digits += use->origins[digit] == 0x400001 + digit;
        }
        if (sum_digits && primary_digits != 5) {
            if (use->generated)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            struct hpclang_rpl_code_store operand = {
                .first = 0, .nibbles = 5, .value = use->value, .adjustment = use->adjustment};
            for (unsigned digit = 0; digit < 5; ++digit)
                operand.origins[digit] = use->origins[digit];
            probe->payload_limit = probe->stores;
            int result = hpclang_rpl_execution_plan_payload(
                &probe->plan, &operand, replacement, execution_source, execution_payload, probe);
            if (result)
                return result;
            ++probe->planned_uses;
            continue;
        }
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
                replacement, execution_source, probe);
            if (result)
                return result;
        }
        if (use->generated) {
            const struct hpclang_rpl_reader instructions = {execution_plan_read, probe};
            const struct hpclang_rpl_reference_mappings mappings = {1, 1, 1};
            struct hpclang_rpl_code_patch patch;
            int result = hpclang_rpl_code_generated_return_plan(&instructions, use, &mappings,
                                                                probe->move_first, probe->move_end,
                                                                16, replacement, &patch);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
            ++probe->generated_uses;
        }
        if (!expression_digits && !use->generated) {
            const struct hpclang_rpl_code_step_uses single = {.count = 1, .addresses = {*use}};
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
    if (state->control.pc == 0x3130 && probe->bank_writes == 2 && !probe->dispatch_roots_checked) {
        int result = execution_dispatch_roots(probe);
        if (result)
            return result;
        probe->dispatch_roots_checked = 1;
    }
    return state->control.pc == 0xbc4b && probe->bitshift_count == 2 && probe->logical_count == 1 &&
           probe->dispatch_roots_checked && probe->native_word_count == 16 &&
           probe->bank_writes == 2 && probe->expression_uses == 1 && probe->stores == 19 &&
           probe->configurations == 4 && probe->unconfigurations == 2 && probe->predicates == 6 &&
           probe->constant_count == 7 && probe->unit_count == 3 && probe->sum_count == 9 &&
           probe->expression_count == 3 && probe->native_save_count == 2 && probe->scan_reads == 2;
    /* This selected boundary is not a complete relocation certificate. */
}

/* Qualify two captured CONFIG effects: flags, then one I/O dispatch window.
 * Ordinary address maps remain identical; shadow reads/writes reject the new
 * I/O window. Original guards always retain original, not simulated, bytes. */
static int execution_config(void *context, const struct hpclang_rpl_code_configuration *config)
{
    struct execution_probe *probe = context;
    static const uint32_t pcs[] = {0xb247, 0xb2d1, 0xb371, 0xb376};
    if (probe->configurations >= 4 || config->instruction != pcs[probe->configurations] ||
        probe->unconfigurations != probe->configurations / 2 ||
        (probe->configurations >= 2 && (probe->bank_writes != 1 || probe->stores != 15)))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned phase = probe->configurations & 1;
    const struct hpclang_rpl_code_store source = {
        .nibbles = 5, .value = config->value, .adjustment = config->adjustment};
    struct hpclang_rpl_code_store input = source;
    memcpy(input.origins, config->origins, sizeof config->origins);
    probe->payload_limit = probe->stores;
    int result = hpclang_rpl_execution_plan_payload(&probe->plan, &input, input.value,
                                                    execution_source, execution_payload, probe);
    if (result)
        return result;
    for (unsigned at = 0; at < sizeof probe->native_after; ++at) {
        uint8_t low, high;
        if (!execution_native_read(probe, at * 2, &low) ||
            !execution_native_read(probe, at * 2 + 1, &high))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        uint8_t original = low | (high << 4);
        if (at == 0xb9c && original != 3)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (probe->configurations &&
            probe->native_after[at] != (at == 0xb9c && phase ? 35 : original))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (!probe->configurations)
            probe->native_after[at] = original;
    }
    if (probe->native_after[0xb9c] != (phase ? 35 : 3))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    /* The predictor permits exact input/output aliasing. Original bytes remain
     * guarded in native memory, so a second full snapshot is unnecessary. */
    result = hpclang_rpl_config_predict(probe->native_after, config->value, probe->native_after);
    if (result)
        return result;
    if (probe->native_after[0xb9c] != (phase ? 7 : 35))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (phase)
        for (unsigned i = 0; i < 4; ++i)
            if (probe->native_after[0xbb0 + i] != (i ? 0 : 0x7f))
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
    for (unsigned at = 0; at < sizeof probe->native_after; ++at) {
        uint8_t low, high;
        if (!execution_native_read(probe, at * 2, &low) ||
            !execution_native_read(probe, at * 2 + 1, &high))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        uint8_t original = low | (high << 4);
        if (phase && at >= 0xbb0 && at < 0xbb4) {
            if (original != 255)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        } else if (at != 0xb9c && original != probe->native_after[at]) {
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        }
    }
    ++probe->configurations;
    return 0;
}

/* This path cancels only the bank-selector window. RAM unconfiguration needs
 * a separately qualified map rebuild and cannot reuse this flags-only effect. */
static int execution_unconfig(void *context, const struct hpclang_rpl_code_configuration *config)
{
    struct execution_probe *probe = context;
    if (probe->unconfigurations >= 2 ||
        probe->configurations != 2 * (probe->unconfigurations + 1) ||
        probe->bank_writes != probe->unconfigurations + 1 ||
        config->instruction != (probe->unconfigurations ? 0xb37fu : 0xb2dau) ||
        probe->native_after[0xb9c] != 7)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_unconfig effect;
    int result = hpclang_rpl_unconfig_inspect(probe->native_after, config->value, &effect);
    if (result)
        return result;
    if (effect.kind != HPCLANG_RPL_UNCONFIG_BANK || effect.rebuild || effect.flags != 3 ||
        effect.clear_field != 0xbb0)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_store input = {
        .nibbles = 5, .value = config->value, .adjustment = config->adjustment};
    memcpy(input.origins, config->origins, sizeof config->origins);
    probe->payload_limit = probe->stores;
    result = hpclang_rpl_execution_plan_payload(&probe->plan, &input, input.value, execution_source,
                                                execution_payload, probe);
    if (result)
        return result;
    /* Both CONFIG callbacks captured the entire original native state. Keep
     * those original guards; only advance the private simulation here. */
    probe->native_after[0xb9c] = 3;
    memset(probe->native_after + 0xbb0, 255, 4);
    ++probe->unconfigurations;
    return 0;
}

static int execution_ram_read(struct execution_probe *probe, uint32_t at, uint8_t *value)
{
    if (at >= 0x100000 || !probe->view.sram || !probe->view.ram_section ||
        *probe->view.ram_section != probe->view.expected_ram)
        return 0;
    *value = (probe->view.sram[at / 2] >> (4 * (at & 1))) & 15;
    return 1;
}

static int execution_scan_saves(struct execution_probe *probe,
                                const struct hpclang_rpl_code_execution *before)
{
    if (probe->native_save_count || probe->stores != 13 || probe->stored_sources != 59 ||
        probe->view.state_offset > 0x80000 - 0xcb8)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_store facts[2] = {{0}};
    uint32_t replacements[2];
    for (unsigned word = 0; word < 2; ++word) {
        unsigned reg = word ? HPCLANG_RPL_CODE_B : HPCLANG_RPL_CODE_D1;
        facts[word].instruction = before->control.pc;
        facts[word].nibbles = word ? 8 : 5;
        facts[word].value = (uint32_t)before->values.registers[reg];
        facts[word].adjustment = before->origins.adjustment[reg];
        memcpy(facts[word].origins, before->origins.registers[reg], sizeof facts[word].origins);
        if (!word && facts[word].value > 0xfffff)
            return HPCLANG_RPL_SPAN_MALFORMED;
        struct hpclang_rpl_code_address_use use = {.value = (uint32_t)facts[word].value & 0xfffff,
                                                   .adjustment = facts[word].adjustment};
        memcpy(use.origins, facts[word].origins, sizeof use.origins);
        uint32_t address;
        int result = execution_address_replacement(probe, &use, &address);
        if (result)
            return result;
        replacements[word] = ((uint32_t)facts[word].value & 0xfff00000) | address;
        probe->payload_limit = probe->stores;
        result = hpclang_rpl_execution_plan_payload(&probe->plan, &facts[word], replacements[word],
                                                    execution_source, execution_payload, probe);
        if (result)
            return result;
        for (unsigned digit = 0; digit < 8; ++digit) {
            uint32_t field = (probe->view.state_offset + 0xcb0 + 4 * word) * 2 + digit;
            for (unsigned prior = 0; prior < probe->stored_sources; ++prior)
                if (probe->shadow[prior].field == field)
                    return HPCLANG_RPL_SPAN_UNSUPPORTED;
            uint8_t value;
            if (!execution_ram_read(probe, field, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            result = hpclang_rpl_reference_guards_observe(&probe->builder, 4, field, value);
            if (result)
                return result;
        }
    }
    /* Publish both owned scratch words only after all source and destination
     * checks. CPU-relative saves do not go through the Saturn Write[] table. */
    for (unsigned word = 0; word < 2; ++word) {
        probe->native_saves[word] = facts[word];
        probe->native_save_values[word] = replacements[word];
        for (unsigned digit = 0; digit < 8; ++digit) {
            unsigned slot = probe->stored_sources++;
            probe->shadow[slot].field = (probe->view.state_offset + 0xcb0 + 4 * word) * 2 + digit;
            probe->shadow[slot].value = (facts[word].value >> (4 * digit)) & 15;
            probe->shadow[slot].origin = 0x900001 + 16 * word + digit;
        }
    }
    probe->native_save_count = 2;
    return 0;
}

struct execution_scan_context
{
    struct execution_probe *probe;
    const struct hpclang_rpl_code_execution *before;
    uint32_t pointer, pointer_origins[5];
};

static int execution_scan_read(void *context, const struct hpclang_rpl_code_scan_read *read)
{
    struct execution_scan_context *scan = context;
    struct execution_probe *probe = scan->probe;
    unsigned index = probe->scan_reads;
    if (index >= 2 ||
        read->kind != (index ? HPCLANG_RPL_CODE_SCAN_TAG : HPCLANG_RPL_CODE_SCAN_POINTER) ||
        read->address !=
            (index ? scan->pointer : scan->before->values.registers[HPCLANG_RPL_CODE_D0]) ||
        (read->address & 7))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    /* The selected continuation uses aligned, same-page native 32-bit reads.
     * Retain all eight padded digits, not only the five consumed by Saturn. */
    for (unsigned digit = 0; digit < 8; ++digit) {
        struct hpclang_rpl_code_origin_cell cell;
        int result = execution_source(probe, read->address + digit + 1, &cell);
        if (result)
            return result;
        uint32_t saves_first = (probe->view.state_offset + 0xcb0) * 2;
        if (cell.mapping == 1 && cell.field >= saves_first && cell.field < saves_first + 16)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        uint8_t value;
        if (!execution_shadow_read(probe, read->address + digit, &value))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
    }
    struct hpclang_rpl_code_step_uses uses = {.count = 1};
    uses.addresses[0].kind = HPCLANG_RPL_CODE_USE_LOAD_ADDRESS;
    uses.addresses[0].value = read->address;
    uses.addresses[0].adjustment =
        index ? 0 : scan->before->origins.adjustment[HPCLANG_RPL_CODE_D0];
    memcpy(uses.addresses[0].origins,
           index ? scan->pointer_origins : scan->before->origins.registers[HPCLANG_RPL_CODE_D0],
           sizeof uses.addresses[0].origins);
    int result = execution_visit(probe, scan->before->control.pc, scan->before, &uses);
    if (result)
        return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
    if (index && (read->value == 0x28fc || read->value == 0x3130))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_code_store operand = {.nibbles = 5, .value = read->value};
    for (unsigned digit = 0; digit < 5; ++digit) {
        result = execution_token(probe, 1, read->address + digit, (read->value >> (4 * digit)) & 15,
                                 &operand.origins[digit]);
        if (result)
            return result;
    }
    uint32_t replacement = index ? read->value : execution_shift(probe, read->value);
    probe->payload_limit = probe->stores;
    result = hpclang_rpl_execution_plan_payload(&probe->plan, &operand, replacement,
                                                execution_source, execution_payload, probe);
    if (result)
        return result;
    if (!index) {
        scan->pointer = read->value;
        memcpy(scan->pointer_origins, operand.origins, sizeof scan->pointer_origins);
    }
    ++probe->scan_reads;
    return 0;
}

static int execution_scan(void *context, const struct hpclang_rpl_code_native_scan *effect,
                          const struct hpclang_rpl_code_execution *before,
                          struct hpclang_rpl_code_scan_origins *origins)
{
    (void)origins; /* This maintained fixture selects the ordinary return path. */
    struct execution_probe *probe = context;
    if (probe->view.state_offset > 0x80000 - 0xcb8 || before->control.pc != 0x301c ||
        !effect->returns || effect->reads != 2 || probe->scan_reads ||
        before->values.registers[HPCLANG_RPL_CODE_P] || (before->status & 2) ||
        effect->d0 != before->values.registers[HPCLANG_RPL_CODE_D0] + 5 ||
        effect->b != before->values.registers[HPCLANG_RPL_CODE_B] ||
        effect->d1 != before->values.registers[HPCLANG_RPL_CODE_D1])
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    /* Original ROM's P setter returns immediately when old P equals zero.
     * Retain the producing P relation; the scan performs no cache writes. */
    struct hpclang_rpl_code_store p = {
        .nibbles = 1, .value = 0, .adjustment = before->origins.adjustment[HPCLANG_RPL_CODE_P]};
    p.origins[0] = before->origins.registers[HPCLANG_RPL_CODE_P][0];
    int result = hpclang_rpl_execution_plan_payload(&probe->plan, &p, 0, execution_source,
                                                    execution_payload, probe);
    if (result)
        return result;
    const struct hpclang_rpl_reader reader = {execution_shadow_read, probe};
    struct hpclang_rpl_code_native_scan replay;
    struct execution_scan_context scan = {probe, before, 0, {0}};
    result = hpclang_rpl_code_native_scan_observed(&reader, &reader, 4, before, 2,
                                                   execution_scan_read, &scan, &replay);
    if (result || memcmp(effect, &replay, sizeof replay))
        return result ? result : HPCLANG_RPL_SPAN_MALFORMED;
    return execution_scan_saves(probe, before);
}

static int execution_guard_read(void *context, uint32_t mapping, uint32_t at, uint8_t *value)
{
    struct execution_probe *probe = context;
    if (mapping == 1)
        return hpclang_rpl_stock_view_saturn(&probe->view, at, value);
    if (mapping == 2)
        return hpclang_rpl_stock_view_native(&probe->view, at, value);
    if (mapping == 4)
        return execution_ram_read(probe, at, value);
    if (mapping == 3 && at < 8) {
        *value = (*probe->pc >> (4 * at)) & 15;
        return 1;
    }
    return 0;
}

/* These are the two live relations at this captured dispatcher boundary, not
 * a certificate for arbitrary interpreter continuations. Preserve lineage as
 * well as values: a coincidentally equal scalar is not an address/free-count root. */
static int execution_dispatch_roots(struct execution_probe *probe)
{
    if (probe->state.control.pc != 0x3130 || probe->unit_count != 3 ||
        probe->native_word_count != 13)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const uint32_t anchors[] = {0x806f3, 0x80e9b};
    uint32_t values[2] = {0};
    for (unsigned anchor = 0; anchor < 2; ++anchor)
        for (unsigned digit = 0; digit < 5; ++digit) {
            uint8_t value;
            uint32_t token, at = anchors[anchor] + digit;
            if (!execution_shadow_read(probe, at, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            int result = execution_token(probe, 1, at, value, &token);
            if (result)
                return result;
            if (token != at + 1)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            values[anchor] |= (uint32_t)value << (4 * digit);
        }
    uint32_t rstk = values[0], available = values[1];
    if (rstk < 10 || rstk > 0xfffef || available > 0xffffd || probe->move_end != rstk)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    const struct hpclang_rpl_code_unit *unit = &probe->units[2];
    if (unit->instruction != 0xb347 || unit->length != 6 || unit->reg != HPCLANG_RPL_CODE_B ||
        unit->first || unit->nibbles != 5 || unit->decrement != 1 || unit->carry ||
        unit->adjustment != 0xffffb || (unit->before & 0xfffff) != rstk - 5 ||
        unit->after != unit->before - 5 ||
        probe->state.values.registers[HPCLANG_RPL_CODE_B] != unit->after ||
        probe->state.origins.adjustment[HPCLANG_RPL_CODE_B] ||
        probe->unit_inputs[2] != unit->before + 16 || probe->unit_outputs[2] != unit->after + 16)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    for (unsigned digit = 0; digit < 5; ++digit)
        if (unit->origins[digit] != 0x806f4 + digit ||
            probe->state.origins.registers[HPCLANG_RPL_CODE_B][digit] != 0x700021 + digit)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t value = (uint32_t)probe->state.values.registers[HPCLANG_RPL_CODE_D];
    uint32_t adjustment = probe->state.origins.adjustment[HPCLANG_RPL_CODE_D];
    const uint32_t *origins = probe->state.origins.registers[HPCLANG_RPL_CODE_D];
    if (value != available + 2)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    unsigned limit = probe->native_word_count;
    while (origins[0] >= 0xa00001) {
        unsigned index = (origins[0] - 0xa00001) / 16;
        if (index >= limit || adjustment > 0xfffff)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        for (unsigned digit = 0; digit < 5; ++digit)
            if (origins[digit] != 0xa00001 + 16 * index + digit)
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
        const struct hpclang_rpl_code_native_word *word = &probe->native_words[index];
        if (word->reg != HPCLANG_RPL_CODE_D || word->instruction != 0x3130 || word->delta != 1 ||
            word->before >= 0xfffff || word->after != word->before + 1 ||
            value != ((word->after + adjustment) & 0xfffff))
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        value = word->before;
        adjustment = word->adjustment;
        origins = word->origins;
        limit = index;
    }
    if (adjustment > 0xfffff || value != ((available + adjustment) & 0xfffff))
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    for (unsigned digit = 0; digit < 5; ++digit)
        if (origins[digit] != 0x80e9c + digit)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
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
        hpclang_rpl_reference_guards_begin(&probe->builder, probe->guards, 1152) ||
        hpclang_rpl_reference_edits_begin_packed(&probe->plan, probe->edits, 160))
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
    const struct hpclang_rpl_code_callbacks callbacks = {.derive = execution_derive,
                                                         .store = execution_store,
                                                         .config = execution_config,
                                                         .predicate = execution_predicate,
                                                         .constant = execution_constant,
                                                         .unit = execution_unit,
                                                         .sum = execution_sum,
                                                         .unconfig = execution_unconfig,
                                                         .native_word = execution_native_word,
                                                         .native_scan = execution_scan,
                                                         .native_scan_max_reads = 2,
                                                         .logical = execution_logical,
                                                         .bitshift = execution_bitshift};
    int stop = hpclang_rpl_execution_collect_observed(boot, &execution, 0x200000, 258,
                                                      &probe->state, &steps, &callbacks);
    if (stop != HPCLANG_RPL_EXECUTION_BOUNDARY || !probe->plan.count ||
        probe->planned_uses != probe->uses || !probe->first_use.count ||
        probe->native_word_count != 16 || !probe->dispatch_roots_checked)
        goto done;
    uint32_t original_status = 0;
    for (unsigned digit = 0; digit < 8; ++digit) {
        uint8_t value;
        if (!execution_native_read(probe, 0x968 * 2 + digit, &value))
            goto done;
        original_status |= (uint32_t)value << (4 * digit);
    }
    if (probe->state.status != (original_status & ~UINT32_C(3)))
        goto done; /* 841 and the later 840 clear software status bits one and zero. */
    /* Validate the final overlay with last-writer-wins semantics. Earlier
     * overwritten payloads remain constrained by their retained producers. */
    for (unsigned effect = 0; effect < probe->stores; ++effect) {
        const struct hpclang_rpl_code_store *store = &probe->store_records[effect];
        for (unsigned digit = 0; digit < store->nibbles; ++digit) {
            if (effect == 3 && probe->stores >= 16)
                continue; /* Store sixteen replaces the earlier saved pointer. */
            if (effect == 13 && probe->stores >= 18)
                continue; /* The next native stack save replaces this payload. */
            if (effect == 4)
                continue; /* All five digits are replaced by store six. */
            if (effect == 5 && probe->stores >= 14 && digit < 5)
                continue; /* Store fourteen replaces only the first half. */
            if (effect == 6 && probe->stores >= 15)
                continue; /* Store fifteen replaces the saved address. */
            uint8_t code, data;
            uint32_t token, at = store->address.value + digit;
            unsigned source_digit = store->first + digit;
            uint32_t expected = store->adjustment && source_digit < 5
                                    ? 0x500001 + 16 * effect + source_digit
                                    : store->origins[source_digit];
            if (!execution.code.read_nibble(execution.code.context, at, &code) ||
                !execution.data.read_nibble(execution.data.context, at, &data) || code != data ||
                data != ((store->value >> (4 * source_digit)) & 15) ||
                execution_token(probe, 1, at, data, &token) || token != expected)
                goto done;
            ++probe->checked_reads;
        }
    }
    for (unsigned word = 0; word < probe->native_save_count; ++word)
        for (unsigned digit = 0; digit < 8; ++digit) {
            unsigned slot = 59 + 8 * word + digit;
            if (probe->shadow[slot].field !=
                    (probe->view.state_offset + 0xcb0 + 4 * word) * 2 + digit ||
                probe->shadow[slot].value !=
                    ((probe->native_saves[word].value >> (4 * digit)) & 15) ||
                probe->shadow[slot].origin != 0x900001 + 16 * word + digit)
                goto done;
            ++probe->checked_reads;
        }
    unsigned source_cells = 0;
    for (unsigned i = 0; i < probe->plan.count; ++i) {
        const struct hpclang_rpl_reference_edit *edit = &probe->edits[i];
        source_cells += edit->width;
        if (edit->expected != edit->value) {
            if (edit->mapping != 1)
                goto done; /* A proposed ROM write invalidates this test plan. */
            for (unsigned digit = 0; digit < edit->width; ++digit)
                if (((edit->expected ^ edit->value) >> (4 * digit)) & 15)
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
        length != 253)
        goto done;
    uint32_t words[31] = {28,
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
                          source_cells,
                          probe->changed_cells,
                          probe->branches,
                          probe->rejected_plans,
                          probe->stores,
                          probe->stored_sources,
                          probe->checked_reads,
                          probe->d0_derived,
                          probe->native_save_count,
                          probe->scan_reads};
    for (unsigned i = 0; i < 31; ++i)
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
