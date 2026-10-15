#include "rpl-flash-record.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t unavailable = UINT32_MAX;
static int invalid_nibble;

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    if (address == unavailable)
        return 0;
    const uint8_t *data = context;
    *value = invalid_nibble ? 16u : (data[address / 2u] >> ((address & 1u) * 4u)) & 15u;
    return 1;
}

static void crc_tests(void)
{
    uint8_t bytes[] = "X123456789";
    const struct hpclang_rpl_reader reader = {read_nibble, bytes};
    uint32_t crc = 17;
    assert(!hpclang_rpl_flash_record_crc(&reader, 0, 10, &crc));
    assert(crc == UINT32_C(0xcbf43926));
    bytes[0] = 0;
    assert(!hpclang_rpl_flash_record_crc(&reader, 0, 10, &crc));
    assert(crc == UINT32_C(0xcbf43926));
    for (uint32_t at = 2; at < 20; ++at) {
        unavailable = at;
        crc = 17;
        assert(hpclang_rpl_flash_record_crc(&reader, 0, 10, &crc) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(crc == 17);
    }
    unavailable = UINT32_MAX;
    invalid_nibble = 1;
    assert(hpclang_rpl_flash_record_crc(&reader, 0, 10, &crc) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(crc == 17);
    invalid_nibble = 0;
    assert(hpclang_rpl_flash_record_crc(&reader, 0xfffffu, 2, &crc) == HPCLANG_RPL_SPAN_MALFORMED);
}

int main(void)
{
    const uint32_t first = 0x140000u, end = 0x200000u, slot = first + 0x105u;
    const struct hpclang_rpl_flash_intent source = {
        0x160105u, 0x180105u, 32u, 64u, UINT32_C(0x11223344), UINT32_C(0xaabbccdd)};
    uint8_t data[HPCLANG_RPL_FLASH_INTENT_BYTES];
    assert(!hpclang_rpl_flash_intent_encode(first, end, slot, &source, data, sizeof data));
    assert(data[0] == 0x7e && data[1] == 0x56 && data[2] == 0 && data[3] == 0x20);
    assert(!memcmp(data + 4u, "HPCREPL1", 8u));
    /* Independent Python zlib/struct vector fixes the endian/layout contract. */
    const uint8_t golden_crc[] = {0x6a, 0x64, 0xcb, 0xb8};
    assert(!memcmp(data + 40u, golden_crc, sizeof golden_crc));
    struct hpclang_rpl_flash_intent decoded;
    enum hpclang_rpl_flash_intent_phase phase;
    assert(!hpclang_rpl_flash_intent_decode(first, end, slot, data, sizeof data, &decoded, &phase));
    assert(!memcmp(&source, &decoded, sizeof source) && phase == HPCLANG_RPL_FLASH_INTENT_READY);
    for (unsigned status = 0; status < 256; ++status) {
        data[0] = (uint8_t)status;
        decoded = source;
        phase = HPCLANG_RPL_FLASH_INTENT_READY;
        int result =
            hpclang_rpl_flash_intent_decode(first, end, slot, data, sizeof data, &decoded, &phase);
        if (status == 0xff || status == 0x7f || status == 0xfe)
            assert(!result && phase == HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED);
        else if (status == 0x7e)
            assert(!result && phase == HPCLANG_RPL_FLASH_INTENT_READY);
        else if (!status)
            assert(!result && phase == HPCLANG_RPL_FLASH_INTENT_RETIRED);
        else if (!(status & ~0x7eu))
            assert(!result && phase == HPCLANG_RPL_FLASH_INTENT_RETIRING);
        else
            assert(result == HPCLANG_RPL_SPAN_MALFORMED && phase == HPCLANG_RPL_FLASH_INTENT_READY);
        assert(!memcmp(&source, &decoded, sizeof source));
    }
    data[0] = 0x7e;
    phase = HPCLANG_RPL_FLASH_INTENT_READY;
    for (unsigned byte = 1; byte < sizeof data; ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            data[byte] ^= (uint8_t)(1u << bit);
            assert(hpclang_rpl_flash_intent_decode(first, end, slot, data, sizeof data, &decoded,
                                                   &phase) == HPCLANG_RPL_SPAN_MALFORMED);
            assert(!memcmp(&source, &decoded, sizeof source));
            assert(phase == HPCLANG_RPL_FLASH_INTENT_READY);
            data[byte] ^= (uint8_t)(1u << bit);
        }
    }
    assert(hpclang_rpl_flash_intent_decode(first, end, slot + 4u, data, sizeof data, &decoded,
                                           &phase) == HPCLANG_RPL_SPAN_MALFORMED);
    for (uint32_t bytes = 0; bytes < sizeof data; ++bytes) {
        uint8_t before[sizeof data];
        memcpy(before, data, sizeof data);
        assert(hpclang_rpl_flash_intent_encode(first, end, slot, &source, data, bytes) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(before, data, sizeof data));
        assert(hpclang_rpl_flash_intent_decode(first, end, slot, data, bytes, &decoded, &phase) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&source, &decoded, sizeof source));
    }
    assert(hpclang_rpl_flash_intent_encode(first, end - 2u, slot, &source, data, sizeof data) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_flash_intent_encode(first, end, end - 42u, &source, data, sizeof data) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    const uint32_t bad_slots[] = {0,          first, first + 0x104u,  end,
                                  UINT32_MAX, slot,  source.old_slot, source.old_slot + 16u,
                                  end - 16u};
    for (unsigned i = 0; i < sizeof bad_slots / sizeof bad_slots[0]; ++i) {
        struct hpclang_rpl_flash_intent bad = source;
        bad.new_slot = bad_slots[i];
        uint8_t before[sizeof data];
        memcpy(before, data, sizeof data);
        assert(hpclang_rpl_flash_intent_encode(first, end, slot, &bad, data, sizeof data) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(before, data, sizeof data));
    }
    const uint32_t bad_sizes[] = {0, 1, 7, 31, UINT32_MAX, 0x1fefbu, 0x20000u};
    for (unsigned i = 0; i < sizeof bad_sizes / sizeof bad_sizes[0]; ++i) {
        struct hpclang_rpl_flash_intent bad = source;
        bad.new_bytes = bad_sizes[i];
        assert(hpclang_rpl_flash_intent_encode(first, end, slot, &bad, data, sizeof data) ==
               HPCLANG_RPL_SPAN_MALFORMED);
    }
    /* A recomputed checksum must not bypass allocation-overlap validation. */
    memcpy(data + 20u, data + 16u, 4u);
    const struct hpclang_rpl_reader reader = {read_nibble, data};
    uint32_t crc;
    assert(!hpclang_rpl_flash_record_crc(&reader, 0, 40, &crc));
    for (unsigned i = 0; i < 4; ++i)
        data[40u + i] = (uint8_t)(crc >> (8u * i));
    assert(hpclang_rpl_flash_intent_decode(first, end, slot, data, sizeof data, &decoded, &phase) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&source, &decoded, sizeof source));
    crc_tests();
    return 0;
}
