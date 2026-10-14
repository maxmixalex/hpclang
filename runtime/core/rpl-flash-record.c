#include "rpl-flash-record.h"
#include "rpl-port.h"
#include <stdlib.h>
#include <string.h>

int hpclang_rpl_flash_discard_plan(const struct hpclang_rpl_reader *reader, uint32_t first,
                                   uint32_t end, struct hpclang_rpl_flash_discard *plan)
{
    if (!reader || !reader->read_nibble || !plan || ((first | end) & 1u))
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_port_cursor cursor = {first, end, 0};
    struct hpclang_rpl_port_entry entry;
    int result;
    while ((result = hpclang_rpl_flash_next(reader, &cursor, &entry)) > 0) {
    }
    if (result)
        return result;
    if (cursor.address & 1u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t available = end - cursor.address, last = 0, mask = 0;
    for (uint32_t offset = 0; offset < available; ++offset) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, cursor.address + offset, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15u)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (nibble != 15u)
            last = offset + 1u;
        if (offset >= 2u && offset < 7u)
            mask |= (uint32_t)nibble << ((offset - 2u) * 4u);
    }
    if (!last)
        return 0;
    if (available < 18u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    /* Native deleted slots require extent >= 11. Preserve the erased end
     * byte and round the slot, including status, to a whole halfword. */
    uint32_t extent = (((last < 13u ? 13u : last) + 3u) & ~3u) - 2u;
    for (; extent <= available - 4u; extent += 4u) {
        if ((extent & mask) == extent) {
            *plan = (struct hpclang_rpl_flash_discard){cursor.address, extent};
            return 1;
        }
    }
    return HPCLANG_RPL_SPAN_MALFORMED;
}

int hpclang_rpl_flash_tail(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t end,
                           struct hpclang_rpl_flash_space *space)
{
    if (!reader || !reader->read_nibble || !space || ((first | end) & 1u))
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_port_cursor cursor = {first, end, 0};
    struct hpclang_rpl_port_entry entry;
    int result;
    while ((result = hpclang_rpl_flash_next(reader, &cursor, &entry)) > 0) {
    }
    if (result)
        return result;
    if (cursor.address & 1u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    for (uint32_t at = cursor.address; at < end; ++at) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble != 15u)
            return HPCLANG_RPL_SPAN_MALFORMED;
    }
    *space = (struct hpclang_rpl_flash_space){cursor.address, (end - cursor.address - 2u) & ~3u};
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_flash_backup_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t span, uint8_t **record, uint32_t *bytes)
{
    if (!record || !bytes)
        return HPCLANG_RPL_SPAN_MALFORMED;
    /* A native bank has 0x40000 nibbles, a 0x20A-nibble prefix and an
     * erased end marker. Never allocate a record that cannot fit one bank. */
    if (span > 0x40000u - 0x20au - 2u - 8u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_backup_view view;
    int result = hpclang_rpl_backup_view(reader, address, span, &view);
    if (result)
        return result;
    if (view.span != span)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t nibbles = (span + 8u + 3u) & ~3u;
    uint32_t count = nibbles / 2u;
    uint8_t *data = malloc(count);
    if (!data)
        return HPCLANG_RPL_BACKUP_NOMEM;
    memset(data, 0xff, count);
    uint32_t extent = nibbles - 2u;
    data[0] = 0x53;
    data[1] = (uint8_t)extent;
    data[2] = (uint8_t)(extent >> 8);
    data[3] = (uint8_t)(0x10u | (extent >> 16));
    for (uint32_t i = 0; i < span; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, address + i, &nibble)) {
            free(data);
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        }
        if (nibble > 15u) {
            free(data);
            return HPCLANG_RPL_SPAN_MALFORMED;
        }
        uint32_t at = i + 8u;
        uint32_t shift = (at & 1u) * 4u;
        data[at / 2u] = (uint8_t)((data[at / 2u] & ~(15u << shift)) | (nibble << shift));
    }
    *record = data;
    *bytes = count;
    return HPCLANG_RPL_SPAN_OK;
}
