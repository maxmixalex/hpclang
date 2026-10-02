#include <stddef.h>
#include <stdint.h>

#include "rpl-mutation.h"
#include "rpl-object-store.h"
#include "rpl-stack-view.h"

enum
{
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
    RPL_VGER_GROUP_LIMIT = 256u,
    RPL_DIRECTORY_DEPTH_LIMIT = 32u,
    RPL_DIRECTORY_ENTRY_LIMIT = 4096u,
    RPL_DIRECTORY_PATCH_LIMIT = 96u
};

static int checked_add(uint32_t a, uint32_t b, uint32_t *out)
{
    if (out == NULL || a > UINT32_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int checked_add_signed(uint32_t value, int32_t offset, uint32_t *out)
{
    if (out == NULL)
        return 0;

    int64_t shifted = (int64_t)value + (int64_t)offset;
    if (shifted < 0 || shifted > UINT32_MAX)
        return 0;

    *out = (uint32_t)shifted;
    return 1;
}

static int io_ready(const struct hpclang_rpl_mutation_io *io)
{
    return io != NULL && io->read_nibble != NULL && io->write_nibble != NULL &&
           io->temp_limit > RPL_MAIN_FIRST && io->temp_limit <= RPL_MAIN_END &&
           io->user_limit >= RPL_MAIN_FIRST && io->user_limit < RPL_MAIN_END &&
           io->stack_reserve_nibbles <= RPL_MAIN_END - io->user_limit &&
           (io->stack_reserve_nibbles % 5u) == 0u &&
           (io->native_root_count == 0u || io->native_roots != NULL);
}

/* Preserve native reservations when either end of the free interval moves. */
static int plan_free_cells(uint32_t avmem, uint32_t old_cells, uint32_t new_cells,
                           uint32_t *updated)
{
    if (avmem > old_cells)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t reserved = old_cells - avmem;
    if (new_cells < reserved)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    *updated = new_cells - reserved;
    return HPCLANG_RPL_MUTATION_OK;
}

static int read_word(const struct hpclang_rpl_mutation_io *io, uint32_t address, uint32_t nibbles,
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

static int write_word(const struct hpclang_rpl_mutation_io *io, uint32_t address, uint32_t nibbles,
                      uint32_t value)
{
    if (!io_ready(io) || nibbles == 0u || nibbles > 8u || address > RPL_MAIN_END - nibbles)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = (uint8_t)((value >> (i * 4u)) & 0x0fu);
        if (!io->write_nibble(io->context, address + i, nibble))
            return HPCLANG_RPL_MUTATION_IO;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int range_mapped(const struct hpclang_rpl_mutation_io *io, uint32_t address,
                        uint32_t nibbles)
{
    if (!io_ready(io))
        return 0;
    if (nibbles == 0u)
        return 1;
    if (address < RPL_MAIN_FIRST || address >= RPL_MAIN_END || nibbles > RPL_MAIN_END - address)
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

static int move_nibbles(const struct hpclang_rpl_mutation_io *io, uint32_t destination,
                        uint32_t source, uint32_t nibbles)
{
    if (nibbles == 0u || destination == source)
        return HPCLANG_RPL_MUTATION_OK;

    if (!range_mapped(io, source, nibbles) || !range_mapped(io, destination, nibbles))
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

static int snapshot_copy_to_saturn(const struct hpclang_rpl_mutation_io *io, uint32_t destination,
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

static int pointer_hits_range(uint32_t value, uint32_t start, uint32_t end)
{
    return value >= start && value < end;
}

enum pointer_scan_mode
{
    POINTER_SCAN_REJECT = 0,
    POINTER_SCAN_VALIDATE_SHIFT = 1,
    POINTER_SCAN_APPLY_SHIFT = 2
};

struct pointer_scan_request
{
    uint32_t move_start;
    uint32_t move_end;
    uint32_t remove_start;
    uint32_t remove_end;
    int32_t offset;
    enum pointer_scan_mode mode;
    int *busy_found; /* validation accumulates BUSY without hiding later errors */
};

static int inspect_pointer_value(const struct hpclang_rpl_mutation_io *io, uint32_t slot,
                                 uint32_t value, const struct pointer_scan_request *request,
                                 uint32_t *native_slot)
{
    if (request == NULL)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    if (request->mode == POINTER_SCAN_REJECT) {
        if (!pointer_hits_range(value, request->move_start, request->move_end))
            return HPCLANG_RPL_MUTATION_OK;
        if (request->busy_found != NULL) {
            *request->busy_found = 1;
            return HPCLANG_RPL_MUTATION_OK;
        }
        return HPCLANG_RPL_MUTATION_BUSY;
    }

    if (request->remove_start < request->remove_end &&
        pointer_hits_range(value, request->remove_start, request->remove_end)) {
        if (request->busy_found != NULL) {
            *request->busy_found = 1;
            return HPCLANG_RPL_MUTATION_OK;
        }
        return HPCLANG_RPL_MUTATION_BUSY;
    }

    if (!pointer_hits_range(value, request->move_start, request->move_end))
        return HPCLANG_RPL_MUTATION_OK;

    uint32_t shifted = 0u;
    if (!checked_add_signed(value, request->offset, &shifted) || shifted < RPL_MAIN_FIRST ||
        shifted >= RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    if (request->mode == POINTER_SCAN_VALIDATE_SHIFT)
        return HPCLANG_RPL_MUTATION_OK;
    if (request->mode != POINTER_SCAN_APPLY_SHIFT)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    if (native_slot != NULL) {
        *native_slot = shifted;
        return HPCLANG_RPL_MUTATION_OK;
    }
    return write_word(io, slot, 5u, shifted);
}

static int scan_pointer_slot(const struct hpclang_rpl_mutation_io *io, uint32_t slot,
                             const struct pointer_scan_request *request)
{
    uint32_t value = 0u;
    int result = read_word(io, slot, 5u, &value);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    return inspect_pointer_value(io, slot, value, request, NULL);
}

static int scan_data_stack_references(const struct hpclang_rpl_mutation_io *io,
                                      const struct pointer_scan_request *request)
{
    uint32_t top = 0u;
    uint32_t editline = 0u;
    int result = read_word(io, RPL_DSKTOP, 5u, &top);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_EDITLINE, 5u, &editline);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (top < RPL_MAIN_FIRST || editline < top || editline > RPL_MAIN_END ||
        (editline - top) % 5u != 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t slots = (editline - top) / 5u;
    if (slots > RPL_REFERENCE_SLOT_LIMIT)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    for (uint32_t i = 0u; i < slots; ++i) {
        result = scan_pointer_slot(io, top + i * 5u, request);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int scan_return_stack_references(const struct hpclang_rpl_mutation_io *io,
                                        const struct pointer_scan_request *request)
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

        result = inspect_pointer_value(io, slot, value, request, NULL);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;

        if (slot < RPL_MAIN_FIRST + 5u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        slot -= 5u;
    }
    return HPCLANG_RPL_MUTATION_MALFORMED;
}

static int scan_tempenv_references(const struct hpclang_rpl_mutation_io *io,
                                   const struct pointer_scan_request *request)
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
    for (uint32_t frame = 0u; frame < RPL_TEMPENV_FRAME_LIMIT; ++frame) {
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
                if (scanned_slots > RPL_REFERENCE_SLOT_LIMIT - 2u || ptr > RPL_MAIN_END - 10u)
                    return HPCLANG_RPL_MUTATION_MALFORMED;

                result = scan_pointer_slot(io, ptr, request);
                if (result != HPCLANG_RPL_MUTATION_OK)
                    return result;
                ptr += 5u;
                ++scanned_slots;

                result = scan_pointer_slot(io, ptr, request);
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

static int scan_touchtab_references(const struct hpclang_rpl_mutation_io *io,
                                    const struct pointer_scan_request *request)
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

        result = inspect_pointer_value(io, ptr, value, request, NULL);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;

        if (ptr > RPL_MAIN_END - 10u)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        ptr += 5u;
    }
    return HPCLANG_RPL_MUTATION_MALFORMED;
}

static int scan_vger_references(const struct hpclang_rpl_mutation_io *io,
                                const struct pointer_scan_request *request)
{
    uint32_t base = 0u;
    int result = read_word(io, RPL_FSTVGERPTR, 5u, &base);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (base == 0u)
        return HPCLANG_RPL_MUTATION_OK;
    if (base > RPL_MAIN_END - 20u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    /*
     * RPL_FSTVGERPTR itself is part of the system-variable table below and
     * must be adjusted exactly once. Traverse the table through its old base
     * here so nested pointer cells can be validated/rewritten before USEROB
     * movement.
     */
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

            result = inspect_pointer_value(io, ptr, value, request, NULL);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;

            if (ptr > RPL_MAIN_END - 10u)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            ptr += 5u;
        }
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int scan_system_references(const struct hpclang_rpl_mutation_io *io,
                                  const struct pointer_scan_request *request)
{
    for (uint32_t i = 0u; i < RPL_SYSUPD_SLOTS; ++i) {
        int result = scan_pointer_slot(io, RPL_SYSUPDSTART + i * 5u, request);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

static int scan_all_references(const struct hpclang_rpl_mutation_io *io,
                               const struct pointer_scan_request *request)
{
    for (uint32_t i = 0u; i < io->native_root_count; ++i) {
        uint32_t *root = io->native_roots[i];
        if (root == NULL)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        int result = inspect_pointer_value(io, 0u, *root, request, root);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }
    int result = scan_data_stack_references(io, request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_return_stack_references(io, request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_tempenv_references(io, request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_touchtab_references(io, request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = scan_vger_references(io, request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    return scan_system_references(io, request);
}

static int object_reference_state(const struct hpclang_rpl_mutation_io *io, uint32_t start,
                                  uint32_t end)
{
    if (start >= end || end > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int busy = 0;
    const struct pointer_scan_request request = {
        .move_start = start,
        .move_end = end,
        .remove_start = 0u,
        .remove_end = 0u,
        .offset = 0,
        .mode = POINTER_SCAN_REJECT,
        .busy_found = &busy,
    };
    int result = scan_all_references(io, &request);
    return result == HPCLANG_RPL_MUTATION_OK && busy ? HPCLANG_RPL_MUTATION_BUSY : result;
}

struct userob_shift_geometry;

static int preserve_userob_references_to_temp(const struct hpclang_rpl_mutation_io *io,
                                              uint32_t start, uint32_t end,
                                              int32_t future_user_delta,
                                              struct userob_shift_geometry *future_geometry);

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

struct userob_shift_geometry
{
    uint32_t old_dstk;
    uint32_t new_dstk;
    uint32_t move_end;
    uint32_t remove_start;
    uint32_t remove_end;
    uint32_t new_avmem;
    uint32_t home;
    uint32_t home_end;
    int32_t pointer_offset;
};

struct directory_patch
{
    uint32_t slot;
    uint32_t value;
};

struct directory_patch_plan
{
    struct directory_patch patches[RPL_DIRECTORY_PATCH_LIMIT];
    uint32_t count;
    uint32_t remaining_entries;
};

struct directory_delete_context
{
    uint32_t directory;
    uint32_t item;
    uint32_t found;
};

static int map_structural_address(const struct userob_shift_geometry *geometry, uint32_t address,
                                  uint32_t width, uint32_t *mapped)
{
    if (geometry == NULL || mapped == NULL || width == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t end = 0u;
    if (!checked_add(address, width, &end) || end > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    if (geometry->remove_start < geometry->remove_end && address < geometry->remove_end &&
        end > geometry->remove_start)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    if (address < geometry->move_end && end > geometry->move_end)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    if (address >= geometry->old_dstk && end <= geometry->move_end) {
        if (!checked_add_signed(address, geometry->pointer_offset, mapped) ||
            *mapped < RPL_MAIN_FIRST || *mapped > RPL_MAIN_END - width)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        return HPCLANG_RPL_MUTATION_OK;
    }

    *mapped = address;
    return HPCLANG_RPL_MUTATION_OK;
}

static int add_directory_patch(struct directory_patch_plan *plan, uint32_t slot, uint32_t value)
{
    if (plan == NULL || value > UINT32_C(0x000fffff))
        return HPCLANG_RPL_MUTATION_MALFORMED;

    for (uint32_t i = 0u; i < plan->count; ++i) {
        if (plan->patches[i].slot != slot)
            continue;
        return plan->patches[i].value == value ? HPCLANG_RPL_MUTATION_OK
                                               : HPCLANG_RPL_MUTATION_MALFORMED;
    }

    if (plan->count >= RPL_DIRECTORY_PATCH_LIMIT)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    plan->patches[plan->count].slot = slot;
    plan->patches[plan->count].value = value;
    ++plan->count;
    return HPCLANG_RPL_MUTATION_OK;
}

static int plan_directory_relation(const struct hpclang_rpl_mutation_io *io,
                                   const struct userob_shift_geometry *geometry,
                                   struct directory_patch_plan *plan, uint32_t slot,
                                   uint32_t target, int target_above_slot)
{
    uint32_t current = 0u;
    int result = read_word(io, slot, 5u, &current);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t expected = 0u;
    if (target_above_slot) {
        if (target <= slot)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        expected = target - slot;
    } else {
        if (slot <= target)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        expected = slot - target;
    }
    if (current != expected)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t mapped_slot = 0u;
    uint32_t mapped_target = 0u;
    result = map_structural_address(geometry, slot, 5u, &mapped_slot);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = map_structural_address(geometry, target, 1u, &mapped_target);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t updated = 0u;
    if (target_above_slot) {
        if (mapped_target <= mapped_slot)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        updated = mapped_target - mapped_slot;
    } else {
        if (mapped_slot <= mapped_target)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        updated = mapped_slot - mapped_target;
    }

    if (updated == current)
        return HPCLANG_RPL_MUTATION_OK;
    return add_directory_patch(plan, slot, updated);
}

static int plan_directory_rewire(const struct hpclang_rpl_mutation_io *io,
                                 const struct userob_shift_geometry *geometry,
                                 struct directory_patch_plan *plan, uint32_t slot,
                                 uint32_t old_target, uint32_t new_target)
{
    uint32_t current = 0u;
    int result = read_word(io, slot, 5u, &current);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t expected = 0u;
    if (old_target > slot)
        expected = old_target - slot;
    else if (slot > old_target)
        expected = slot - old_target;
    else
        return HPCLANG_RPL_MUTATION_MALFORMED;
    if (current != expected)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t mapped_slot = 0u;
    result = map_structural_address(geometry, slot, 5u, &mapped_slot);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t updated = 0u;
    if (new_target != 0u) {
        uint32_t mapped_target = 0u;
        result = map_structural_address(geometry, new_target, 1u, &mapped_target);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        if (mapped_target > mapped_slot)
            updated = mapped_target - mapped_slot;
        else if (mapped_slot > mapped_target)
            updated = mapped_slot - mapped_target;
        else
            return HPCLANG_RPL_MUTATION_MALFORMED;
    }

    if (updated == current)
        return HPCLANG_RPL_MUTATION_OK;
    return add_directory_patch(plan, slot, updated);
}

static int collect_directory_patches(const struct hpclang_rpl_mutation_io *io,
                                     const struct hpclang_rpl_reader *reader,
                                     const struct userob_shift_geometry *geometry,
                                     struct directory_patch_plan *plan, uint32_t directory,
                                     uint32_t enclosing_end, uint32_t depth,
                                     struct directory_delete_context *delete_context)
{
    if (depth > RPL_DIRECTORY_DEPTH_LIMIT)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    uint32_t span = 0u;
    int result = hpclang_rpl_object_span(reader, directory, RPL_MAIN_END - directory, &span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return object_span_result_to_mutation(result);

    uint32_t directory_end = 0u;
    if (span == 0u || !checked_add(directory, span, &directory_end) ||
        directory_end > enclosing_end)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    struct hpclang_rpl_directory_cursor cursor;
    result = hpclang_rpl_directory_cursor_open(reader, directory, &cursor);
    if (result != HPCLANG_RPL_STORE_OK)
        return store_result_to_mutation(result);
    if (cursor.finished)
        return HPCLANG_RPL_MUTATION_OK;
    if (cursor.item <= cursor.chain || cursor.item >= directory_end)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t incoming_slot = cursor.chain;
    int incoming_planned = 0;

    while (!cursor.finished) {
        if (plan->remaining_entries == 0u)
            return HPCLANG_RPL_MUTATION_UNSUPPORTED;
        --plan->remaining_entries;

        uint32_t item = cursor.item;
        if (item <= cursor.chain || item >= directory_end)
            return HPCLANG_RPL_MUTATION_MALFORMED;

        struct hpclang_rpl_directory_entry_view entry;
        result = hpclang_rpl_directory_cursor_next(reader, &cursor, &entry);
        if (result != 1)
            return result < 0 ? store_result_to_mutation(result) : HPCLANG_RPL_MUTATION_MALFORMED;

        if (entry.object_address < item || entry.object_address >= directory_end)
            return HPCLANG_RPL_MUTATION_MALFORMED;

        int deleting = delete_context != NULL && directory == delete_context->directory &&
                       item == delete_context->item;

        if (deleting) {
            if (delete_context->found != 0u || entry.prologue == RPL_DORRP)
                return HPCLANG_RPL_MUTATION_MALFORMED;

            uint32_t next = cursor.finished ? 0u : cursor.item;
            result = plan_directory_rewire(io, geometry, plan, incoming_slot, item, next);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;

            delete_context->found = 1u;
            incoming_planned = 1;
        } else {
            if (!incoming_planned) {
                result = plan_directory_relation(io, geometry, plan, incoming_slot, item,
                                                 item > incoming_slot);
                if (result != HPCLANG_RPL_MUTATION_OK)
                    return result;
            } else {
                incoming_planned = 0;
            }

            if (entry.prologue == RPL_DORRP) {
                result = collect_directory_patches(io, reader, geometry, plan, entry.object_address,
                                                   directory_end, depth + 1u, delete_context);
                if (result != HPCLANG_RPL_MUTATION_OK)
                    return result;
            }
        }

        if (!cursor.finished && !deleting) {
            if (item < 5u || cursor.item <= cursor.chain || cursor.item >= item)
                return HPCLANG_RPL_MUTATION_MALFORMED;
            incoming_slot = item - 5u;
        }
    }

    if (delete_context != NULL && directory == delete_context->directory &&
        delete_context->found == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    return HPCLANG_RPL_MUTATION_OK;
}

static int plan_userob_shift(const struct hpclang_rpl_mutation_io *io, uint32_t position,
                             int32_t delta, struct userob_shift_geometry *geometry,
                             struct directory_patch_plan *directory_plan,
                             struct directory_delete_context *delete_context)
{
    if (!io_ready(io) || geometry == NULL || directory_plan == NULL)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    /* No-op requests are handled by callers; success here must yield a plan. */
    if (delta == 0)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t old_dstk = 0u;
    uint32_t rstk = 0u;
    uint32_t avmem = 0u;
    int result = read_word(io, RPL_DSKTOP, 5u, &old_dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_RSKTOP, 5u, &rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = read_word(io, RPL_AVMEM, 5u, &avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (rstk < RPL_MAIN_FIRST || old_dstk <= rstk || old_dstk >= RPL_MAIN_END ||
        position <= old_dstk || position > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    const struct hpclang_rpl_reader reader = {
        .read_nibble = io->read_nibble,
        .context = io->context,
    };
    uint32_t home = 0u;
    result = hpclang_rpl_store_home_object(&reader, &home);
    if (result != HPCLANG_RPL_STORE_OK)
        return store_result_to_mutation(result);

    uint32_t home_span = 0u;
    result = hpclang_rpl_object_span(&reader, home, RPL_MAIN_END - home, &home_span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return object_span_result_to_mutation(result);

    uint32_t home_end = 0u;
    if (home_span == 0u || !checked_add(home, home_span, &home_end) || position > home_end)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int64_t new_dstk_wide = (int64_t)old_dstk - (int64_t)delta;
    if (new_dstk_wide < RPL_MAIN_FIRST || new_dstk_wide >= RPL_MAIN_END)
        return delta > 0 ? HPCLANG_RPL_MUTATION_NOSPACE : HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t new_dstk = (uint32_t)new_dstk_wide;

    uint32_t move_end = position;
    uint32_t remove_start = 0u;
    uint32_t remove_end = 0u;

    if (delta > 0) {
        uint32_t stack_floor = 0u;
        if (!checked_add(io->user_limit, io->stack_reserve_nibbles, &stack_floor) ||
            new_dstk < stack_floor)
            return HPCLANG_RPL_MUTATION_NOSPACE;
    } else {
        uint32_t remove = (uint32_t)(-(int64_t)delta);
        if (remove > position - old_dstk)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        remove_start = position - remove;
        remove_end = position;
        move_end = remove_start;
        if (remove_start < home)
            return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    }

    if (new_dstk <= rstk)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    if (move_end < old_dstk)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t old_cells = (old_dstk - rstk) / 5u;
    uint32_t new_cells = (new_dstk - rstk) / 5u;
    uint32_t new_avmem;
    result = plan_free_cells(avmem, old_cells, new_cells, &new_avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t move_nibbles = move_end - old_dstk;
    if (!range_mapped(io, old_dstk, move_nibbles) || !range_mapped(io, new_dstk, move_nibbles))
        return HPCLANG_RPL_MUTATION_IO;

    int64_t pointer_offset_wide = (int64_t)new_dstk - (int64_t)old_dstk;
    if (pointer_offset_wide < INT32_MIN || pointer_offset_wide > INT32_MAX)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    *geometry = (struct userob_shift_geometry){
        .old_dstk = old_dstk,
        .new_dstk = new_dstk,
        .move_end = move_end,
        .remove_start = remove_start,
        .remove_end = remove_end,
        .new_avmem = new_avmem,
        .home = home,
        .home_end = home_end,
        .pointer_offset = (int32_t)pointer_offset_wide,
    };

    /*
     * Prove directory geometry before reference state. A BUSY result can then
     * be treated by higher-level replace/remove as the only remaining blocker,
     * preserved through TEMPOB, then committed with this validated plan.
     */
    directory_plan->count = 0u;
    directory_plan->remaining_entries = RPL_DIRECTORY_ENTRY_LIMIT;
    if (delete_context != NULL)
        delete_context->found = 0u;
    result = collect_directory_patches(io, &reader, geometry, directory_plan, home, RPL_MAIN_END,
                                       0u, delete_context);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    int busy = 0;
    const struct pointer_scan_request pointer_request = {
        .move_start = old_dstk,
        .move_end = move_end,
        .remove_start = remove_start,
        .remove_end = remove_end,
        .offset = geometry->pointer_offset,
        .mode = POINTER_SCAN_VALIDATE_SHIFT,
        .busy_found = &busy,
    };
    result = scan_all_references(io, &pointer_request);
    return result == HPCLANG_RPL_MUTATION_OK && busy ? HPCLANG_RPL_MUTATION_BUSY : result;
}

static int commit_userob_shift(const struct hpclang_rpl_mutation_io *io,
                               const struct userob_shift_geometry *geometry,
                               const struct directory_patch_plan *directory_plan)
{
    if (geometry == NULL || directory_plan == NULL)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int result = HPCLANG_RPL_MUTATION_OK;
    for (uint32_t i = 0u; i < directory_plan->count; ++i) {
        result =
            write_word(io, directory_plan->patches[i].slot, 5u, directory_plan->patches[i].value);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }

    const struct pointer_scan_request pointer_request = {
        .move_start = geometry->old_dstk,
        .move_end = geometry->move_end,
        .remove_start = geometry->remove_start,
        .remove_end = geometry->remove_end,
        .offset = geometry->pointer_offset,
        .mode = POINTER_SCAN_APPLY_SHIFT,
    };
    result = scan_all_references(io, &pointer_request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t move_count = geometry->move_end - geometry->old_dstk;
    result = move_nibbles(io, geometry->new_dstk, geometry->old_dstk, move_count);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = write_word(io, RPL_DSKTOP, 5u, geometry->new_dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    return write_word(io, RPL_AVMEM, 5u, geometry->new_avmem);
}

int hpclang_rpl_mutation_userob_shift_before(const struct hpclang_rpl_mutation_io *io,
                                             uint32_t position, int32_t delta)
{
    if (delta == 0)
        return io_ready(io) ? HPCLANG_RPL_MUTATION_OK : HPCLANG_RPL_MUTATION_UNAVAILABLE;

    struct userob_shift_geometry geometry;
    struct directory_patch_plan directory_plan;
    int result = plan_userob_shift(io, position, delta, &geometry, &directory_plan, NULL);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    /*
     * All structural/logical checks are complete before the first write.
     * Relative links are patched in their old locations so moved link cells
     * carry the corrected value with them.
     */
    return commit_userob_shift(io, &geometry, &directory_plan);
}

static int snapshot_matches_saturn(const struct hpclang_rpl_mutation_io *io, uint32_t address,
                                   const struct hpclang_object *object, int *matches)
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

static int snapshot_preflight(const struct hpclang_object *object)
{
    if (object == NULL || object->nibble_count == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    for (uint32_t i = 0u; i < object->nibble_count; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_object_snapshot_read_nibble(object, i, &nibble) || nibble > 0x0fu)
            return HPCLANG_RPL_MUTATION_MALFORMED;
    }
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_mutation_store_create(const struct hpclang_rpl_mutation_io *io, uint32_t directory,
                                      const uint8_t *name, uint32_t name_length,
                                      const struct hpclang_object *object)
{
    if (!io_ready(io) || object == NULL || object->nibble_count == 0u ||
        (name == NULL && name_length != 0u) || name_length >= HPCLANG_RPL_STORE_NAME_CAPACITY)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    int result = snapshot_preflight(object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    const struct hpclang_rpl_reader reader = {
        .read_nibble = io->read_nibble,
        .context = io->context,
    };

    struct hpclang_rpl_directory_entry_view existing;
    result = hpclang_rpl_directory_find(&reader, directory, name, name_length, &existing);
    if (result == HPCLANG_RPL_STORE_OK)
        return HPCLANG_RPL_MUTATION_EXISTS;
    if (result != HPCLANG_RPL_STORE_NOT_FOUND)
        return store_result_to_mutation(result);

    struct hpclang_rpl_directory_cursor cursor;
    result = hpclang_rpl_directory_cursor_open(&reader, directory, &cursor);
    if (result != HPCLANG_RPL_STORE_OK)
        return store_result_to_mutation(result);

    uint32_t directory_span = 0u;
    result = hpclang_rpl_object_span(&reader, directory, RPL_MAIN_END - directory, &directory_span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return object_span_result_to_mutation(result);

    uint32_t directory_end = 0u;
    if (directory_span == 0u || !checked_add(directory, directory_span, &directory_end))
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t item_nibbles = 2u;
    if (name_length != 0u) {
        if (name_length > (UINT32_MAX - 4u) / 2u)
            return HPCLANG_RPL_MUTATION_UNSUPPORTED;
        item_nibbles = 4u + name_length * 2u;
    }

    uint32_t entry_nibbles = 0u;
    if (!checked_add(5u, item_nibbles, &entry_nibbles) ||
        !checked_add(entry_nibbles, object->nibble_count, &entry_nibbles) ||
        entry_nibbles > INT32_MAX || entry_nibbles > directory_end - RPL_MAIN_FIRST)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t link = directory_end - entry_nibbles;
    uint32_t item = link + 5u;
    uint32_t object_address = item + item_nibbles;
    if (object_address > directory_end || object->nibble_count != directory_end - object_address ||
        cursor.chain < entry_nibbles)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t new_chain = cursor.chain - entry_nibbles;
    uint32_t chain_offset = item - new_chain;
    if (chain_offset > UINT32_C(0x000fffff))
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    uint32_t link_offset = 0u;
    if (!cursor.finished) {
        if (cursor.item < entry_nibbles)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        uint32_t previous_item = cursor.item - entry_nibbles;
        if (previous_item >= link)
            return HPCLANG_RPL_MUTATION_MALFORMED;
        link_offset = link - previous_item;
        if (link_offset > UINT32_C(0x000fffff))
            return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    }

    if (!range_mapped(io, link, entry_nibbles))
        return HPCLANG_RPL_MUTATION_IO;

    result = hpclang_rpl_mutation_userob_shift_before(io, directory_end, (int32_t)entry_nibbles);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    /*
     * Everything below remains unreachable until the final chain write.
     * Unexpected I/O can therefore leave unused bytes in the opened gap, but
     * not a half-visible directory entry.
     */
    result = write_word(io, link, 5u, link_offset);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    result = write_word(io, item, 2u, name_length);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t cursor_address = item + 2u;
    if (name_length != 0u) {
        for (uint32_t i = 0u; i < name_length; ++i) {
            result = write_word(io, cursor_address, 2u, name[i]);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;
            cursor_address += 2u;
        }
        result = write_word(io, cursor_address, 2u, name_length);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        cursor_address += 2u;
    }
    if (cursor_address != object_address)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    result = snapshot_copy_to_saturn(io, object_address, object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(io, new_chain, 5u, chain_offset);
}

static int store_replace_target(const struct hpclang_rpl_mutation_io *io, uint32_t directory,
                                const uint8_t *name, uint32_t name_length,
                                struct hpclang_rpl_directory_entry_view *entry, uint32_t *span)
{
    if (!io_ready(io) || entry == NULL || span == NULL || (name == NULL && name_length != 0u))
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    const struct hpclang_rpl_reader reader = {
        .read_nibble = io->read_nibble,
        .context = io->context,
    };

    int result = hpclang_rpl_directory_find(&reader, directory, name, name_length, entry);
    if (result != HPCLANG_RPL_STORE_OK)
        return store_result_to_mutation(result);
    if (entry->prologue == RPL_DORRP)
        return HPCLANG_RPL_MUTATION_DIRECTORY;

    result = hpclang_rpl_object_span(&reader, entry->object_address,
                                     RPL_MAIN_END - entry->object_address, span);
    return result == HPCLANG_RPL_SPAN_OK ? HPCLANG_RPL_MUTATION_OK
                                         : object_span_result_to_mutation(result);
}

int hpclang_rpl_mutation_store_replace(const struct hpclang_rpl_mutation_io *io, uint32_t directory,
                                       const uint8_t *name, uint32_t name_length,
                                       const struct hpclang_object *object)
{
    if (!io_ready(io) || object == NULL || object->nibble_count == 0u ||
        (name == NULL && name_length != 0u))
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    int result = snapshot_preflight(object);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    struct hpclang_rpl_directory_entry_view entry;
    uint32_t span = 0u;
    result = store_replace_target(io, directory, name, name_length, &entry, &span);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    if (span == object->nibble_count) {
        int identical = 0;
        result = snapshot_matches_saturn(io, entry.object_address, object, &identical);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;

        if (!identical) {
            result = preserve_userob_references_to_temp(io, entry.object_address,
                                                        entry.object_address + span, 0, NULL);
            if (result != HPCLANG_RPL_MUTATION_OK)
                return result;
        }
        return snapshot_copy_to_saturn(io, entry.object_address, object);
    }

    uint32_t old_end = 0u;
    if (!checked_add(entry.object_address, span, &old_end))
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int reference_state = object_reference_state(io, entry.object_address, old_end);
    if (reference_state != HPCLANG_RPL_MUTATION_OK && reference_state != HPCLANG_RPL_MUTATION_BUSY)
        return reference_state;

    int64_t delta_wide = (int64_t)object->nibble_count - (int64_t)span;
    if (delta_wide < INT32_MIN || delta_wide > INT32_MAX)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    int32_t delta = (int32_t)delta_wide;

    int64_t new_address_wide = (int64_t)entry.object_address - delta_wide;
    if (new_address_wide < RPL_MAIN_FIRST || new_address_wide >= RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_NOSPACE;
    uint32_t new_address = (uint32_t)new_address_wide;

    /*
     * Preflight the complete post-shift destination while the tree is still
     * untouched. The Saturn mapping is static for this native invocation, so
     * a successful check removes ordinary bounds/mapping failures from the
     * post-geometry copy.
     */
    if (!range_mapped(io, new_address, object->nibble_count))
        return HPCLANG_RPL_MUTATION_IO;

    struct userob_shift_geometry shift_geometry;
    struct directory_patch_plan shift_directory_plan;
    result = plan_userob_shift(io, old_end, delta, &shift_geometry, &shift_directory_plan, NULL);
    if (result != HPCLANG_RPL_MUTATION_OK &&
        !(result == HPCLANG_RPL_MUTATION_BUSY && reference_state == HPCLANG_RPL_MUTATION_BUSY))
        return result;

    if (reference_state == HPCLANG_RPL_MUTATION_BUSY) {
        result = preserve_userob_references_to_temp(io, entry.object_address, old_end, delta,
                                                    &shift_geometry);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }

    result = commit_userob_shift(io, &shift_geometry, &shift_directory_plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return snapshot_copy_to_saturn(io, new_address, object);
}

int hpclang_rpl_mutation_store_remove(const struct hpclang_rpl_mutation_io *io, uint32_t directory,
                                      const uint8_t *name, uint32_t name_length)
{
    if (!io_ready(io) || (name == NULL && name_length != 0u) ||
        name_length >= HPCLANG_RPL_STORE_NAME_CAPACITY)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;

    struct hpclang_rpl_directory_entry_view entry;
    uint32_t span = 0u;
    int result = store_replace_target(io, directory, name, name_length, &entry, &span);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t item_nibbles = name_length == 0u ? 2u : 4u + name_length * 2u;
    if (entry.object_address < item_nibbles + 5u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t item = entry.object_address - item_nibbles;
    uint32_t link = item - 5u;
    uint32_t object_end = 0u;
    if (!checked_add(entry.object_address, span, &object_end) || object_end <= link)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t remove_nibbles = object_end - link;
    if (remove_nibbles > INT32_MAX)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    /*
     * Structural link/name bytes have no value-preservation semantics. Reject
     * any Stock pointer into them. References to the object itself are
     * preserved through TEMPOB exactly like destructive replacement.
     */
    result = object_reference_state(io, link, entry.object_address);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    int reference_state = object_reference_state(io, entry.object_address, object_end);
    if (reference_state != HPCLANG_RPL_MUTATION_OK && reference_state != HPCLANG_RPL_MUTATION_BUSY)
        return reference_state;

    struct directory_delete_context delete_context = {
        .directory = directory,
        .item = item,
        .found = 0u,
    };
    struct userob_shift_geometry geometry;
    struct directory_patch_plan directory_plan;
    int32_t delta = -(int32_t)remove_nibbles;

    result = plan_userob_shift(io, object_end, delta, &geometry, &directory_plan, &delete_context);
    if (result != HPCLANG_RPL_MUTATION_OK &&
        !(result == HPCLANG_RPL_MUTATION_BUSY && reference_state == HPCLANG_RPL_MUTATION_BUSY))
        return result;
    if (delete_context.found == 0u)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    if (reference_state == HPCLANG_RPL_MUTATION_BUSY) {
        result = preserve_userob_references_to_temp(io, entry.object_address, object_end, delta,
                                                    &geometry);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
    }

    return commit_userob_shift(io, &geometry, &directory_plan);
}

static int stack_depth(const struct hpclang_rpl_mutation_io *io, uint32_t *depth)
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

struct temp_plan
{
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

static int plan_temp(const struct hpclang_rpl_mutation_io *io, uint32_t object_nibbles,
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

    if (ttop < RPL_MAIN_FIRST || rstk < ttop || dstk <= rstk || dstk > RPL_MAIN_END ||
        rstk > io->temp_limit)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    /*
     * Keep the HPGCC 2/3 five-nibble guard after the six-nibble TEMPOB block
     * overhead. temp_limit is already tightened around hpclang native pages.
     */
    if (io->temp_limit - rstk <= 11u || object_nibbles > io->temp_limit - rstk - 11u)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t delta = 0u;
    if (!checked_add(object_nibbles, 6u, &delta))
        return HPCLANG_RPL_MUTATION_NOSPACE;

    uint32_t new_rstk = 0u;
    uint32_t new_ttop = 0u;
    if (!checked_add(rstk, delta, &new_rstk) || !checked_add(ttop, delta, &new_ttop) ||
        new_rstk > io->temp_limit || new_rstk > dstk)
        return HPCLANG_RPL_MUTATION_NOSPACE;

    /*
     * Preserve any launcher/native reservation already subtracted from AVMEM.
     * Charge only the change in geometrically usable five-nibble cells.
     */
    uint32_t old_cells = (dstk - rstk) / 5u;
    uint32_t new_cells = (dstk - new_rstk) / 5u;
    if (new_cells > old_cells)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t new_avmem = 0u;
    result = plan_free_cells(avmem, old_cells, new_cells, &new_avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    uint32_t source_size = rstk - ttop;
    if (!range_mapped(io, ttop, source_size) || !range_mapped(io, new_ttop, source_size) ||
        !range_mapped(io, ttop, delta))
        return HPCLANG_RPL_MUTATION_IO;

    /*
     * Existing TEMPOB objects and the live return-stack cells occupy the
     * [TEMPTOP, RSKTOP) interval that will move upward. Validate every
     * Stock-visible pointer before committing the move; unlike historical
     * sat3_createtemp(), hpclang must not leave an existing TEMP reference
     * pointing at the old address after allocation.
     */
    if (delta > INT32_MAX)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    const struct pointer_scan_request pointer_request = {
        .move_start = ttop,
        .move_end = rstk,
        .remove_start = 0u,
        .remove_end = 0u,
        .offset = (int32_t)delta,
        .mode = POINTER_SCAN_VALIDATE_SHIFT,
    };
    result = scan_all_references(io, &pointer_request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    plan->ttop = ttop;
    plan->rstk = rstk;
    plan->dstk = dstk;
    plan->avmem = avmem;
    plan->delta = delta;
    plan->temp_cost = avmem - new_avmem;
    plan->new_rstk = new_rstk;
    plan->new_ttop = new_ttop;
    plan->object_address = ttop + 1u;
    return HPCLANG_RPL_MUTATION_OK;
}

static int commit_temp(const struct hpclang_rpl_mutation_io *io, const struct temp_plan *plan)
{
    if (plan == NULL)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t source_size = plan->rstk - plan->ttop;
    if (plan->delta > INT32_MAX)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    /*
     * Rewrite pointers in their old cells before moving the interval. Pointer
     * cells that themselves live in TEMPOB/return-stack memory then carry the
     * corrected value to their new address with the memmove.
     */
    const struct pointer_scan_request pointer_request = {
        .move_start = plan->ttop,
        .move_end = plan->rstk,
        .remove_start = 0u,
        .remove_end = 0u,
        .offset = (int32_t)plan->delta,
        .mode = POINTER_SCAN_APPLY_SHIFT,
    };
    int result = scan_all_references(io, &pointer_request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = move_nibbles(io, plan->new_ttop, plan->ttop, source_size);
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

    return write_word(io, RPL_AVMEM, 5u, plan->avmem - plan->temp_cost);
}

static int preserve_userob_references_to_temp(const struct hpclang_rpl_mutation_io *io,
                                              uint32_t start, uint32_t end,
                                              int32_t future_user_delta,
                                              struct userob_shift_geometry *future_geometry)
{
    if (!io_ready(io) || start >= end || end > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    int result = object_reference_state(io, start, end);
    if (result == HPCLANG_RPL_MUTATION_OK)
        return HPCLANG_RPL_MUTATION_OK;
    if (result != HPCLANG_RPL_MUTATION_BUSY)
        return result;

    uint32_t span = end - start;
    struct temp_plan plan;
    result = plan_temp(io, span, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    /*
     * TEMP growth raises RSKTOP while USEROB growth lowers DSKTOP. Prove the
     * combined final geometry now so an ordinary ENOSPC cannot appear only
     * after references have already been redirected to the TEMP copy.
     */
    if (future_user_delta != 0) {
        int64_t final_dstk_wide = (int64_t)plan.dstk - (int64_t)future_user_delta;
        if (final_dstk_wide < RPL_MAIN_FIRST || final_dstk_wide >= RPL_MAIN_END)
            return future_user_delta > 0 ? HPCLANG_RPL_MUTATION_NOSPACE
                                         : HPCLANG_RPL_MUTATION_MALFORMED;
        uint32_t final_dstk = (uint32_t)final_dstk_wide;

        uint32_t stack_floor = 0u;
        if (!checked_add(io->user_limit, io->stack_reserve_nibbles, &stack_floor) ||
            (future_user_delta > 0 && final_dstk < stack_floor) || final_dstk <= plan.new_rstk)
            return HPCLANG_RPL_MUTATION_NOSPACE;

        uint32_t old_cells = (plan.dstk - plan.rstk) / 5u;
        uint32_t final_cells = (final_dstk - plan.new_rstk) / 5u;
        uint32_t final_avmem = 0u;
        result = plan_free_cells(plan.avmem, old_cells, final_cells, &final_avmem);
        if (result != HPCLANG_RPL_MUTATION_OK)
            return result;
        /* TEMP allocation changes the accounting, not USEROB positions or
           directory links. Finish the already-validated USEROB plan now;
           never discover a new planning error after the first write. */
        if (future_geometry != NULL)
            future_geometry->new_avmem = final_avmem;
    }

    int64_t offset_wide = (int64_t)plan.object_address - (int64_t)start;
    if (offset_wide < INT32_MIN || offset_wide > INT32_MAX)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;

    struct pointer_scan_request pointer_request = {
        .move_start = start,
        .move_end = end,
        .remove_start = 0u,
        .remove_end = 0u,
        .offset = (int32_t)offset_wide,
        .mode = POINTER_SCAN_VALIDATE_SHIFT,
    };
    result = scan_all_references(io, &pointer_request);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = commit_temp(io, &plan);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    result = move_nibbles(io, plan.object_address, start, span);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    pointer_request.mode = POINTER_SCAN_APPLY_SHIFT;
    return scan_all_references(io, &pointer_request);
}

int hpclang_rpl_mutation_push_snapshot(const struct hpclang_rpl_mutation_io *io,
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
    if (!checked_add(io->user_limit, io->stack_reserve_nibbles, &stack_floor) ||
        slot < stack_floor || plan.temp_cost == UINT32_MAX || plan.avmem < plan.temp_cost + 1u)
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

    return write_word(io, RPL_AVMEM, 5u, plan.avmem - plan.temp_cost - 1u);
}

int hpclang_rpl_mutation_drop(const struct hpclang_rpl_mutation_io *io, uint32_t count)
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

    if (count > UINT32_MAX / 5u)
        return HPCLANG_RPL_MUTATION_LEVEL;
    uint32_t delta = count * 5u;
    uint32_t new_dstk = 0u;
    if (!checked_add(dstk, delta, &new_dstk) || new_dstk > RPL_MAIN_END)
        return HPCLANG_RPL_MUTATION_MALFORMED;

    uint32_t rstk = 0u;
    result = read_word(io, RPL_RSKTOP, 5u, &rstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;
    if (rstk < RPL_MAIN_FIRST || rstk >= dstk)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    uint32_t new_avmem = 0u;
    result = plan_free_cells(avmem, (dstk - rstk) / 5u, (new_dstk - rstk) / 5u, &new_avmem);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    /* Release count cells without exposing native reservations as free RAM. */
    result = write_word(io, RPL_DSKTOP, 5u, new_dstk);
    if (result != HPCLANG_RPL_MUTATION_OK)
        return result;

    return write_word(io, RPL_AVMEM, 5u, new_avmem);
}

int hpclang_rpl_mutation_replace(const struct hpclang_rpl_mutation_io *io, uint32_t level,
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
    if (!checked_add(dstk, offset, &slot) || slot > RPL_MAIN_END - 5u)
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
