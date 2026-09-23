#include <stddef.h>
#include <stdint.h>

#include "rpl-mutation.h"
#include "rpl-object-store.h"
#include "rpl-stack-view.h"

enum {
    RPL_TEMPTOP = 0x000806eeu,
    RPL_RSKTOP = 0x000806f3u,
    RPL_DSKTOP = 0x000806f8u,
    RPL_EDITLINE = 0x000806fdu,
    RPL_TEMPENV = 0x00080702u,
    RPL_TOUCHTAB = 0x0008070cu,
    RPL_FSTVGERPTR = 0x0008072fu,
    RPL_SYSUPDSTART = 0x000806dau,
    RPL_AVMEM = 0x00080e9bu,
    RPL_DORRP = 0x00002a96u,
    RPL_MAIN_FIRST = 0x00080000u,
    RPL_MAIN_END = 0x00100000u,
    RPL_SYSUPD_SLOTS = 0x18cu,
    RPL_REFERENCE_SLOT_LIMIT = 8192u,
    RPL_TEMPENV_FRAME_LIMIT = 1024u,
    RPL_VGER_GROUP_LIMIT = 256u
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int io_ready(const struct hpclang_rpl_mutation_io *io)
{
    return io != NULL &&
           io->read_nibble != NULL &&
           io->write_nibble != NULL &&
           io->temp_limit > RPL_MAIN_FIRST &&
           io->temp_limit <= RPL_MAIN_END &&
           io->user_limit >= RPL_MAIN_FIRST &&
           io->user_limit < RPL_MAIN_END &&
           io->stack_reserve_nibbles <= RPL_MAIN_END - io->user_limit &&
           (io->stack_reserve_nibbles % 5u) == 0u;
}

static int read_word(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t address,
    uint32_t nibbles,
    uint32_t *value)
{
    if (!io_ready(io) || value == NULL || nibbles == 0u || nibbles > 8u ||
        address > RPL_MAIN_END - nibbles)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!io->read_nibble(io->context, address + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        if (nibble > 0x0fu)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        out |= (uint32_t)nibble << (i * 4u);
    }

    *value = out;
    return HPCLANG_RPL_MUTATION_OK;
}

static int write_word(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t address,
    uint32_t nibbles,
    uint32_t value)
{
    if (!io_ready(io) || nibbles == 0u || nibbles > 8u ||
        address > RPL_MAIN_END - nibbles)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = (uint8_t)((value >> (i * 4u)) & 0x0fu);
        if (!io->write_nibble(io->context, address + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int range_mapped(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t address,
    uint32_t nibbles)
{
    if (!io_ready(io))
        return 0;
    if (nibbles == 0u)
        return 1;
    if (address < RPL_MAIN_FIRST ||
        address >= RPL_MAIN_END ||
        nibbles > RPL_MAIN_END - address)
        return 0;

    uint32_t end = address + nibbles;
    uint32_t current = address;
    while (current < end) {
        uint8_t ignored = 0u;
        if (!io->read_nibble(io->context, current, &ignored))
            return 0;

        uint32_t next = (current & UINT32_C(0xfffff000)) + 0x1000u;
        if (next <= current || next >= end)
            break;
        current = next;
    }

    uint8_t ignored = 0u;
    return io->read_nibble(io->context, end - 1u, &ignored);
}

static int move_nibbles(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t destination,
    uint32_t source,
    uint32_t nibbles)
{
    if (nibbles == 0u || destination == source)
        return HPCLANG_RPL_MUTATION_OK;

    if (!range_mapped(io, source, nibbles) ||
        !range_mapped(io, destination, nibbles))
        return HPCLANG_RPL_MUTATION_IO;

    if (destination > source) {
        uint32_t remaining = nibbles;
        while (remaining != 0u) {
            uint32_t chunk = remaining > 8u ? 8u : remaining;
            uint32_t offset = remaining - chunk;
            uint32_t value = 0u;
            int result = read_word(io, source + offset, chunk, &value);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;
            result = write_word(io, destination + offset, chunk, value);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;
            remaining -= chunk;
        }
        return HPCLANG_RPL_MUTATION_OK;
    }

    uint32_t offset = 0u;
    while (offset < nibbles) {
        uint32_t remaining = nibbles - offset;
        uint32_t chunk = remaining > 8u ? 8u : remaining;
        uint32_t value = 0u;
        int result = read_word(io, source + offset, chunk, &value);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        result = write_word(io, destination + offset, chunk, value);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        offset += chunk;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int snapshot_copy_to_saturn(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t destination,
    const struct hpclang_object *object)
{
    if (object == NULL || object->nibble_count == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    if (!range_mapped(io, destination, object->nibble_count))
        return HPCLANG_RPL_MUTATION_IO;

    for (uint32_t i = 0u; i < object->nibble_count; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, i, &nibble))
            return HPCLANG_RPL_MUTATION_MALFORMED;
        if (!io->write_nibble(io->context, destination + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    return HPCLANG_RPL_MUTATION_OK;
}


static int pointer_hits_range(
    uint32_t value,
    uint32_t start,
    uint32_t end)
{
    return value >= start && value < end;
}

static int scan_pointer_slot(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t slot,
    uint32_t start,
    uint32_t end)
{
    uint32_t value = 0u;
    int result = read_word(io, slot, 5u, &value);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    return pointer_hits_range(value, start, end)
        ? HPCLANG_RPL_MUTATION_BUSY
        : HPCLANG_RPL_MUTATION_OK;
}

static int scan_data_stack_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    uint32_t top = 0u;
    uint32_t editline = 0u;
    int result = read_word(io, RPL_DSKTOP, 5u, &top);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_EDITLINE, 5u, &editline);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (top < RPL_MAIN_FIRST ||
        editline < top ||
        editline > RPL_MAIN_END ||
        (editline - top) % 5u != 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t slots = (editline - top) / 5u;
    if (slots > RPL_REFERENCE_SLOT_LIMIT)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    for (uint32_t i = 0u; i < slots; ++i) {
        result = scan_pointer_slot(
            io, top + i * 5u, start, end);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int scan_return_stack_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    uint32_t rstk = 0u;
    int result = read_word(io, RPL_RSKTOP, 5u, &rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (rstk < RPL_MAIN_FIRST + 5u || rstk > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t slot = rstk - 5u;
    for (uint32_t i = 0u; i < RPL_REFERENCE_SLOT_LIMIT; ++i) {
        uint32_t value = 0u;
        result = read_word(io, slot, 5u, &value);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        if ((value & ~UINT32_C(0x0000000f)) == 0u)
            return HPCLANG_RPL_MUTATION_OK;
        if (pointer_hits_range(value, start, end))
            return HPCLANG_RPL_MUTATION_BUSY;
        if (slot < RPL_MAIN_FIRST + 5u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        slot -= 5u;
    }
    return HPCLANG_RPL_MUTATION_MALFORMED;
}

static int scan_tempenv_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    uint32_t ptr = 0u;
    int result = read_word(io, RPL_TEMPENV, 5u, &ptr);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (ptr == 0u)
        return HPCLANG_RPL_MUTATION_OK;
    if (ptr < RPL_MAIN_FIRST || ptr > RPL_MAIN_END - 10u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t scanned_slots = 0u;
    for (uint32_t frame = 0u;
         frame < RPL_TEMPENV_FRAME_LIMIT;
         ++frame) {
        uint32_t count = 0u;
        result = read_word(io, ptr, 5u, &count);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        if (ptr > RPL_MAIN_END - 10u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        ptr += 10u;

        if (count > 15u) {
            int32_t remaining = (int32_t)count - 15;
            while (remaining > 0) {
                if (scanned_slots > RPL_REFERENCE_SLOT_LIMIT - 2u ||
                    ptr > RPL_MAIN_END - 10u)
                    return HPCLANG_RPL_MUTATION_MALFORMED;

                result = scan_pointer_slot(io, ptr, start, end);
                if (result != HPCLANG_RPL_MUTATION_OK)
                    return result;
                ptr += 5u;
                ++scanned_slots;

                result = scan_pointer_slot(io, ptr, start, end);
                if (result != HPCLANG_RPL_MUTATION_OK)
                    return result;
                ptr += 5u;
                ++scanned_slots;
                remaining -= 10;
            }
        }

        if (count == 0u)
            return HPCLANG_RPL_MUTATION_OK;
    }
    return HPCLANG_RPL_MUTATION_MALFORMED;
}

static int scan_touchtab_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    uint32_t base = 0u;
    int result = read_word(io, RPL_TOUCHTAB, 5u, &base);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (base == 0u)
        return HPCLANG_RPL_MUTATION_OK;
    if (base > RPL_MAIN_END - 3u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t ptr = base + 3u;
    for (uint32_t i = 0u; i < RPL_REFERENCE_SLOT_LIMIT; ++i) {
        uint32_t value = 0u;
        result = read_word(io, ptr, 5u, &value);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        if (value == 0u)
            return HPCLANG_RPL_MUTATION_OK;
        if (pointer_hits_range(value, start, end))
            return HPCLANG_RPL_MUTATION_BUSY;
        if (ptr > RPL_MAIN_END - 10u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        ptr += 5u;
    }
    return HPCLANG_RPL_MUTATION_MALFORMED;
}

static int scan_vger_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    uint32_t base = 0u;
    int result = read_word(io, RPL_FSTVGERPTR, 5u, &base);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (base == 0u)
        return HPCLANG_RPL_MUTATION_OK;
    if (base > RPL_MAIN_END - 20u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t ptr = base + 15u;
    uint32_t groups = 0u;
    result = read_word(io, ptr, 5u, &groups);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (groups > RPL_VGER_GROUP_LIMIT)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t scanned_slots = 0u;
    for (uint32_t group = 0u; group < groups; ++group) {
        if (ptr > RPL_MAIN_END - 25u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        ptr += 20u;

        for (;;) {
            if (scanned_slots++ >= RPL_REFERENCE_SLOT_LIMIT)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            uint32_t value = 0u;
            result = read_word(io, ptr, 5u, &value);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;
            if (value == 0u)
                break;
            if (pointer_hits_range(value, start, end))
                return HPCLANG_RPL_MUTATION_BUSY;
            if (ptr > RPL_MAIN_END - 10u)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            ptr += 5u;
        }
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int scan_system_references(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    for (uint32_t i = 0u; i < RPL_SYSUPD_SLOTS; ++i) {
        int result = scan_pointer_slot(
            io, RPL_SYSUPDSTART + i * 5u, start, end);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int object_reference_state(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t start,
    uint32_t end)
{
    if (start >= end || end > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int result = scan_data_stack_references(io, start, end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_return_stack_references(io, start, end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_tempenv_references(io, start, end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_touchtab_references(io, start, end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_vger_references(io, start, end);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    return scan_system_references(io, start, end);
}

static int store_result_to_mutation(int result)
{
    switch (result) {
    case HPCLANG_RPL_STORE_OK:
        return HPCLANG_RPL_MUTATION_OK;
    case HPCLANG_RPL_STORE_NOT_FOUND:
        return HPCLANG_RPL_MUTATION_NOT_FOUND;
    case HPCLANG_RPL_STORE_NOT_DIRECTORY:
        return HPCLANG_RPL_MUTATION_DIRECTORY;
    case HPCLANG_RPL_STORE_UNAVAILABLE:
        return HPCLANG_RPL_MUTATION_IO;
    case HPCLANG_RPL_STORE_LIMIT:
    case HPCLANG_RPL_STORE_MALFORMED:
    default:
        return HPCLANG_RPL_MUTATION_MALFORMED;
    }
}

static int object_span_result_to_mutation(int result)
{
    switch (result) {
    case HPCLANG_RPL_SPAN_OK:
        return HPCLANG_RPL_MUTATION_OK;
    case HPCLANG_RPL_SPAN_UNAVAILABLE:
        return HPCLANG_RPL_MUTATION_IO;
    case HPCLANG_RPL_SPAN_UNSUPPORTED:
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    case HPCLANG_RPL_SPAN_BOUNDS:
    case HPCLANG_RPL_SPAN_DEPTH:
    case HPCLANG_RPL_SPAN_MALFORMED:
    default:
        return HPCLANG_RPL_MUTATION_MALFORMED;
    }
}

static int snapshot_matches_saturn(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t address,
    const struct hpclang_object *object,
    int *matches)
{
    if (object == NULL || matches == NULL)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    *matches = 0;

    if (!range_mapped(io, address, object->nibble_count))
        return HPCLANG_RPL_MUTATION_IO;

    for (uint32_t i = 0u; i < object->nibble_count; ++i) {
        uint8_t expected = 0u;
        uint8_t actual = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, i, &expected))
            return HPCLANG_RPL_MUTATION_MALFORMED;
        if (!io->read_nibble(io->context, address + i, &actual))
            return HPCLANG_RPL_MUTATION_IO;
        if (actual > 0x0fu)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        if (actual != expected)
            return HPCLANG_RPL_MUTATION_OK;
    }

    *matches = 1;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_mutation_store_replace_same_size(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t directory,
    const uint8_t *name,
    uint32_t name_length,
    const struct hpclang_object *object)
{
    if (!io_ready(io) ||
        object == NULL ||
        object->nibble_count == 0u ||
        (name == NULL && name_length != 0u))
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    const struct hpclang_rpl_reader reader = {
        .read_nibble = io->read_nibble,
        .context = io->context,
    };

    struct hpclang_rpl_directory_entry_view entry;
    int result = hpclang_rpl_directory_find(
        &reader, directory, name, name_length, &entry);
    if (result != HPCLANG_RPL_STORE_OK)
        return store_result_to_mutation(result);
    if (entry.prologue == RPL_DORRP)
        return HPCLANG_RPL_MUTATION_DIRECTORY;

    uint32_t span = 0u;
    result = hpclang_rpl_object_span(
        &reader,
        entry.object_address,
        RPL_MAIN_END - entry.object_address,
        &span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return object_span_result_to_mutation(result);
    if (span != object->nibble_count)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    int identical = 0;
    result = snapshot_matches_saturn(
        io, entry.object_address, object, &identical);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (!identical) {
        result = object_reference_state(
            io, entry.object_address, entry.object_address + span);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }

    return snapshot_copy_to_saturn(io, entry.object_address, object);
}

static int stack_depth(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t *depth)
{
    const struct hpclang_rpl_reader reader = {
        .read_nibble = io->read_nibble,
        .context = io->context,
    };

    int result = hpclang_rpl_stack_depth(&reader, depth);
    switch (result) {
    case HPCLANG_RPL_STACK_OK:
        return HPCLANG_RPL_MUTATION_OK;
    case HPCLANG_RPL_STACK_UNAVAILABLE:
        return HPCLANG_RPL_MUTATION_IO;
    default:
        return HPCLANG_RPL_MUTATION_MALFORMED;
    }
}

struct temp_plan {
    uint32_t ttop;
    uint32_t rstk;
    uint32_t dstk;
    uint32_t avmem;
    uint32_t delta;
    uint32_t temp_cost;
    uint32_t new_rstk;
    uint32_t new_ttop;
    uint32_t object_address;
};

static int plan_temp(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t object_nibbles,
    struct temp_plan *plan)
{
    if (plan == NULL || object_nibbles == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t ttop = 0u;
    uint32_t rstk = 0u;
    uint32_t dstk = 0u;
    uint32_t avmem = 0u;

    int result = read_word(io, RPL_TEMPTOP, 5u, &ttop);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_RSKTOP, 5u, &rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_DSKTOP, 5u, &dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_AVMEM, 5u, &avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (ttop < RPL_MAIN_FIRST ||
        rstk < ttop ||
        dstk <= rstk ||
        dstk > RPL_MAIN_END ||
        rstk > io->temp_limit)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    /*
     * Keep the HPGCC 2/3 five-nibble guard after the six-nibble TEMPOB block
     * overhead. temp_limit is already tightened around hpclang native pages.
     */
    if (io->temp_limit - rstk <= 11u ||
        object_nibbles > io->temp_limit - rstk - 11u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t delta = 0u;
    if (!checked_add(object_nibbles, 6u, &delta))
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t new_rstk = 0u;
    uint32_t new_ttop = 0u;
    if (!checked_add(rstk, delta, &new_rstk) ||
        !checked_add(ttop, delta, &new_ttop) ||
        new_rstk > io->temp_limit ||
        new_rstk > dstk)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    /*
     * Preserve any launcher/native reservation already subtracted from AVMEM.
     * Charge only the change in geometrically usable five-nibble cells.
     */
    uint32_t old_cells = (dstk - rstk) / 5u;
    uint32_t new_cells = (dstk - new_rstk) / 5u;
    if (new_cells > old_cells)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t temp_cost = old_cells - new_cells;
    if (avmem < temp_cost)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t source_size = rstk - ttop;
    if (!range_mapped(io, ttop, source_size) ||
        !range_mapped(io, new_ttop, source_size) ||
        !range_mapped(io, ttop, delta))
        return HPCLANG_RPL_MUTATION_IO;

    plan->ttop = ttop;
    plan->rstk = rstk;
    plan->dstk = dstk;
    plan->avmem = avmem;
    plan->delta = delta;
    plan->temp_cost = temp_cost;
    plan->new_rstk = new_rstk;
    plan->new_ttop = new_ttop;
    plan->object_address = ttop + 1u;
    return HPCLANG_RPL_MUTATION_OK;
}

static int commit_temp(
    const struct hpclang_rpl_mutation_io *io,
    const struct temp_plan *plan)
{
    if (plan == NULL)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t source_size = plan->rstk - plan->ttop;
    int result = move_nibbles(
        io, plan->new_ttop, plan->ttop, source_size);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t object_nibbles = plan->delta - 6u;
    uint32_t link = plan->ttop + 1u + object_nibbles;
    result = write_word(io, link, 5u, plan->delta);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = write_word(io, RPL_RSKTOP, 5u, plan->new_rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = write_word(io, RPL_TEMPTOP, 5u, plan->new_ttop);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(
        io, RPL_AVMEM, 5u, plan->avmem - plan->temp_cost);
}

int hpclang_rpl_mutation_push_snapshot(
    const struct hpclang_rpl_mutation_io *io,
    const struct hpclang_object *object)
{
    if (!io_ready(io) || object == NULL || object->nibble_count == 0u)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    struct temp_plan plan;
    int result = plan_temp(io, object->nibble_count, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (plan.dstk < 5u)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    uint32_t slot = plan.dstk - 5u;

    uint32_t stack_floor = 0u;
    if (!checked_add(
            io->user_limit,
            io->stack_reserve_nibbles,
            &stack_floor) ||
        slot < stack_floor ||
        plan.temp_cost == UINT32_MAX ||
        plan.avmem < plan.temp_cost + 1u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    result = commit_temp(io, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = snapshot_copy_to_saturn(io, plan.object_address, object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = write_word(io, slot, 5u, plan.object_address);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = write_word(io, RPL_DSKTOP, 5u, slot);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(
        io, RPL_AVMEM, 5u,
        plan.avmem - plan.temp_cost - 1u);
}

int hpclang_rpl_mutation_drop(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t count)
{
    if (!io_ready(io))
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (count == 0u)
        return HPCLANG_RPL_MUTATION_OK;

    uint32_t depth = 0u;
    int result = stack_depth(io, &depth);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (count > depth)
        return HPCLANG_RPL_MUTATION_LEVEL;

    uint32_t dstk = 0u;
    uint32_t avmem = 0u;
    result = read_word(io, RPL_DSKTOP, 5u, &dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_AVMEM, 5u, &avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (count > UINT32_MAX / 5u ||
        avmem > UINT32_MAX - count)
        return HPCLANG_RPL_MUTATION_LEVEL;
    uint32_t delta = count * 5u;
    uint32_t new_dstk = 0u;
    if (!checked_add(dstk, delta, &new_dstk) ||
        new_dstk > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    /*
     * Match Stock/HPGCC stack semantics: AVMEM changes by stack cells, not by
     * recomputing DSKTOP-RSKTOP. The latter would incorrectly count a hidden
     * launcher-bias cell and hpclang-owned native pages as ordinary free RAM.
     */
    result = write_word(io, RPL_DSKTOP, 5u, new_dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(io, RPL_AVMEM, 5u, avmem + count);
}

int hpclang_rpl_mutation_replace(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t level,
    const struct hpclang_object *object)
{
    if (!io_ready(io) || object == NULL || object->nibble_count == 0u)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (level == 0u)
        return HPCLANG_RPL_MUTATION_LEVEL;

    uint32_t depth = 0u;
    int result = stack_depth(io, &depth);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (level > depth)
        return HPCLANG_RPL_MUTATION_LEVEL;

    uint32_t dstk = 0u;
    result = read_word(io, RPL_DSKTOP, 5u, &dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t zero_based = level - 1u;
    if (zero_based > UINT32_MAX / 5u)
        return HPCLANG_RPL_MUTATION_LEVEL;
    uint32_t offset = zero_based * 5u;
    uint32_t slot = 0u;
    if (!checked_add(dstk, offset, &slot) ||
        slot > RPL_MAIN_END - 5u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    struct temp_plan plan;
    result = plan_temp(io, object->nibble_count, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = commit_temp(io, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = snapshot_copy_to_saturn(io, plan.object_address, object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(io, slot, 5u, plan.object_address);
}
