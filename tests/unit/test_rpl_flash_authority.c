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

static void staging_tests(void)
{
    prepare(0);
    assert(!hpclang_rpl_flash_stage_authority(&io, ROOT, JOURNAL, journal, sizeof journal));
    unsigned writes = programs;
    assert(writes && flash[ROOT - FIRST] == 0x7f);
    assert(!memcmp(flash + ROOT - FIRST + 1, authority + 1, sizeof authority - 1));
    programs = 0;
    assert(hpclang_rpl_flash_stage_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
           HPCLANG_RPL_FLASH_OCCUPIED);
    assert(!programs);
    uint8_t scratch[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    assert(hpclang_rpl_flash_sector_replay_bound(&replay_io, ROOT, JOURNAL, journal, sizeof journal,
                                                 scratch,
                                                 sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs && !erases);
    assert(!hpclang_rpl_flash_finish_authority(&io, ROOT, JOURNAL, journal, sizeof journal));
    assert(programs == 1 && flash[ROOT - FIRST] == 0x7e);
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xffff};
    for (unsigned cut = 1; cut <= writes; ++cut) {
        for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
            prepare(0);
            fail_program = cut;
            program_mask = masks[mask];
            assert(hpclang_rpl_flash_stage_authority(&io, ROOT, JOURNAL, journal, sizeof journal) ==
                   HPCLANG_RPL_FLASH_DIRTY);
            assert(programs == cut && flash[ROOT - FIRST] != 0x7e);
            fail_program = 0;
            assert(!hpclang_rpl_flash_finish_staged_authority(&io, ROOT, JOURNAL, journal,
                                                              sizeof journal));
            assert(flash[ROOT - FIRST] == 0x7f);
            assert(!memcmp(flash + ROOT - FIRST + 1, authority + 1, sizeof authority - 1));
            assert(!memcmp(flash, initial, ROOT - FIRST));
            assert(!memcmp(flash + ROOT - FIRST + sizeof authority,
                           initial + ROOT - FIRST + sizeof authority,
                           sizeof flash - (ROOT - FIRST) - sizeof authority));
            programs = 0;
            assert(!hpclang_rpl_flash_finish_staged_authority(&io, ROOT, JOURNAL, journal,
                                                              sizeof journal));
            assert(!programs && !erases);
        }
    }
    for (unsigned status = 0; status < 256; ++status) {
        prepare(0);
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[ROOT - FIRST] = (uint8_t)status;
        int result =
            hpclang_rpl_flash_finish_staged_authority(&io, ROOT, JOURNAL, journal, sizeof journal);
        if (status == 0xff || status == 0x7f)
            assert(!result && flash[ROOT - FIRST] == 0x7f &&
                   programs == (unsigned)(status == 0xff));
        else
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs && flash[ROOT - FIRST] == status);
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        prepare(0);
        memcpy(flash + ROOT - FIRST, authority, sizeof authority);
        flash[ROOT - FIRST] = 0x7f;
        if (fault == 0)
            flash[ROOT - FIRST + 8] ^= 1;
        else if (fault == 1)
            flash[JOURNAL - FIRST + 100] ^= 1;
        else if (fault == 2)
            flash[ROOT - FIRST + sizeof authority] = 0;
        else
            fail_read = 1;
        assert(hpclang_rpl_flash_finish_staged_authority(
                   &io, ROOT, JOURNAL, journal, sizeof journal) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases);
    }
}

static void activation_fixture(unsigned phase)
{
    prepare(0);
    memcpy(flash + ROOT - FIRST, authority, sizeof authority);
    flash[ROOT - FIRST] = phase;
    /* A published successor must survive activation unchanged. */
    memset(flash + ROOT - FIRST + sizeof authority, 0x53, 240);
}

static int activate(void)
{
    return hpclang_rpl_flash_activate_staged_authority(&io, ROOT, JOURNAL, journal, sizeof journal);
}

static void activation_tests(void)
{
    static uint8_t expected[sizeof flash];
    for (unsigned phase = 0; phase < 256; ++phase) {
        activation_fixture(phase);
        memcpy(expected, flash, sizeof flash);
        int result = activate();
        if (phase == 0x7f || phase == 0x7e) {
            assert(!result && programs == (unsigned)(phase == 0x7f));
            expected[ROOT - FIRST] = 0x7e;
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
        }
        assert(!memcmp(flash, expected, sizeof flash) && !erases);
    }
    activation_fixture(0x7f);
    assert(!activate());
    unsigned read_count = reads;
    for (unsigned cut = 1; cut <= read_count; ++cut) {
        activation_fixture(0x7f);
        fail_read = cut;
        assert(activate() == HPCLANG_RPL_FLASH_DIRTY);
        fail_read = 0;
        assert(!activate());
        assert(programs == 1 && flash[ROOT - FIRST] == 0x7e);
    }
    for (unsigned mask = 0; mask < 2; ++mask) {
        activation_fixture(0x7f);
        fail_program = 1;
        program_mask = mask ? 0xffff : 0;
        assert(activate() == HPCLANG_RPL_FLASH_DIRTY);
        assert(flash[ROOT - FIRST] == (mask ? 0x7e : 0x7f));
        fail_program = 0;
        assert(!activate());
        assert(programs == (mask ? 1u : 2u));
    }
    for (unsigned fault = 0; fault < 3; ++fault) {
        activation_fixture(0x7f);
        if (fault == 0)
            flash[ROOT - FIRST + 8] ^= 1;
        else if (fault == 1)
            flash[JOURNAL - FIRST + 100] ^= 1;
        else
            flash[JOURNAL - FIRST] = 0;
        memcpy(expected, flash, sizeof flash);
        assert(activate() == HPCLANG_RPL_FLASH_DIRTY && !programs);
        assert(!memcmp(flash, expected, sizeof flash));
    }
    activation_fixture(0x7f);
    silent = 1;
    assert(activate() == HPCLANG_RPL_FLASH_DIRTY);
    assert(flash[ROOT - FIRST] == 0x7f);
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

static void prepare_native_port(uint8_t authority_status, uint8_t journal_status,
                                uint32_t authority_slot, uint32_t journal_slot)
{
    prepare(0);
    memset(flash, 255, sizeof flash);
    for (unsigned bank = 0; bank < 6; ++bank) {
        flash[bank * 0x20000 + 0x100] = 0xf0;
        flash[bank * 0x20000 + 0x101] = (uint8_t)(8 + bank);
    }
    memcpy(image, flash, sizeof image);
    assert(hpclang_rpl_flash_sector_encode(FIRST, END, journal_slot, FIRST, image, journal,
                                           sizeof journal) == 0);
    assert(hpclang_rpl_flash_authority_encode(FIRST, END, authority_slot, journal_slot, journal,
                                              sizeof journal, authority, sizeof authority) == 0);
    memcpy(flash + journal_slot - FIRST, journal, sizeof journal);
    memcpy(flash + authority_slot - FIRST, authority, sizeof authority);
    flash[journal_slot - FIRST] = journal_status;
    flash[authority_slot - FIRST] = authority_status;
}

static void tombstone(uint32_t slot, uint32_t bytes)
{
    uint32_t extent = 2 * bytes - 2;
    flash[slot - FIRST] = 0;
    flash[slot - FIRST + 1] = (uint8_t)extent;
    flash[slot - FIRST + 2] = (uint8_t)(extent >> 8);
    flash[slot - FIRST + 3] = (uint8_t)(extent >> 16);
}

static void bound_same_bank_tests(void)
{
    const uint32_t slot = FIRST + 0x2005u, root = slot + sizeof journal;
    uint8_t scratch[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    prepare_native_port(0x7e, 0x7e, root, slot);
    tombstone(FIRST + 0x105u, slot - FIRST - 0x105u);
    memcpy(image, flash, sizeof image);
    assert(hpclang_rpl_flash_sector_encode(FIRST, END, slot, FIRST, image, journal,
                                           sizeof journal) == 0);
    assert(hpclang_rpl_flash_authority_encode(FIRST, END, root, slot, journal, sizeof journal,
                                              authority, sizeof authority) == 0);
    memcpy(flash + slot - FIRST, journal, sizeof journal);
    memcpy(flash + root - FIRST, authority, sizeof authority);
    memset(flash, 255, sizeof image);
    memcpy(initial, flash, sizeof flash);
    uint32_t target = 123;
    assert(hpclang_rpl_flash_preflight_bound(&io, root, slot, scratch, sizeof scratch, &target) ==
           HPCLANG_RPL_FLASH_SECTOR_READY);
    assert(target == FIRST && !programs && !erases);
    assert(!memcmp(initial, flash, sizeof flash));
    assert(!memcmp(scratch, journal, sizeof journal));
    assert(flash[0x100] == 255);
    flash[slot - FIRST] = 0x7c;
    target = 123;
    assert(hpclang_rpl_flash_preflight_bound(&io, root, slot, scratch, sizeof scratch, &target) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(target == 123);
    memcpy(flash, image, sizeof image);
    assert(hpclang_rpl_flash_preflight_bound(&io, root, slot, scratch, sizeof scratch, &target) ==
           HPCLANG_RPL_FLASH_SECTOR_RETIRING);
    assert(target == FIRST && !programs && !erases);
    uint8_t foreign[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    assert(hpclang_rpl_flash_sector_encode(FIRST, END, ROOT, FIRST, image, foreign,
                                           sizeof foreign) == 0);
    memcpy(flash + ROOT - FIRST, foreign, sizeof foreign);
    target = 123;
    assert(hpclang_rpl_flash_preflight_bound(&io, root, slot, scratch, sizeof scratch, &target) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(target == 123 && !programs && !erases);
}

static void bound_preflight_tests(void)
{
    uint8_t scratch[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint32_t target = 123;
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    memset(flash, 255, sizeof image);
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_SECTOR_READY);
    assert(target == FIRST && !programs && !erases);
    assert(!memcmp(scratch, journal, sizeof journal) && flash[0x100] == 0xff);
    const unsigned read_cuts[] = {1, 17, 18, 128, 2081};
    for (unsigned i = 0; i < sizeof read_cuts / sizeof read_cuts[0]; ++i) {
        prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
        target = 123;
        fail_read = read_cuts[i];
        assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                                 &target) < 0);
        assert(target == 123 && !programs && !erases);
    }
    const uint8_t statuses[] = {0, 0x02, 0x7c, 0x7e, 0x7f, 0xfe, 0xff, 0x53};
    for (unsigned i = 0; i < sizeof statuses; ++i) {
        prepare_native_port(statuses[i], 0, ROOT, JOURNAL);
        target = 123;
        int result =
            hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch, &target);
        if (!statuses[i])
            assert(result == 0 && target == 123);
        else if (!(statuses[i] & ~0x7eu))
            assert(result == HPCLANG_RPL_FLASH_SECTOR_RETIRING && target == FIRST);
        else
            assert(result == HPCLANG_RPL_FLASH_DIRTY && target == 123);
        assert(!programs && !erases);
    }
    prepare_native_port(0, 0x7e, ROOT, JOURNAL);
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0x7e, 0x7c, ROOT, JOURNAL);
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_SECTOR_RETIRING);
    flash[123] ^= 1;
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_DIRTY);
    /* Valid-looking bindings/journals nested in deleted payloads are not owned
     * allocations, even when all checksums and their mutual binding agree. */
    prepare_native_port(0x7e, 0x7e, ROOT + 8, JOURNAL);
    tombstone(ROOT, sizeof authority + 8);
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT + 8, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0, 0, ROOT, JOURNAL + 8);
    tombstone(JOURNAL, sizeof journal + 8);
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL + 8, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_DIRTY);
    /* A closed transaction must validate today's port, without overlaying its
     * historical sector image or dropping a later native backup. */
    static const uint8_t backup[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03,
                                     0x00, 0x08, 0x46, 0x4c, 0x41, 0x53, 0x48, 0x4f,
                                     0x4e, 0x45, 0x08, 0x14, 0x26, 0x90, 0x00, 0x00,
                                     0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
    prepare_native_port(0, 0, ROOT, JOURNAL);
    memcpy(flash + 0x105, backup, sizeof backup);
    target = 123;
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == 0);
    assert(target == 123 && !programs && !erases && !memcmp(flash + 0x105, backup, sizeof backup));
    flash[0x110] ^= 1;
    assert(hpclang_rpl_flash_preflight_bound(&io, ROOT, JOURNAL, scratch, sizeof scratch,
                                             &target) == HPCLANG_RPL_FLASH_DIRTY);
}

static int membership(uint32_t root, uint32_t slot)
{
    return hpclang_rpl_flash_bound_membership(&io, root, flash + root - FIRST, sizeof authority,
                                              slot, flash + slot - FIRST, sizeof journal);
}

static void bound_membership_tests(void)
{
    /* Check the loaded pair without allocating another sector buffer. */
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    memset(flash, 255, sizeof image);
    memcpy(initial, flash, sizeof flash);
    assert(membership(ROOT, JOURNAL) == 0);
    assert(!programs && !erases && !memcmp(initial, flash, sizeof flash));
    prepare_native_port(0x7e, 0x7e, JOURNAL + sizeof journal, JOURNAL);
    assert(membership(JOURNAL + sizeof journal, JOURNAL) == 0);
    prepare_native_port(0x7e, 0x7e, ROOT + 8, JOURNAL);
    tombstone(ROOT, sizeof authority + 8);
    assert(membership(ROOT + 8, JOURNAL) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0, 0, ROOT, JOURNAL + 8);
    tombstone(JOURNAL, sizeof journal + 8);
    assert(membership(ROOT, JOURNAL + 8) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    flash[ROOT - FIRST - 4] ^= 1; /* Wrong authority-bank identity. */
    assert(membership(ROOT, JOURNAL) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    flash[ROOT - FIRST + sizeof authority + 4] = 0;
    assert(membership(ROOT, JOURNAL) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0x7f, 0x7e, ROOT, JOURNAL);
    assert(membership(ROOT, JOURNAL) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_native_port(0, 0, ROOT, JOURNAL);
    flash[123] ^= 1; /* Closed records do not impose their historical image. */
    assert(membership(ROOT, JOURNAL) == 0);
    fail_read = reads + 1;
    assert(membership(ROOT, JOURNAL) == HPCLANG_RPL_FLASH_FAILED);
    assert(!programs && !erases);
}

static void packed_word(uint8_t *data, uint32_t at, uint32_t value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i, ++at) {
        unsigned shift = 4u * (at & 1u);
        data[at / 2u] =
            (uint8_t)((data[at / 2u] & ~(15u << shift)) | (((value >> (4u * i)) & 15u) << shift));
    }
}

static uint8_t catalog_data[100];
static int read_catalog(void *context, uint32_t at, uint8_t *value)
{
    assert(context == catalog_data);
    if (at >= sizeof catalog_data * 2)
        return 0;
    *value = (uint8_t)((catalog_data[at / 2u] >> (4u * (at & 1u))) & 15u);
    return 1;
}

static void seal_library(uint8_t library[64])
{
    uint32_t crc = 0;
    for (unsigned at = 13; at < 124; ++at) {
        unsigned nibble = (library[at / 2u] >> (4u * (at & 1u))) & 15u;
        crc = (crc >> 4u) ^ (((crc ^ nibble) & 15u) * 0x1081u);
    }
    packed_word(library, 124, crc, 4);
}

static uint32_t cancel_journal, cancel_authority;

static int cancel_program(void *context, uint32_t address, uint16_t value)
{
    assert(context == flash);
    assert((address >= (cancel_journal & ~1u) && address < cancel_journal + 4) ||
           (address >= (cancel_authority & ~1u) && address < cancel_authority + 4));
    if (address >= (cancel_authority & ~1u) && address < cancel_authority + 4) {
        uint32_t extent = 2u * sizeof journal - 2u;
        assert(flash[cancel_journal - FIRST] == 0);
        for (unsigned byte = 1; byte < 4; ++byte)
            assert(flash[cancel_journal - FIRST + byte] == (uint8_t)(extent >> (8u * (byte - 1u))));
    }
    ++programs;
    if (silent)
        return 1;
    if (programs == fail_program)
        value |= (uint16_t)~program_mask;
    flash[address - FIRST] &= (uint8_t)value;
    flash[address - FIRST + 1] &= (uint8_t)(value >> 8u);
    assert(flash[cancel_journal - FIRST] != 0x53 && flash[cancel_authority - FIRST] != 0x53);
    return programs != fail_program;
}

static void cancellation_tests(const struct hpclang_rpl_flash_recovery_layout *layout,
                               const struct hpclang_rpl_flash_catalog *catalog)
{
    static uint8_t before[END - FIRST], expected[END - FIRST];
    const struct hpclang_rpl_flash_io cancel_io = {flash, read16, cancel_program, FIRST, END};
    const uint16_t masks[] = {0, 0xff, 0xff00, 0x5555, 0xffff};
    for (unsigned state = 0; state < 4; ++state) {
        prepare_native_port(0xff, state ? 0x7e : 0xff, cancel_authority, cancel_journal);
        memcpy(flash + 0x1a0105 - FIRST, layout->record, layout->bytes);
        if (!state)
            memset(flash + cancel_journal - FIRST, 255, sizeof journal);
        if (state < 2)
            memset(flash + cancel_authority - FIRST, 255, sizeof authority);
        if (state == 3)
            flash[cancel_authority - FIRST] = 0x7f;
        memcpy(before, flash, sizeof flash);
        tombstone(cancel_journal, sizeof journal);
        tombstone(cancel_authority, sizeof authority);
        memcpy(expected, flash, sizeof flash);
        memcpy(flash, before, sizeof flash);
        reads = programs = fail_read = fail_program = 0;
        silent = 0;
        assert(!hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog));
        unsigned total = programs;
        unsigned total_reads = reads;
        assert(total && !memcmp(flash, expected, sizeof flash));
        programs = 0;
        assert(!hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog));
        assert(!programs && !memcmp(flash, expected, sizeof flash));
        for (unsigned cut = 1; cut <= total; ++cut)
            for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
                memcpy(flash, before, sizeof flash);
                reads = programs = 0;
                fail_program = cut;
                program_mask = masks[mask];
                assert(hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog) < 0);
                fail_program = 0;
                assert(!hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog));
                assert(!memcmp(flash, expected, sizeof flash));
            }
        /* Exercise early admission and final verification failures, plus reads
         * distributed through the complete catalog/allocation traversal. */
        for (unsigned sample = 0; sample < 48; ++sample) {
            memcpy(flash, before, sizeof flash);
            reads = programs = 0;
            fail_read = sample < 16   ? sample + 1
                        : sample < 32 ? total_reads - (sample - 16)
                                      : 1 + (total_reads - 1) / 16 * (sample - 32);
            assert(hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog) < 0);
            fail_read = 0;
            assert(!hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog));
            assert(!memcmp(flash, expected, sizeof flash));
        }
        memcpy(flash, before, sizeof flash);
        programs = 0;
        silent = 1;
        assert(hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog) < 0);
        assert(!memcmp(flash, before, sizeof flash));
        silent = 0;
    }
    const uint8_t rejected[][2] = {{0x7e, 0x7e}, {0, 0}, {0xff, 0}};
    for (unsigned i = 0; i < sizeof rejected / sizeof rejected[0]; ++i) {
        prepare_native_port(rejected[i][0], rejected[i][1], cancel_authority, cancel_journal);
        memcpy(flash + 0x1a0105 - FIRST, layout->record, layout->bytes);
        memcpy(before, flash, sizeof flash);
        assert(hpclang_rpl_flash_cancel_installed(&cancel_io, layout, catalog) < 0);
        assert(!programs && !memcmp(flash, before, sizeof flash));
    }
}

static void installed_preflight_tests(void)
{
    enum
    {
        LIBRARY = 0x1a0105
    };
    uint8_t library[64] = {0x53, 0x7e, 0, 0x10};
    packed_word(library, 8, 0x2b40, 5);
    packed_word(library, 13, 115, 5);
    packed_word(library, 18, 1, 2);
    packed_word(library, 20, 'X', 2);
    packed_word(library, 22, 1, 2);
    packed_word(library, 24, 123, 3);
    for (unsigned at = 56; at <= 88; at += 32) {
        packed_word(library, at, 0x42435048, 8);
        packed_word(library, at + 8, 0x31444e49, 8);
        packed_word(library, at + 16, JOURNAL, 8);
        packed_word(library, at + 24, ROOT, 8);
    }
    seal_library(library);
    for (unsigned i = 0; i < 16; ++i)
        packed_word(catalog_data, i * 7u, 0x12345u + i * i, 5);
    packed_word(catalog_data, 120, 1, 3);
    packed_word(catalog_data, 123, 123, 3);
    packed_word(catalog_data, 126, 0x40222, 5);
    packed_word(catalog_data, 131, 0x12345 + 11 * 11, 5);
    const struct hpclang_rpl_reader reader = {read_catalog, catalog_data};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 120, 19, 0};
    struct hpclang_rpl_flash_recovery_layout layout = {library, sizeof library, 56, 88};
    const struct hpclang_rpl_flash_installed_recovery sentinel = {1, 2, 3, 4};
    struct hpclang_rpl_flash_installed_recovery output;
    uint8_t workspace[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    /* Initial planning must work before either private record exists. Test
     * separate banks and both contiguous orders, without imposing fixture-only
     * adjacency. Erased bytes inside a tombstone are not free allocations. */
    for (unsigned fault = 0; fault < 13; ++fault) {
        uint32_t j = JOURNAL, a = ROOT;
        if (fault == 1)
            a = j + sizeof journal;
        if (fault == 2) {
            a = j;
            j += sizeof authority;
        }
        if (fault == 3 || fault == 6)
            j += 8;
        if (fault == 4)
            a += 8;
        prepare_native_port(0x7e, 0x7e, a, j);
        memset(flash + j - FIRST, 255, sizeof journal);
        memset(flash + a - FIRST, 255, sizeof authority);
        for (unsigned at = 56; at <= 88; at += 32) {
            packed_word(library, at + 16, j, 8);
            packed_word(library, at + 24, a, 8);
        }
        seal_library(library);
        memcpy(flash + LIBRARY - FIRST, library, sizeof library);
        if (fault == 5)
            flash[a - FIRST] = 0xfe; /* A publication retry is not fresh space. */
        if (fault == 6)
            tombstone(JOURNAL, sizeof journal + 8);
        if (fault == 7) {
            image[0x101] ^= 1; /* Invalid desired bank identity. */
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, j, FIRST, image, journal,
                                                    sizeof journal));
        }
        if (fault == 8) {
            memcpy(image, flash + (LIBRARY & ~4095u) - FIRST, sizeof image);
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, j, LIBRARY & ~4095u, image, journal,
                                                    sizeof journal));
        }
        if (fault == 9)
            layout.admission_binding = layout.recovery_binding;
        if (fault == 10)
            journal[100] ^= 1;
        if (fault == 12)
            packed_word(catalog_data, 126, 0x40223, 5);
        reads = 0;
        fail_read = fault == 11 ? 1 : 0;
        memcpy(initial, flash, sizeof flash);
        output = sentinel;
        int result = hpclang_rpl_flash_plan_installed(&io, &layout, &catalog, journal,
                                                      sizeof journal, &output);
        if (fault < 3) {
            assert(result == 0 && output.library_slot == LIBRARY && output.journal_slot == j &&
                   output.authority_slot == a && output.target == FIRST);
        } else {
            assert(result < 0 && !memcmp(&output, &sentinel, sizeof output));
        }
        assert(!memcmp(initial, flash, sizeof flash) && !programs && !erases);
        layout.admission_binding = 88;
        packed_word(catalog_data, 126, 0x40222, 5);
    }
    for (unsigned geometry = 0; geometry < 6; ++geometry) {
        cancel_journal = JOURNAL;
        cancel_authority = geometry ? JOURNAL + sizeof journal : ROOT;
        if (geometry == 2) {
            cancel_authority = JOURNAL;
            cancel_journal = JOURNAL + sizeof authority;
        }
        if (geometry >= 3) {
            cancel_journal = JOURNAL + 8;
            cancel_authority = ROOT;
        }
        for (unsigned at = 56; at <= 88; at += 32) {
            packed_word(library, at + 16, cancel_journal, 8);
            packed_word(library, at + 24, cancel_authority, 8);
        }
        seal_library(library);
        if (geometry < 3) {
            cancellation_tests(&layout, &catalog);
        } else {
            /* Neither a gap nor an erased interior of a deleted allocation
             * grants ownership. A changed installed root also forbids writes. */
            prepare_native_port(0xff, 0xff, cancel_authority, cancel_journal);
            memset(flash + cancel_journal - FIRST, 255, sizeof journal);
            memset(flash + cancel_authority - FIRST, 255, sizeof authority);
            memcpy(flash + LIBRARY - FIRST, library, sizeof library);
            if (geometry == 4)
                tombstone(JOURNAL, sizeof journal + 8);
            if (geometry == 5) {
                tombstone(JOURNAL, 8);
                flash[LIBRARY - FIRST + 40] ^= 1;
            }
            memcpy(initial, flash, sizeof flash);
            const struct hpclang_rpl_flash_io cancel_io = {flash, read16, cancel_program, FIRST,
                                                           END};
            assert(hpclang_rpl_flash_cancel_installed(&cancel_io, &layout, &catalog) < 0);
            assert(!programs && !memcmp(initial, flash, sizeof flash));
        }
    }
    for (unsigned at = 56; at <= 88; at += 32) {
        packed_word(library, at + 16, JOURNAL, 8);
        packed_word(library, at + 24, ROOT, 8);
    }
    seal_library(library);
    /* A staged binding validates current storage as well as its desired image;
     * neither an overlay hiding damage nor matching interior bytes is enough. */
    for (unsigned fault = 0; fault < 21; ++fault) {
        uint32_t j = JOURNAL + (fault == 16 || fault == 17 ? 8 : 0);
        uint32_t a = fault == 18 ? j + sizeof journal : ROOT;
        for (unsigned at = 56; at <= 88; at += 32) {
            packed_word(library, at + 16, j, 8);
            packed_word(library, at + 24, a, 8);
        }
        seal_library(library);
        layout.admission_binding = 88;
        packed_word(catalog_data, 126, 0x40222, 5);
        prepare_native_port(0x7f, 0x7e, a, j);
        memcpy(flash + LIBRARY - FIRST, library, sizeof library);
        if (fault >= 2 && fault <= 5) {
            const uint8_t wrong[] = {0xff, 0xfe, 0x7e, 0};
            flash[a - FIRST] = wrong[fault - 2];
        }
        if (fault == 6)
            flash[j - FIRST] = 0;
        if (fault == 7)
            flash[j - FIRST + 100] ^= 1;
        if (fault == 8)
            flash[a - FIRST + 17] ^= 1;
        if (fault == 9)
            flash[0x100] = 0;
        if (fault == 1 || fault == 10) {
            image[fault == 1 ? 4 : 0x100] = 0;
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, j, FIRST, image, journal,
                                                    sizeof journal));
            assert(!hpclang_rpl_flash_authority_encode(FIRST, END, a, j, journal, sizeof journal,
                                                       authority, sizeof authority));
            memcpy(flash + j - FIRST, journal, sizeof journal);
            memcpy(flash + a - FIRST, authority, sizeof authority);
            flash[a - FIRST] = 0x7f;
        }
        if (fault == 11)
            packed_word(catalog_data, 126, 0x40223, 5);
        if (fault == 12)
            flash[5 * 0x20000 + 0x101] = 0;
        if (fault == 14)
            flash[LIBRARY - FIRST + 40] ^= 1;
        if (fault == 15)
            layout.admission_binding = 56;
        if (fault == 17)
            tombstone(JOURNAL, sizeof journal + 8);
        if (fault == 20)
            flash[a - FIRST + sizeof authority] = 0;
        reads = 0;
        fail_read = fault == 13 ? 1 : 0;
        memcpy(initial, flash, sizeof flash);
        output = sentinel;
        int result = hpclang_rpl_flash_preflight_staged_installed(
            &io, &layout, &catalog, workspace, fault == 19 ? sizeof journal - 1 : sizeof workspace,
            &output);
        if (fault == 0 || fault == 1 || fault == 18)
            assert(!result && output.library_slot == LIBRARY && output.journal_slot == j &&
                   output.authority_slot == a && output.target == FIRST);
        else
            assert(result < 0 && !memcmp(&output, &sentinel, sizeof output));
        assert(!programs && !erases && !memcmp(flash, initial, sizeof flash));
    }
    layout.admission_binding = 88;
    packed_word(catalog_data, 126, 0x40222, 5);
    for (unsigned at = 56; at <= 88; at += 32) {
        packed_word(library, at + 16, JOURNAL, 8);
        packed_word(library, at + 24, ROOT, 8);
    }
    seal_library(library);
    const uint8_t states[][2] = {{0x7e, 0x7e}, {0x7e, 0x7c}, {0x7c, 0}, {0, 0}};

    const int phases[] = {1, 2, 2, 0};
    for (unsigned i = 0; i < 4; ++i) {
        prepare_native_port(states[i][0], states[i][1], ROOT, JOURNAL);
        memcpy(flash + LIBRARY - FIRST, library, sizeof library);
        if (!i)
            memset(flash, 0xff, sizeof image); /* Damaged target identity. */
        output = sentinel;
        assert(hpclang_rpl_flash_preflight_installed(&io, &layout, &catalog, workspace,
                                                     sizeof workspace, &output) == phases[i]);
        assert(output.library_slot == LIBRARY && output.journal_slot == JOURNAL &&
               output.authority_slot == ROOT && output.target == FIRST);
        assert(!programs && !erases);
    }
    /* A library existing only in the journal cannot be the recovery root. */
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    memcpy(image + 0x105, library, sizeof library);
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, JOURNAL, FIRST, image, journal,
                                            sizeof journal));
    assert(!hpclang_rpl_flash_authority_encode(FIRST, END, ROOT, JOURNAL, journal, sizeof journal,
                                               authority, sizeof authority));
    memcpy(flash + JOURNAL - FIRST, journal, sizeof journal);
    memcpy(flash + ROOT - FIRST, authority, sizeof authority);
    memset(flash, 0xff, sizeof image);
    packed_word(catalog_data, 131, 0x12345 + 8 * 8, 5);
    output = sentinel;
    assert(hpclang_rpl_flash_preflight_installed(&io, &layout, &catalog, workspace,
                                                 sizeof workspace,
                                                 &output) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!memcmp(&output, &sentinel, sizeof output) && !programs && !erases);
    packed_word(catalog_data, 131, 0x12345 + 11 * 11, 5);
    prepare_native_port(0x7e, 0x7e, ROOT, JOURNAL);
    memcpy(flash + LIBRARY - FIRST, library, sizeof library);
    memcpy(initial, flash, sizeof flash);
    for (unsigned fault = 0; fault < 10; ++fault) {
        memcpy(flash, initial, sizeof flash);
        output = sentinel;
        layout.admission_binding = fault == 0 ? 56 : 88;
        if (fault == 7)
            layout.admission_binding = UINT32_MAX;
        layout.recovery_binding = fault == 8 ? 124 : 56;
        layout.bytes = fault == 9 ? 15 : sizeof library;
        if (fault == 1)
            packed_word(library, 104, JOURNAL + 4, 8);
        if (fault == 2)
            flash[LIBRARY - FIRST] = 0;
        if (fault == 3)
            flash[ROOT - FIRST] = 0xff;
        if (fault == 4)
            flash[LIBRARY - FIRST + 40] ^= 1;
        if (fault == 5)
            packed_word(catalog_data, 126, 0x40223, 5);
        reads = 0;
        fail_read = fault == 6 ? 1 : 0;
        assert(hpclang_rpl_flash_preflight_installed(&io, &layout, &catalog, workspace,
                                                     sizeof workspace, &output) < 0);
        assert(!memcmp(&output, &sentinel, sizeof output) && !programs && !erases);
        packed_word(library, 104, JOURNAL, 8);
        packed_word(catalog_data, 126, 0x40222, 5);
    }
}

int main(void)
{
    bound_same_bank_tests();
    bound_membership_tests();
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
    staging_tests();
    activation_tests();
    closure_tests();
    bound_replay_tests();
    transaction_test();
    bound_preflight_tests();
    installed_preflight_tests();
    return 0;
}
