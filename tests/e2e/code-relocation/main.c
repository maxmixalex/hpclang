
#include <stdint.h>
#include "rpl-stock-memory.h"
#include "rpl-object-store.h"
#include "rpl-code.h"
volatile uint32_t hpclang_e2e_result;
static int word(struct hpclang_rpl_mutation_io *io, uint32_t at, unsigned width, uint32_t *out)
{
    *out = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t n;
        if (!io->read_nibble(io->context, at + i, &n) || n > 15)
            return 0;
        *out |= (uint32_t)n << (4 * i);
    }
    return 1;
}
static int bind_word(struct hpclang_rpl_mutation_io *io, uint32_t at, uint32_t value)
{
    for (unsigned n = 0; n < 5; ++n)
        if (!io->write_nibble(io->context, at + n, (value >> (4 * n)) & 15))
            return 0;
    uint32_t actual;
    return word(io, at, 5, &actual) && actual == value;
}
static int mapped_read(void *ctx, uint32_t mapping, uint32_t at, uint8_t *out)
{
    struct hpclang_rpl_mutation_io *io = ctx;
    return mapping == 1 && io->read_nibble(io->context, at, out);
}
static int mapped_write(void *ctx, uint32_t mapping, uint32_t at, uint8_t value)
{
    struct hpclang_rpl_mutation_io *io = ctx;
    return mapping == 1 && io->write_nibble(io->context, at, value);
}
#include "cases.h"
enum
{
    CASE_COUNT = sizeof cases / sizeof cases[0]
};
struct boundary_check
{
    uint32_t target, length, found;
};
static int check_boundary(void *context, uint32_t instruction, uint32_t length)
{
    struct boundary_check *check = context;
    if (instruction == check->target) {
        if (length != check->length)
            return HPCLANG_RPL_SPAN_MALFORMED;
        ++check->found;
    }
    return 0;
}
static int run(void)
{
    struct hpclang_rpl_stock_memory memory;
    struct hpclang_rpl_mutation_io io;
    if (!hpclang_rpl_stock_mutation_io(&io, &memory))
        return 1;
    struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    uint32_t home;
    if (!word(&io, 0x80711, 5, &home))
        return 2;
    const uint8_t name[] = "BRLIVE";
    struct hpclang_rpl_directory_entry_view entry;
    int found = hpclang_rpl_directory_find(&reader, home, name, 6, &entry);
    if (found == HPCLANG_RPL_STORE_NOT_FOUND)
        return 0; /* Initial standard Stock launch. */
    if (found)
        return 3;
    if (entry.prologue != 0x2d9d)
        return 4;
    uint32_t cursor = entry.object_address + 5, span, prologue;
    if (hpclang_rpl_object_span(&reader, cursor, 0x100000 - cursor, &span))
        return 5;
    cursor += span; /* Embedded ARM launcher string. */
    if (!word(&io, cursor, 5, &prologue) || prologue != 0x2e92)
        return 6;
    cursor += 11; /* library 275 command 0 */
    struct hpclang_rpl_reference_edit edits[CASE_COUNT];
    struct hpclang_rpl_reference_edit_builder builder;
    struct hpclang_rpl_reference_guard guards[CASE_COUNT * 3];
    uint32_t addresses[CASE_COUNT];
    unsigned count = 0;
    if (hpclang_rpl_reference_edits_begin(&builder, edits, CASE_COUNT))
        return 7;
    for (unsigned i = 0; i < CASE_COUNT; ++i) {
        if (!word(&io, cursor, 5, &prologue) || prologue != 0x2a4e ||
            hpclang_rpl_object_span(&reader, cursor, 0x100000 - cursor, &span) || span != 15)
            return 8;
        cursor += span;
        if (!word(&io, cursor, 5, &prologue) || prologue != 0x2dcc ||
            hpclang_rpl_object_span(&reader, cursor, 0x100000 - cursor, &span) ||
            span != cases[i].body + 10)
            return 9;
        uint32_t body = cursor + 10, at = body + cases[i].at;
        /* The generator establishes this instruction-only prefix. Walk from
         * its entry instead of trusting the proposed operand site's boundary.
         * Padding and trailing pointer data are outside this interval. */
        struct boundary_check boundary = {at, cases[i].length, 0};
        if (hpclang_rpl_code_visit(&reader, body, cases[i].at + cases[i].length, check_boundary,
                                   &boundary) ||
            boundary.found != 1)
            return 19;
        addresses[i] = at;
        if (cases[i].pointer) {
            /* Bind the fixture's absolute address after Stock allocation. */
            uint32_t target = body + cases[i].old;
            if (!bind_word(&io, at + 2, target))
                return 16;
        }
        if (cases[i].indirect) {
            uint32_t cell = body + cases[i].cell, target = body + cases[i].old;
            int via_memory = cases[i].indirect >= 3;
            if (!bind_word(&io, body + cases[i].load, cell) ||
                !bind_word(&io, cell, via_memory ? cell + 5 : target) ||
                (via_memory && !bind_word(&io, cell + 5, target)))
                return 17;
        }
        for (unsigned n = 0; n < cases[i].length;) {
            unsigned width = cases[i].length - n;
            if (width > 5)
                width = 5;
            uint32_t value;
            if (!word(&io, at + n, width, &value))
                return 10;
            guards[count++] = (struct hpclang_rpl_reference_guard){1, at + n, value, width};
            n += width;
        }
        struct hpclang_rpl_reference_mappings maps = {1, 1, 1};
        struct hpclang_rpl_code_patch patch;
        int (*plan)(const struct hpclang_rpl_reader *, uint32_t, uint32_t,
                    const struct hpclang_rpl_reference_mappings *, uint32_t, uint32_t, int32_t,
                    struct hpclang_rpl_code_patch *) = cases[i].pointer
                                                           ? hpclang_rpl_code_pointer_plan_shift
                                                           : hpclang_rpl_code_branch_plan_shift;
        int planned;
        if (cases[i].indirect) {
            uint32_t source = body + cases[i].cell, value;
            unsigned register_c = !(cases[i].indirect & 1);
            struct hpclang_rpl_code_indirect transfer;
            if (!word(&io, source, 5, &value) ||
                hpclang_rpl_code_indirect_inspect(&reader, &reader, at, cases[i].length,
                                                  register_c ? 0 : value, register_c ? value : 0,
                                                  &transfer) ||
                transfer.register_c != register_c ||
                transfer.via_memory != (cases[i].indirect >= 3) ||
                transfer.target != body + cases[i].old)
                return 18;
            guards[count++] = (struct hpclang_rpl_reference_guard){1, source, value, 5};
            uint32_t field = transfer.via_memory ? transfer.register_value : source;
            if (transfer.via_memory)
                guards[count++] =
                    (struct hpclang_rpl_reference_guard){1, field, transfer.target, 5};
            struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_EXECUTION, field,
                                                 transfer.target, 0, 0};
            struct hpclang_rpl_reference_patch resolved;
            planned = hpclang_rpl_reference_plan_execution_shift(
                &edge, &maps, body + cases[i].old, body + cases[i].old + 16, 16, &resolved);
            if (planned == 1)
                patch = (struct hpclang_rpl_code_patch){resolved.field, 5, transfer.target,
                                                        resolved.value};
        } else {
            planned = plan(&reader, at, cases[i].length, &maps, body + cases[i].old,
                           body + cases[i].old + (cases[i].move ? cases[i].length : 16),
                           cases[i].move ? 32 : 16, &patch);
        }
        if (planned != 1)
            return 11;
        if (cases[i].repair) {
            struct hpclang_rpl_reference_edit edit = {1, patch.field, patch.expected, patch.value,
                                                      patch.width};
            if (hpclang_rpl_reference_edits_add(&builder, &edit))
                return 12;
        }
        cursor += span;
    }
    struct hpclang_rpl_reference_writer writer = {{mapped_read, &io}, mapped_write};
    if (hpclang_rpl_reference_guards_validate(&writer.reader, guards, count) ||
        hpclang_rpl_reference_edits_validate(&writer.reader, edits, builder.count))
        return 13;
    if (hpclang_rpl_reference_edits_replay(&writer, edits, builder.count))
        return 14;
    for (unsigned i = 0; i < CASE_COUNT; ++i) {
        if (!cases[i].move && !cases[i].pointer)
            continue;
        unsigned length = cases[i].pointer ? 5 : cases[i].length;
        uint32_t start = addresses[i] + (cases[i].pointer ? cases[i].old - cases[i].at : 0);
        for (unsigned n = 0; n < length; ++n) {
            uint32_t value, copied;
            uint32_t source = start + n, destination = source + (cases[i].pointer ? 16 : 32);
            uint8_t poison = cases[i].pointer ? (0xfa173u >> (4 * n)) & 15 : 0;
            if (!word(&io, source, 1, &value) ||
                !io.write_nibble(io.context, destination, (uint8_t)value) ||
                !word(&io, destination, 1, &copied) || copied != value ||
                !io.write_nibble(io.context, source, poison) || !word(&io, source, 1, &copied) ||
                copied != poison)
                return 15;
        }
    }
    return 0;
}
int main(void)
{
    int status = run();
    hpclang_e2e_result = status ? (0x46410000u | status) : 0x50415353u;
    if (status)
        for (;;)
            __asm__ volatile("nop");
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
    return 0;
}
