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
static int prepare_call(struct hpclang_rpl_mutation_io *, uint32_t **);

/* Controlled Stock fixture only: the clean test catalog and fixture programs
 * supply the input provenance. Opaque execution coverage stays unresolved;
 * this is not a public registration or recovery implementation. */
struct live_probe
{
    struct hpclang_rpl_mutation_io *io;
    uint32_t first, end, roots[2048], root_count;
    struct hpclang_rpl_reference_edit_builder builder;
    struct hpclang_rpl_reference_edit edits[4096];
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
        hpclang_rpl_reference_plan_qualified_shift(edge, &maps, p->first, p->end, 16, &patch);
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
static int probe_node(struct live_probe *p, struct hpclang_rpl_reader *reader, uint32_t at,
                      uint32_t span)
{
    struct hpclang_rpl_reference_node node = {at, span, 0};
    return hpclang_rpl_reference_visit_nested(reader, &node, p->work, 512, 10000, probe_edge, p);
}
static int probe_register(void)
{
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory))
        return 1;
    uint32_t *roots[10];
    if (prepare_call(&io, roots))
        return 61;
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
    p->io = &io;
    p->first = insertion.slot;
    p->end = end;
    hpclang_rpl_reference_edits_begin(&p->builder, p->edits, 4096);
    int result = 6;
    if (hpclang_rpl_mutation_visit_root_sources(&io, probe_root, p))
        goto done;
    result = 7;
    if (probe_node(p, &reader, home, 0x100000 - home))
        goto done;
    uint32_t at = top;
    while (at > bottom + 5) {
        uint32_t size;
        if (probe_read_word(&io, at - 5, 5, &size) || size < 11 || size > at - bottom - 5)
            goto done;
        uint32_t object = at - size + 1, stop = at - 5;
        while (object < stop) {
            uint32_t span;
            if (hpclang_rpl_object_span(&reader, object, stop - object, &span) || !span ||
                probe_node(p, &reader, object, span))
                goto done;
            object += span;
        }
        at -= size;
    }
    const struct hpclang_rpl_reference_reader mapped = {probe_mapped_read, &io};
    result = 8;
    if (hpclang_rpl_reference_edits_validate(&mapped, p->edits, p->builder.count))
        goto done;
    const struct hpclang_rpl_catalog_repairs repairs = {1,    p->edits, p->builder.count, NULL, 0,
                                                        NULL, 0};
    /* After this boundary an error must halt the test, never return Stock. */
    const struct hpclang_rpl_library_binding added = {binding, 0, 0};
    if (hpclang_rpl_catalog_insert_repaired(&io, 123, &added, &insertion, &repairs)) {
        hpclang_e2e_result = 0x46410ffe;
        for (;;)
            __asm__ volatile("nop");
    }
    result = 0;
done:
    free(p);
    return result;
}

static int stage_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    return !probe_read_word(context, at, width, value);
}

static int prepare_call(struct hpclang_rpl_mutation_io *io, uint32_t **roots)
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
