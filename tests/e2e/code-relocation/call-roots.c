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
extern volatile uint32_t hpclang_e2e_result;
static int prepare_call(struct hpclang_rpl_mutation_io *, uint32_t **, int *);

/* Controlled Stock fixture only: the clean test catalog and fixture programs
 * supply the input provenance. Opaque execution coverage stays unresolved;
 * this is not a public registration or recovery implementation. */
struct live_probe
{
    struct hpclang_rpl_mutation_io *io;
    uint32_t first, end, roots[2048], root_count;
    int32_t delta;
    struct hpclang_rpl_reference_edit_builder builder;
    struct hpclang_rpl_reference_edit edits[4096];
    struct hpclang_rpl_reference_guard_builder guard_builder;
    struct hpclang_rpl_reference_guard guards[4096];
    struct hpclang_rpl_reference_node work[512];
};
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
    p->roots[p->root_count++] = slot;
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
        if (p->roots[i] == edge->field)
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
    return hpclang_rpl_reference_visit_guarded_nested(reader, &node, p->work, 512, 10000,
                                                      probe_edge, probe_dependency, p);
}
static int probe_collect(struct live_probe *p, struct hpclang_rpl_reader *reader, uint32_t home,
                         uint32_t bottom, uint32_t top)
{
    hpclang_rpl_reference_edits_begin(&p->builder, p->edits, 4096);
    hpclang_rpl_reference_guards_begin(&p->guard_builder, p->guards, 4096);
    int result = 6;
    if (hpclang_rpl_mutation_visit_guarded_roots(p->io, probe_root, probe_dependency, p))
        return result;
    result = 7;
    if (probe_node(p, reader, home, 0x100000 - home))
        return result;
    uint32_t at = top;
    while (at > bottom + 5) {
        uint32_t size;
        if (probe_read_word(p->io, at - 5, 5, &size) || size < 11 || size > at - bottom - 5)
            return result;
        uint32_t object = at - size + 1, stop = at - 5;
        while (object < stop) {
            uint32_t span;
            if (hpclang_rpl_object_span(reader, object, stop - object, &span) || !span ||
                probe_node(p, reader, object, span))
                return result;
            object += span;
        }
        at -= size;
    }
    const struct hpclang_rpl_reference_reader mapped = {probe_mapped_read, p->io};
    result = 8;
    if (hpclang_rpl_reference_edits_validate(&mapped, p->edits, p->builder.count))
        return result;
    return 0;
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
    uint8_t *snapshot = malloc(port - body);
    if (!snapshot)
        return -1;
    int result = -1;
    for (uint32_t i = 0; i < port - body; ++i)
        if (!io->read_nibble(io->context, body + i, snapshot + i))
            goto done;
    if (hpclang_rpl_mutation_home_detach(io, 275))
        goto done;
    uint32_t value;
    if (probe_read_word(io, 0x80711, 5, &value) || value != home + 13 ||
        probe_read_word(io, value + 5, 3, &value) || value != count - 1 ||
        probe_read_word(io, 0x80716, 5, &value) || value != port)
        goto done;
    if (reattach &&
        (hpclang_rpl_mutation_home_attach(io, 275, first, second) ||
         probe_read_word(io, 0x80711, 5, &value) || value != home ||
         probe_read_word(io, value + 5, 3, &value) || value != count ||
         probe_read_word(io, 0x80716, 5, &value) || value != port || probe_attached_rejection(io)))
        goto done;
    for (uint32_t i = 0; i < port - body; ++i) {
        uint8_t actual;
        if (!io->read_nibble(io->context, body + i, &actual) || actual != snapshot[i])
            goto done;
    }
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
    uint8_t *snapshot = remove ? malloc(snapshot_size) : NULL;
    uint32_t native_before[3], avmem_before;
    if (remove) {
        if (!snapshot || probe_read_word(&io, 0x80e9b, 5, &avmem_before)) {
            free(snapshot);
            free(p);
            return 9;
        }
        for (uint32_t i = 0; i < snapshot_size; ++i)
            if (!io.read_nibble(io.context, io.library_table + i, snapshot + i)) {
                free(snapshot);
                free(p);
                return 9;
            }
        for (unsigned i = 0; i < 3; ++i)
            native_before[i] = *io.native_roots[i];
    }
    p->io = &io;
    p->first = insertion.slot;
    p->end = end;
    p->delta = 16;
    int result = probe_collect(p, &reader, home, bottom, top);
    if (result)
        goto done;
    const struct hpclang_rpl_catalog_repairs repairs = {
        1, p->edits, p->builder.count, p->guards, p->guard_builder.count, NULL, 0};
    /* After this boundary an error must halt the test, never return Stock. */
    const struct hpclang_rpl_library_binding added = {binding, 0, 0};
    if (hpclang_rpl_catalog_insert_repaired(&io, 123, &added, &insertion, &repairs)) {
        hpclang_e2e_result = 0x46410ffe;
        for (;;)
            __asm__ volatile("nop");
    }
    if (remove) {
        struct hpclang_rpl_catalog_removal removal;
        memset(p, 0, sizeof *p);
        p->io = &io;
        p->first = insertion.slot + 16;
        p->end = end + 16;
        p->delta = -16;
        uint32_t current_home;
        if (probe_read_word(&io, 0x80711, 5, &current_home) ||
            probe_collect(p, &reader, current_home, bottom + 16, top + 16) ||
            hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 0x100000 - io.library_table,
                                            123, &removal) != 1)
            goto failed_roundtrip;
        const struct hpclang_rpl_catalog_repairs undo = {
            1, p->edits, p->builder.count, p->guards, p->guard_builder.count, NULL, 0};
        if (hpclang_rpl_catalog_remove_repaired(&io, 123, &removal, &undo))
            goto failed_roundtrip;
        for (uint32_t i = 0; i < snapshot_size; ++i) {
            uint8_t value;
            if (!io.read_nibble(io.context, io.library_table + i, &value) || value != snapshot[i])
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
        free(p);
        p = NULL;
        if (probe_detach(&io, remove == 3))
            goto failed_roundtrip;
    }
    result = 0;
done:
    free(snapshot);
    free(p);
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
    if (length == 23 && !probe_read_word(io, object + 26, 2, &encoded) && encoded <= 2) {
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

static const char library_encoding[] = "04b2062000108510b7000000000000000000000d5c2";
static int library_nibble(void *context, uint32_t at, uint8_t *out)
{
    (void)context;
    if (at >= sizeof library_encoding - 1)
        return 0;
    *out =
        library_encoding[at] <= '9' ? library_encoding[at] - '0' : library_encoding[at] - 'a' + 10;
    return 1;
}

/* Record entry before library creation and controlled return-path rewrites.
 * ENTRYOBS is a fixed-size test-owned string: no geometry changes are needed. */
static int capture_entry(struct hpclang_rpl_mutation_io *io, uint32_t home)
{
    if (io->native_root_count != 3 || !io->native_roots[1])
        return -1;
    uint32_t *cpu = (uint32_t *)(uintptr_t)io->native_roots[1][1];
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
                          *io->native_roots[1],
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
    if (capture_entry(&io, home))
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
