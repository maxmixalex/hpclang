#include <stddef.h>
#include <stdint.h>

#include "rpl-mutation.h"
#include "rpl-stack-view.h"

enum {
    RPL_TEMPTOP = 0x000806eeu,
    RPL_RSKTOP = 0x000806f3u,
    RPL_DSKTOP = 0x000806f8u,
    RPL_AVMEM = 0x00080e9bu,
    RPL_MAIN_FIRST = 0x00080000u,
    RPL_MAIN_END = 0x00100000u
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
           io->user_limit < RPL_MAIN_END;
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

static int temp_capacity(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t object_nibbles,
    uint32_t *ttop,
    uint32_t *rstk,
    uint32_t *dstk)
{
    uint32_t local_ttop = 0u;
    uint32_t local_rstk = 0u;
    uint32_t local_dstk = 0u;

    int result = read_word(io, RPL_TEMPTOP, 5u, &local_ttop);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_RSKTOP, 5u, &local_rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_DSKTOP, 5u, &local_dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (local_ttop < RPL_MAIN_FIRST ||
        local_rstk < local_ttop ||
        local_dstk <= local_rstk ||
        local_dstk > RPL_MAIN_END ||
        local_rstk >= io->temp_limit ||
        local_dstk <= io->user_limit)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    /*
     * HPGCC 2/3 reserve eleven nibbles between RSKTOP and the first native
     * page: six for the TEMPOB block link plus one five-nibble stack cell.
     */
    if (io->temp_limit - local_rstk <= 11u ||
        object_nibbles > io->temp_limit - local_rstk - 11u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    if (local_dstk - io->user_limit < 5u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    if (ttop != NULL)
        *ttop = local_ttop;
    if (rstk != NULL)
        *rstk = local_rstk;
    if (dstk != NULL)
        *dstk = local_dstk;
    return HPCLANG_RPL_MUTATION_OK;
}

static int allocate_temp(
    const struct hpclang_rpl_mutation_io *io,
    uint32_t object_nibbles,
    uint32_t *object_address,
    uint32_t *new_rstktop)
{
    if (object_address == NULL || new_rstktop == NULL ||
        object_nibbles == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t ttop = 0u;
    uint32_t rstk = 0u;
    uint32_t dstk = 0u;
    int result = temp_capacity(
        io, object_nibbles, &ttop, &rstk, &dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t delta = 0u;
    if (!checked_add(object_nibbles, 6u, &delta) ||
        delta > io->temp_limit - rstk)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t moved_end = 0u;
    if (!checked_add(rstk, delta, &moved_end) ||
        moved_end > io->temp_limit)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t source_size = rstk - ttop;
    if (!range_mapped(io, ttop, source_size) ||
        !range_mapped(io, ttop + delta, source_size) ||
        !range_mapped(io, ttop + 1u, object_nibbles))
        return HPCLANG_RPL_MUTATION_IO;

    result = move_nibbles(io, ttop + delta, ttop, source_size);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t link = ttop + 1u + object_nibbles;
    result = write_word(io, link, 5u, delta);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t new_rstk = rstk + delta;
    uint32_t new_ttop = ttop + delta;
    result = write_word(io, RPL_RSKTOP, 5u, new_rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = write_word(io, RPL_TEMPTOP, 5u, new_ttop);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t avmem = (dstk - new_rstk) / 5u;
    result = write_word(io, RPL_AVMEM, 5u, avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    *object_address = ttop + 1u;
    *new_rstktop = new_rstk;
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_mutation_push_snapshot(
    const struct hpclang_rpl_mutation_io *io,
    const struct hpclang_object *object)
{
    if (!io_ready(io) || object == NULL || object->nibble_count == 0u)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    /*
     * Check both allocator and data-stack capacity before TEMPOB changes, so a
     * predictable NOSPACE result never leaves an orphan allocation.
     */
    uint32_t ttop = 0u;
    uint32_t rstk = 0u;
    uint32_t dstk = 0u;
    int result = temp_capacity(
        io, object->nibble_count, &ttop, &rstk, &dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    (void)ttop;
    (void)rstk;

    if (dstk < io->user_limit + 5u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t object_address = 0u;
    uint32_t new_rstk = 0u;
    result = allocate_temp(
        io, object->nibble_count, &object_address, &new_rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = snapshot_copy_to_saturn(io, object_address, object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t slot = dstk - 5u;
    result = write_word(io, slot, 5u, object_address);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = write_word(io, RPL_DSKTOP, 5u, slot);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(
        io, RPL_AVMEM, 5u, (slot - new_rstk) / 5u);
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

    uint32_t object_address = 0u;
    uint32_t new_rstk = 0u;
    result = allocate_temp(
        io, object->nibble_count, &object_address, &new_rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    (void)new_rstk;

    result = snapshot_copy_to_saturn(io, object_address, object);
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

    return write_word(io, slot, 5u, object_address);
}
