
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
    struct hpclang_rpl_reference_guard guards[CASE_COUNT * 2];
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
        if (hpclang_rpl_code_branch_plan_shift(&reader, at, cases[i].length, &maps,
                                               body + cases[i].old, body + cases[i].old + 16, 16,
                                               &patch) != 1)
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
