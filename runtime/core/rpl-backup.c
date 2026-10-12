#include "rpl-backup.h"
#include <stddef.h>
#include <stdlib.h>

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t count,
                     uint32_t *value)
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, address + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result |= (uint32_t)nibble << (4 * i);
    }
    *value = result;
    return HPCLANG_RPL_SPAN_OK;
}

static void write_word(uint8_t *packed, uint32_t offset, uint32_t value, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t at = offset + i;
        uint32_t shift = (at & 1u) * 4u;
        packed[at / 2u] =
            (uint8_t)((packed[at / 2u] & ~(15u << shift)) | (((value >> (4u * i)) & 15u) << shift));
    }
}

int hpclang_rpl_backup_encode(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t value_span, const uint8_t *name, uint32_t name_length,
                              uint8_t **packed, uint32_t *span)
{
    if (!reader || !reader->read_nibble || !packed || !span || (!name && name_length) ||
        name_length > 255u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t overhead = 24u + 2u * name_length;
    if (address >= 0x100000u || value_span > 0x100000u - address ||
        value_span > 0xfffffu - overhead)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t actual;
    int result = hpclang_rpl_object_span(reader, address, value_span, &actual);
    if (result)
        return result;
    if (actual != value_span)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t total = overhead + value_span;
    uint8_t *data = calloc((total + 1u) / 2u, 1);
    if (!data)
        return HPCLANG_RPL_BACKUP_NOMEM;
    write_word(data, 0, 0x02b62u, 5);
    write_word(data, 5, total - 5u, 5);
    write_word(data, 10, name_length, 2);
    for (uint32_t i = 0; i < name_length; ++i)
        write_word(data, 12u + 2u * i, name[i], 2);
    write_word(data, 12u + 2u * name_length, name_length, 2);
    uint32_t value_at = 14u + 2u * name_length;
    for (uint32_t i = 0; i < value_span; ++i) {
        uint32_t nibble;
        result = read_word(reader, address + i, 1, &nibble);
        if (result) {
            free(data);
            return result;
        }
        write_word(data, value_at + i, nibble, 1);
    }
    write_word(data, total - 10u, 0x02911u, 5);
    uint32_t crc = 0;
    for (uint32_t i = 5; i < total - 4u; ++i) {
        uint32_t nibble = (data[i / 2u] >> ((i & 1u) * 4u)) & 15u;
        crc = (crc >> 4u) ^ (((crc ^ nibble) & 15u) * 0x1081u);
    }
    write_word(data, total - 4u, crc, 4);
    *packed = data;
    *span = total;
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_backup_view(const struct hpclang_rpl_reader *reader, uint32_t address,
                            uint32_t limit, struct hpclang_rpl_backup_view *view)
{
    if (reader == NULL || reader->read_nibble == NULL || view == NULL)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (address >= 0x100000u || limit < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (limit > 0x100000u - address)
        limit = 0x100000u - address;
    if (limit < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;

    uint32_t prologue;
    int result = read_word(reader, address, 5, &prologue);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (prologue != 0x02b62u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;

    struct hpclang_rpl_backup_view next = {0};
    result = hpclang_rpl_object_span(reader, address, limit, &next.span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    /* Prologue/length, two name lengths, at least one object word, checksum BINT.
     */
    if (next.span < 29u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = read_word(reader, address + 10u, 2, &next.name_length);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (next.name_length * 2u > next.span - 29u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    next.name_address = address + 12u;
    uint32_t repeated_length;
    result = read_word(reader, next.name_address + 2u * next.name_length, 2, &repeated_length);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (repeated_length != next.name_length)
        return HPCLANG_RPL_SPAN_MALFORMED;
    next.value_address = next.name_address + 2u * next.name_length + 2u;
    uint32_t value_limit = next.span - (next.value_address - address) - 10u;
    result = hpclang_rpl_object_span(reader, next.value_address, value_limit, &next.value_span);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (next.value_span != value_limit)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = read_word(reader, next.value_address + next.value_span, 5, &prologue);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    if (prologue != 0x02911u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = read_word(reader, next.value_address + next.value_span + 5u, 5, &next.checksum);
    if (result != HPCLANG_RPL_SPAN_OK)
        return result;
    /* Native nibble CRC covers the length field through the checksum BINT's
     * low nibble, excluding the outer prologue and final four CRC nibbles. */
    uint32_t crc = 0;
    for (uint32_t offset = 5u; offset < next.span - 4u; ++offset) {
        uint32_t nibble;
        result = read_word(reader, address + offset, 1, &nibble);
        if (result != HPCLANG_RPL_SPAN_OK)
            return result;
        crc = (crc >> 4u) ^ (((crc ^ nibble) & 15u) * 0x1081u);
    }
    if (crc != next.checksum >> 4u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    *view = next;
    return HPCLANG_RPL_SPAN_OK;
}
