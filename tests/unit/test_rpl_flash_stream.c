#include "rpl-flash-record.h"
#include <assert.h>
#include <string.h>

enum
{
    FIRST = 0x140000,
    END = 0x200000,
    JOURNAL = 0x160105,
    AUTHORITY = 0x180105
};
static uint8_t image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
static uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
static unsigned calls, fail_at, invalid_at;
static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    assert(context == journal && at >= 13 && at - 13 < 2 * sizeof journal);
    ++calls;
    if (calls == fail_at)
        return 0;
    at -= 13; /* Deliberately exercise an odd local nibble origin. */
    *value = calls == invalid_at ? 16 : (journal[at / 2] >> (4 * (at & 1))) & 15;
    return 1;
}
static const struct hpclang_rpl_reader reader = {read_nibble, journal};
static const struct hpclang_rpl_flash_sector_descriptor sentinel = {91, 92, 93};

static void compare(void)
{
    uint32_t target;
    const uint8_t *data;
    enum hpclang_rpl_flash_sector_phase phase;
    struct hpclang_rpl_flash_sector_descriptor descriptor = sentinel;
    int memory = hpclang_rpl_flash_sector_inspect(FIRST, END, JOURNAL, journal, sizeof journal,
                                                  &target, &data, &phase);
    int streamed =
        hpclang_rpl_flash_sector_inspect_reader(FIRST, END, JOURNAL, &reader, 13, &descriptor);
    assert(memory == streamed);
    if (!memory) {
        assert(target == descriptor.target && phase == descriptor.phase && data == journal + 24);
        enum hpclang_rpl_flash_authority_phase old_phase = 99, new_phase = 99;
        int old_result =
            hpclang_rpl_flash_authority_inspect(FIRST, END, AUTHORITY, authority, sizeof authority,
                                                JOURNAL, journal, sizeof journal, &old_phase);
        int new_result = hpclang_rpl_flash_authority_inspect_descriptor(
            FIRST, END, AUTHORITY, authority, sizeof authority, JOURNAL, &descriptor, &new_phase);
        assert(old_result == new_result && old_phase == new_phase);
    } else {
        assert(!memcmp(&descriptor, &sentinel, sizeof descriptor));
    }
}
int main(void)
{
    for (unsigned i = 0; i < sizeof image; ++i)
        image[i] = (uint8_t)(i * 37 + 11);
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, JOURNAL, FIRST, image, journal,
                                            sizeof journal));
    assert(!hpclang_rpl_flash_authority_encode(FIRST, END, AUTHORITY, JOURNAL, journal,
                                               sizeof journal, authority, sizeof authority));
    for (unsigned status = 0; status < 256; ++status) {
        journal[0] = (uint8_t)status;
        compare();
    }
    journal[0] = 0x7e;
    for (unsigned status = 0; status < 256; ++status) {
        authority[0] = (uint8_t)status;
        compare();
    }
    authority[0] = 0x7e;
    for (unsigned i = 1; i < sizeof journal; ++i) {
        journal[i] ^= 1;
        compare();
        journal[i] ^= 1;
    }
    for (unsigned i = 1; i < sizeof authority; ++i) {
        authority[i] ^= 1;
        compare();
        authority[i] ^= 1;
    }
    for (unsigned failure = 1; failure <= 2 * sizeof journal; ++failure) {
        calls = 0;
        fail_at = failure;
        struct hpclang_rpl_flash_sector_descriptor descriptor = sentinel;
        assert(hpclang_rpl_flash_sector_inspect_reader(
                   FIRST, END, JOURNAL, &reader, 13, &descriptor) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&descriptor, &sentinel, sizeof descriptor));
    }
    fail_at = 0;
    for (unsigned invalid = 1; invalid <= 2 * sizeof journal; invalid += 127) {
        calls = 0;
        invalid_at = invalid;
        struct hpclang_rpl_flash_sector_descriptor descriptor = sentinel;
        assert(hpclang_rpl_flash_sector_inspect_reader(FIRST, END, JOURNAL, &reader, 13,
                                                       &descriptor) == HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&descriptor, &sentinel, sizeof descriptor));
    }
    invalid_at = 0;
    struct hpclang_rpl_flash_sector_descriptor descriptor = sentinel;
    assert(hpclang_rpl_flash_sector_inspect_reader(FIRST, END, JOURNAL, &reader, UINT32_MAX,
                                                   &descriptor) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&descriptor, &sentinel, sizeof descriptor));
    return 0;
}
