#include "rpl-flash-record.h"
#include "rpl-port.h"
#include <stdlib.h>
#include <string.h>

static uint32_t crc_byte(uint32_t crc, uint8_t value)
{
    crc ^= value;
    for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1u) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    return crc;
}

static uint32_t intent_crc(const uint8_t *data)
{
    uint32_t crc = UINT32_MAX;
    for (uint32_t i = 1; i < HPCLANG_RPL_FLASH_INTENT_BYTES - 4u; ++i)
        crc = crc_byte(crc, data[i]);
    return ~crc;
}

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (uint32_t)data[1] << 8u | (uint32_t)data[2] << 16u | (uint32_t)data[3] << 24u;
}

static void write_le32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        data[i] = (uint8_t)(value >> (i * 8u));
}

static int intent_allocation(uint32_t first, uint32_t end, uint32_t slot, uint32_t bytes)
{
    if (slot < first || slot >= end || bytes < 8u || (bytes & 1u))
        return 0;
    uint32_t offset = (slot - first) % 0x20000u;
    return offset >= 0x105u && bytes < 0x20000u - offset;
}

static int separate(uint32_t a, uint32_t a_bytes, uint32_t b, uint32_t b_bytes)
{
    return a + a_bytes <= b || b + b_bytes <= a;
}

static int intent_bounds(uint32_t first, uint32_t end, uint32_t slot,
                         const struct hpclang_rpl_flash_intent *intent)
{
    return intent && !((first | end) & 1u) && first <= end && end - first == 6u * 0x20000u &&
           intent_allocation(first, end, slot, HPCLANG_RPL_FLASH_INTENT_BYTES) &&
           intent_allocation(first, end, intent->old_slot, intent->old_bytes) &&
           intent_allocation(first, end, intent->new_slot, intent->new_bytes) &&
           separate(slot, HPCLANG_RPL_FLASH_INTENT_BYTES, intent->old_slot, intent->old_bytes) &&
           separate(slot, HPCLANG_RPL_FLASH_INTENT_BYTES, intent->new_slot, intent->new_bytes) &&
           separate(intent->old_slot, intent->old_bytes, intent->new_slot, intent->new_bytes);
}

int hpclang_rpl_flash_intent_encode(uint32_t first, uint32_t end, uint32_t slot,
                                    const struct hpclang_rpl_flash_intent *intent, uint8_t *data,
                                    uint32_t capacity)
{
    if (!data || capacity < HPCLANG_RPL_FLASH_INTENT_BYTES ||
        !intent_bounds(first, end, slot, intent))
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint8_t encoded[HPCLANG_RPL_FLASH_INTENT_BYTES] = {0x7e, 0x56, 0, 0x20};
    memcpy(encoded + 4u, "HPCREPL1", 8u);
    write_le32(encoded + 12u, slot);
    write_le32(encoded + 16u, intent->old_slot);
    write_le32(encoded + 20u, intent->new_slot);
    write_le32(encoded + 24u, intent->old_bytes);
    write_le32(encoded + 28u, intent->new_bytes);
    write_le32(encoded + 32u, intent->old_crc);
    write_le32(encoded + 36u, intent->new_crc);
    write_le32(encoded + 40u, intent_crc(encoded));
    memcpy(data, encoded, sizeof encoded);
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_flash_intent_decode(uint32_t first, uint32_t end, uint32_t slot,
                                    const uint8_t *data, uint32_t bytes,
                                    struct hpclang_rpl_flash_intent *intent,
                                    enum hpclang_rpl_flash_intent_phase *phase)
{
    if (!data || !intent || !phase || bytes != HPCLANG_RPL_FLASH_INTENT_BYTES || data[1] != 0x56u ||
        data[2] || data[3] != 0x20u || memcmp(data + 4u, "HPCREPL1", 8u) ||
        read_le32(data + 12u) != slot || read_le32(data + 40u) != intent_crc(data))
        return HPCLANG_RPL_SPAN_MALFORMED;
    enum hpclang_rpl_flash_intent_phase decoded_phase;
    if (data[0] == 0xffu || data[0] == 0x7fu || data[0] == 0xfeu)
        decoded_phase = HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED;
    else if (data[0] == 0x7eu)
        decoded_phase = HPCLANG_RPL_FLASH_INTENT_READY;
    else if (!data[0])
        decoded_phase = HPCLANG_RPL_FLASH_INTENT_RETIRED;
    else if (!(data[0] & ~0x7eu))
        decoded_phase = HPCLANG_RPL_FLASH_INTENT_RETIRING;
    else
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_flash_intent decoded = {read_le32(data + 16u), read_le32(data + 20u),
                                               read_le32(data + 24u), read_le32(data + 28u),
                                               read_le32(data + 32u), read_le32(data + 36u)};
    if (!intent_bounds(first, end, slot, &decoded))
        return HPCLANG_RPL_SPAN_MALFORMED;
    *intent = decoded;
    *phase = decoded_phase;
    return HPCLANG_RPL_SPAN_OK;
}

static int sector_bounds(uint32_t first, uint32_t end, uint32_t slot, uint32_t target)
{
    return !(first & 0x1ffffu) && first <= end && end - first == 6u * 0x20000u && target >= first &&
           target <= end - HPCLANG_RPL_FLASH_SECTOR_BYTES &&
           !(target & (HPCLANG_RPL_FLASH_SECTOR_BYTES - 1u)) &&
           intent_allocation(first, end, slot, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) &&
           separate(slot, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, target,
                    HPCLANG_RPL_FLASH_SECTOR_BYTES);
}

static uint32_t sector_crc(const uint8_t *data)
{
    uint32_t crc = UINT32_MAX;
    for (uint32_t i = 1; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4u; ++i)
        crc = crc_byte(crc, data[i]);
    return ~crc;
}

int hpclang_rpl_flash_sector_encode(uint32_t first, uint32_t end, uint32_t slot, uint32_t target,
                                    const uint8_t *image, uint8_t *data, uint32_t capacity)
{
    if (!image || !data || capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES ||
        !sector_bounds(first, end, slot, target))
        return HPCLANG_RPL_SPAN_MALFORMED;
    memmove(data + 24u, image, HPCLANG_RPL_FLASH_SECTOR_BYTES);
    uint32_t extent = 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 2u;
    data[0] = 0x7eu;
    data[1] = (uint8_t)extent;
    data[2] = (uint8_t)(extent >> 8u);
    data[3] = (uint8_t)((extent >> 16u) | 0x30u);
    memcpy(data + 4u, "HPCSECT1", 8u);
    write_le32(data + 12u, slot);
    write_le32(data + 16u, target);
    write_le32(data + 20u, HPCLANG_RPL_FLASH_SECTOR_BYTES);
    write_le32(data + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4u, sector_crc(data));
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_flash_sector_decode(uint32_t first, uint32_t end, uint32_t slot,
                                    const uint8_t *data, uint32_t bytes, uint32_t *target,
                                    const uint8_t **image)
{
    if (!data || !target || !image || bytes != HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t extent = 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 2u;
    uint32_t address = read_le32(data + 16u);
    /* Only a fully published image is a recovery candidate. Torn publication
     * and retirement are deliberately not interpreted by this codec. */
    if (data[0] != 0x7eu || data[1] != (uint8_t)extent || data[2] != (uint8_t)(extent >> 8u) ||
        data[3] != (uint8_t)((extent >> 16u) | 0x30u) || memcmp(data + 4u, "HPCSECT1", 8u) ||
        read_le32(data + 12u) != slot || read_le32(data + 20u) != HPCLANG_RPL_FLASH_SECTOR_BYTES ||
        read_le32(data + bytes - 4u) != sector_crc(data) ||
        !sector_bounds(first, end, slot, address))
        return HPCLANG_RPL_SPAN_MALFORMED;
    *target = address;
    *image = data + 24u;
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_flash_record_crc(const struct hpclang_rpl_reader *reader, uint32_t address,
                                 uint32_t bytes, uint32_t *crc)
{
    if (!reader || !reader->read_nibble || !crc || !bytes || address >= 0x100000u ||
        bytes > (0x100000u - address) / 2u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t result = UINT32_MAX;
    for (uint32_t i = 1; i < bytes; ++i) {
        uint8_t low, high;
        if (!reader->read_nibble(reader->context, address + i * 2u, &low) ||
            !reader->read_nibble(reader->context, address + i * 2u + 1u, &high))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (low > 15u || high > 15u)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result = crc_byte(result, (uint8_t)(low | high << 4u));
    }
    *crc = ~result;
    return HPCLANG_RPL_SPAN_OK;
}

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

int hpclang_rpl_flash_allocation_find(const struct hpclang_rpl_reader *reader, uint32_t first,
                                      uint32_t end, uint32_t target,
                                      struct hpclang_rpl_flash_allocation *allocation)
{
    if (!allocation)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (target >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_flash_space tail;
    int result = hpclang_rpl_flash_tail(reader, first, end, &tail);
    if (result)
        return result;
    if (target < first || target >= tail.address)
        return 0;
    for (uint32_t at = first; at < tail.address;) {
        uint32_t header = 0;
        for (unsigned i = 0; i < 7; ++i) {
            uint8_t nibble;
            if (!reader->read_nibble(reader->context, at + i, &nibble))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (nibble > 15)
                return HPCLANG_RPL_SPAN_MALFORMED;
            header |= (uint32_t)nibble << (4 * i);
        }
        uint32_t status = header & 255, extent = header >> 8;
        if ((status != 0 && status != 0x53) || extent < 11)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (tail.address - at < 2 || extent > tail.address - at - 2)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t nibbles = extent + 2;
        if (target - at < nibbles) {
            *allocation = (struct hpclang_rpl_flash_allocation){at, nibbles, status == 0};
            return 1;
        }
        at += nibbles;
    }
    return 0;
}

static int encode_record(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t span,
                         uint32_t prologue, uint8_t **record, uint32_t *bytes)
{
    if (!record || !bytes)
        return HPCLANG_RPL_SPAN_MALFORMED;
    /* A native bank has 0x40000 nibbles, a 0x20A-nibble prefix and an
     * erased end marker. Never allocate a record that cannot fit one bank. */
    if (span > ((0x40000u - 0x20au - 2u) & ~7u) - 8u || address >= 0x100000u ||
        span > 0x100000u - address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_port_cursor cursor = {address, address + span, 0};
    struct hpclang_rpl_port_entry view;
    int result = hpclang_rpl_port_next(reader, &cursor, &view);
    if (result < 0)
        return result;
    if (result != 1 || view.prologue != prologue || view.span != span)
        return HPCLANG_RPL_SPAN_MALFORMED;
    /* Native STO rounds the complete allocation to a four-byte size. The
     * slot itself may start at an odd byte address within the native chain. */
    uint32_t nibbles = (span + 8u + 7u) & ~7u;
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

int hpclang_rpl_flash_backup_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t span, uint8_t **record, uint32_t *bytes)
{
    return encode_record(reader, address, span, 0x02b62u, record, bytes);
}

int hpclang_rpl_flash_library_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                     uint32_t span, uint8_t **record, uint32_t *bytes)
{
    return encode_record(reader, address, span, 0x02b40u, record, bytes);
}
