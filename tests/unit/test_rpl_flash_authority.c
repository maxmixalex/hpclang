#include "rpl-flash-write.h"
#include "rpl-flash-record.h"
#include "rpl-flash-replay.h"
#include <assert.h>
#include <string.h>

enum
{
    FIRST = 0x140000,
    END = 0x200000,
    JOURNAL = 0x160105,
    ROOT = 0x180105
};
static uint8_t flash[END - FIRST], initial[END - FIRST];
static uint8_t image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
static uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
static unsigned reads, programs, erases, fail_read, fail_program;
static uint16_t program_mask;
static int closing, replaying, retiring, silent;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    assert(context == flash && !(address & 1u) && address >= FIRST && address < END);
    if (++reads == fail_read)
        return 0;
    *value = (uint16_t)(flash[address - FIRST] | (uint16_t)flash[address - FIRST + 1] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    assert(context == flash && !(address & 1u));
    if (retiring) {
        assert(address == (JOURNAL & ~1u));
        assert(!memcmp(flash, image, sizeof image));
        assert(!memcmp(flash + ROOT - FIRST, authority, sizeof authority));
        ++programs;
        flash[address - FIRST] &= (uint8_t)value;
        flash[address - FIRST + 1] &= (uint8_t)(value >> 8u);
        return 1;
    }
    if (replaying) {
        assert(address >= FIRST && address < FIRST + sizeof image);
        ++programs;
        flash[address - FIRST] &= (uint8_t)value;
        flash[address - FIRST + 1] &= (uint8_t)(value >> 8u);
        return 1;
    }
    assert(address >= (ROOT & ~1u) && address < ROOT + sizeof authority);
    if (closing)
        assert(address == (ROOT & ~1u));
    if (address == (ROOT & ~1u)) {
        assert(!memcmp(flash + ROOT - FIRST + 1, authority + 1, sizeof authority - 1));
        assert(!memcmp(flash + JOURNAL - FIRST + 1, journal + 1, sizeof journal - 1));
        assert(flash[JOURNAL - FIRST] == (closing ? 0 : 0x7e));
    }
    ++programs;
    if (silent)
        return 1;
    if (programs == fail_program)
        value |= (uint16_t)~program_mask;
    flash[address - FIRST] &= (uint8_t)value;
    flash[address - FIRST + 1] &= (uint8_t)(value >> 8u);
    return programs != fail_program;
}

static const struct hpclang_rpl_flash_io io = {flash, read16, program16, FIRST, END};

static int erase(void *context, uint32_t address)
{
    assert(context == flash && replaying && address == FIRST);
    ++erases;
    memset(flash, 255, sizeof image);
    return 1;
}

static const struct hpclang_rpl_flash_replay_io replay_io = {{flash, read16, program16, FIRST, END},
                                                             erase};

static void prepare(int close)
{
    memcpy(flash, initial, sizeof flash);
    closing = close;
    if (close) {
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[JOURNAL - FIRST] = 0;
    }
    reads = programs = erases = fail_read = fail_program = 0;
    program_mask = 0xffff;
    silent = replaying = retiring = 0;
}

static int publish(void)
{
    int result = hpclang_rpl_flash_publish_authority(&io, ROOT, JOURNAL, journal, sizeof journal);
    assert(!memcmp(flash, initial, ROOT - FIRST));
    assert(!memcmp(flash + ROOT - FIRST + sizeof authority,
                   initial + ROOT - FIRST + sizeof authority,
                   sizeof flash - (ROOT - FIRST) - sizeof authority));
    if (flash[ROOT - FIRST] == 0x7e)
        assert(!memcmp(flash + ROOT - FIRST, authority, sizeof authority));
    return result;
}

static int close_transaction(void)
{
    return hpclang_rpl_flash_close_authority(&io, ROOT, JOURNAL, journal, sizeof journal);
}

static void finish_publication(void)
{
    fail_read = fail_program = 0;
    silent = 0;
    assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash + ROOT - FIRST, authority, sizeof authority));
    programs = 0;
    assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!programs && !memcmp(flash, initial, ROOT - FIRST));
}

static void publication_tests(void)
{
    prepare(0);
    assert(publish() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned writes = programs, read_count = reads;
    assert(writes && !memcmp(flash + ROOT - FIRST, authority, sizeof authority));
    programs = 0;
    assert(publish() == HPCLANG_RPL_FLASH_OCCUPIED && !programs);
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xffff};
    for (unsigned cut = 1; cut <= writes; ++cut) {
        for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
            prepare(0);
            fail_program = cut;
            program_mask = masks[mask];
            assert(publish() == HPCLANG_RPL_FLASH_DIRTY);
            assert(programs == cut);
            finish_publication();
        }
    }
    for (unsigned cut = 1; cut <= read_count; ++cut) {
        prepare(0);
        fail_read = cut;
        int result = publish();
        assert(result == HPCLANG_RPL_FLASH_FAILED || result == HPCLANG_RPL_FLASH_DIRTY);
        finish_publication();
    }
    prepare(0);
    silent = 1;
    assert(publish() == HPCLANG_RPL_FLASH_DIRTY && flash[ROOT - FIRST] == 0xff);
    prepare(0);
    flash[ROOT - FIRST + sizeof authority] = 0;
    assert(hpclang_rpl_flash_publish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_OCCUPIED);
    assert(!programs);
    for (unsigned i = 0; i < sizeof journal; ++i) {
        prepare(0);
        flash[JOURNAL - FIRST + i] ^= 1;
        assert(hpclang_rpl_flash_publish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs);
    }
}

static void partial_publication(void)
{
    prepare(0);
    fail_program = 3;
    program_mask = 0x00ff;
    assert(publish() == HPCLANG_RPL_FLASH_DIRTY && flash[ROOT - FIRST] == 0xff);
    programs = reads = fail_program = 0;
}

static void resumption_tests(void)
{
    partial_publication();
    assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    unsigned writes = programs, read_count = reads;
    assert(writes);
    for (unsigned cut = 1; cut <= writes; ++cut) {
        for (unsigned mask = 0; mask < 3; ++mask) {
            partial_publication();
            fail_program = cut;
            program_mask = mask == 0 ? 0 : mask == 1 ? 0x5555 : 0xffff;
            assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal,
                                                      sizeof journal) == HPCLANG_RPL_FLASH_DIRTY);
            assert(programs == cut);
            finish_publication();
        }
    }
    for (unsigned cut = 1; cut <= read_count; ++cut) {
        partial_publication();
        fail_read = cut;
        assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
               HPCLANG_RPL_FLASH_DIRTY);
        finish_publication();
    }
    for (unsigned status = 0; status < 256; ++status) {
        prepare(0);
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[ROOT - FIRST] = (uint8_t)status;
        int result =
            hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal);
        if (status == 0xff || status == 0x7f || status == 0xfe || status == 0x7e)
            assert(result == HPCLANG_RPL_FLASH_WRITTEN && flash[ROOT - FIRST] == 0x7e);
        else
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
    }
    const uint8_t publishing[] = {0x7f, 0xfe, 0x7e};
    for (unsigned i = 0; i < sizeof publishing; ++i) {
        prepare(0);
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[ROOT - FIRST] = publishing[i];
        flash[ROOT - FIRST + 4] |= 0x80;
        assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs);
    }
    prepare(0);
    flash[ROOT - FIRST + 4] = 0; /* Cannot reconstruct the expected H without erasure. */
    assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs);
    prepare(0);
    flash[ROOT - FIRST + sizeof authority] = 0;
    assert(hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs);
}

static void closure_tests(void)
{
    prepare(1);
    assert(close_transaction() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned read_count = reads;
    assert(programs == 1 && flash[ROOT - FIRST] == 0);
    programs = 0;
    assert(close_transaction() == HPCLANG_RPL_FLASH_WRITTEN && !programs);
    for (unsigned mask = 0; mask < 256; ++mask) {
        prepare(1);
        fail_program = 1;
        program_mask = (uint16_t)(mask << 8u);
        assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY);
        assert((flash[ROOT - FIRST] & 0x7e) == flash[ROOT - FIRST]);
        assert(flash[ROOT - FIRST - 1] == 0xa5);
        fail_program = 0;
        assert(close_transaction() == HPCLANG_RPL_FLASH_WRITTEN);
        assert(flash[ROOT - FIRST] == 0);
    }
    for (unsigned cut = 1; cut <= read_count; ++cut) {
        prepare(1);
        fail_read = cut;
        assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY);
        fail_read = 0;
        assert(close_transaction() == HPCLANG_RPL_FLASH_WRITTEN);
    }
    for (unsigned status = 0; status < 256; ++status) {
        prepare(1);
        flash[ROOT - FIRST] = (uint8_t)status;
        int result = close_transaction();
        if (!(status & ~0x7eu))
            assert(result == HPCLANG_RPL_FLASH_WRITTEN && flash[ROOT - FIRST] == 0);
        else
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
    }
    for (unsigned status = 1; status < 256; ++status) {
        prepare(1);
        flash[JOURNAL - FIRST] = (uint8_t)status;
        assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY && !programs);
    }
    prepare(1);
    flash[123] ^= 1;
    assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY && !programs);
    prepare(1);
    flash[ROOT - FIRST + 17] ^= 1;
    assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY && !programs);
    prepare(1);
    flash[JOURNAL - FIRST + 200] ^= 1;
    assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY && !programs);
    prepare(1);
    silent = 1;
    assert(close_transaction() == HPCLANG_RPL_FLASH_DIRTY && flash[ROOT - FIRST] == 0x7e);
}

static void bound_replay_tests(void)
{
    uint8_t scratch[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    for (unsigned status = 0; status < 256; ++status) {
        prepare(0);
        replaying = 1;
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[ROOT - FIRST] = (uint8_t)status;
        flash[123] ^= 1;
        int result = hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal,
                                                           sizeof journal, scratch, sizeof scratch);
        if (status == 0x7e) {
            assert(result == HPCLANG_RPL_FLASH_WRITTEN && erases == 1);
            assert(!memcmp(flash, image, sizeof image));
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs && !erases);
            assert(flash[123] == (uint8_t)(image[123] ^ 1));
        }
        assert(flash[ROOT - FIRST] == status);
        assert(!memcmp(flash + ROOT - FIRST + 1, authority + 1, sizeof authority - 1));
        assert(!memcmp(flash + JOURNAL - FIRST, journal, sizeof journal));
    }
    /* A binding cannot hide a changed/retired journal or failed binding read. */
    for (unsigned cut = 1; cut <= 17; ++cut) {
        prepare(0);
        replaying = 1;
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        fail_read = cut;
        assert(hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal,
                                                     sizeof journal, scratch,
                                                     sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases);
    }
    const unsigned changed[] = {ROOT - FIRST + 10, JOURNAL - FIRST, JOURNAL - FIRST + 200};
    for (unsigned i = 0; i < sizeof changed / sizeof changed[0]; ++i) {
        prepare(0);
        replaying = 1;
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[changed[i]] ^= 1;
        assert(hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal,
                                                     sizeof journal, scratch,
                                                     sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases);
    }
}

static void transaction_test(void)
{
    uint8_t scratch[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    prepare(0);
    assert(publish() == HPCLANG_RPL_FLASH_WRITTEN);
    flash[123] ^= 1;
    replaying = 1;
    assert(hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal, sizeof journal,
                                                 scratch,
                                                 sizeof scratch) == HPCLANG_RPL_FLASH_WRITTEN);
    replaying = 0;
    retiring = 1;
    assert(hpclang_rpl_flash_retire_sector(&io, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    retiring = 0;
    closing = 1;
    assert(close_transaction() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(flash[ROOT - FIRST] == 0 && flash[JOURNAL - FIRST] == 0);
    assert(!memcmp(flash, image, sizeof image));
    /* Later data must survive attempts to reuse this completed transaction. */
    flash[123] ^= 2;
    programs = erases = 0;
    replaying = 1;
    assert(hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal, sizeof journal,
                                                 scratch,
                                                 sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs && !erases && flash[123] == (uint8_t)(image[123] ^ 2));
}

int main(void)
{
    memset(initial, 255, sizeof initial);
    memset(image, 0x42, sizeof image);
    assert(hpclang_rpl_flash_sector_encode(FIRST, END, JOURNAL, FIRST, image, journal,
                                           sizeof journal) == 0);
    assert(hpclang_rpl_flash_authority_encode(FIRST, END, ROOT, JOURNAL, journal, sizeof journal,
                                              authority, sizeof authority) == 0);
    memcpy(initial, image, sizeof image);
    memcpy(initial + JOURNAL - FIRST, journal, sizeof journal);
    initial[ROOT - FIRST - 1] = 0xa5;
    publication_tests();
    resumption_tests();
    closure_tests();
    bound_replay_tests();
    transaction_test();
    return 0;
}
