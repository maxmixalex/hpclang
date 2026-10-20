#include "rpl-flash-replay.h"
#include <assert.h>
#include <string.h>

enum
{
    FIRST = 0x140000,
    END = 0x200000,
    SLOT = 0x160105,
    SECTOR = 4096
};
static uint8_t flash[END - FIRST], original[END - FIRST];
static uint8_t desired[SECTOR], scratch[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static unsigned reads, programs, erases, fail_read, fail_program, erase_prefix;
static int erase_error, silent_program;
static uint16_t failed_program_bits, failed_erase_bits;
static int masked_erase;
static int publishing;
static uint32_t sector_address = FIRST, last_program;
static const uint8_t backup[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};

static int read16(void *context, uint32_t address, uint16_t *value)
{
    assert(context == flash && !(address & 1u) && address >= FIRST && address + 1 < END);
    if (++reads == fail_read)
        return 0;
    *value = (uint16_t)(flash[address - FIRST] | (uint16_t)flash[address - FIRST + 1] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    assert(context == flash && !(address & 1u));
    if (publishing) {
        assert(address >= (SLOT & ~1u) && address < SLOT + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        if (address == (SLOT & ~1u))
            assert(!memcmp(flash + SLOT - FIRST + 1, original + SLOT - FIRST + 1,
                           HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 1));
    } else {
        assert(address >= sector_address && address < sector_address + SECTOR);
        if (sector_address == FIRST && address == FIRST + 0x100) {
            /* Identity publication must follow independent body verification,
             * including when an earlier program silently did nothing. */
            assert(!memcmp(flash, desired, 0x100));
            assert(!memcmp(flash + 0x102, desired + 0x102, SECTOR - 0x102));
        }
    }
    ++programs;
    last_program = address;
    if (silent_program)
        return 1;
    uint32_t offset = address - FIRST;
    /* Select completed bit transitions independently of byte boundaries. */
    if (programs == fail_program) {
        uint16_t partial = (uint16_t)(value | (uint16_t)~failed_program_bits);
        flash[offset] &= (uint8_t)partial;
        flash[offset + 1] &= (uint8_t)(partial >> 8u);
        return 0;
    }
    flash[offset] &= (uint8_t)value;
    flash[offset + 1] &= (uint8_t)(value >> 8u);
    return 1;
}

static int erase(void *context, uint32_t address)
{
    assert(context == flash && address == sector_address);
    ++erases;
    if (masked_erase) {
        for (unsigned i = 0; i < SECTOR; i += 2) {
            flash[address - FIRST + i] |= (uint8_t)failed_erase_bits;
            flash[address - FIRST + i + 1] |= (uint8_t)(failed_erase_bits >> 8u);
        }
        return 0;
    }
    memset(flash + address - FIRST, 255, erase_prefix);
    return !erase_error;
}

static const struct hpclang_rpl_flash_replay_io io = {{flash, read16, program16, FIRST, END},
                                                      erase};

static void reset_counters(void)
{
    reads = programs = erases = fail_read = fail_program = 0;
    last_program = 0;
    erase_prefix = SECTOR;
    erase_error = silent_program = 0;
    failed_program_bits = 0x00ff;
    failed_erase_bits = 0;
    masked_erase = 0;
    /* Retry must reconstruct its entire image from durable Flash. */
    memset(scratch, 0xa5, sizeof scratch);
}

static void prepare(void)
{
    memcpy(flash, original, sizeof flash);
    reset_counters();
}

static int replay(void)
{
    int result = hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch);
    /* Journal and all unrelated sectors must survive every failure and retry. */
    assert(!memcmp(flash + SECTOR, original + SECTOR, sizeof flash - SECTOR));
    return result;
}

static void retry(void)
{
    reset_counters();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash, desired, SECTOR));
    reset_counters();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!programs && !erases);
}

static void expected_replay_tests(void)
{
    uint8_t expected[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    memcpy(expected, original + SLOT - FIRST, sizeof expected);
    prepare();
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, expected, sizeof expected, scratch,
                                                    sizeof scratch) == 0);
    assert(!memcmp(flash, desired, SECTOR));
    assert(!memcmp(flash + SECTOR, original + SECTOR, sizeof flash - SECTOR));
    assert(!memcmp(expected, original + SLOT - FIRST, sizeof expected));
    reset_counters();
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, expected, sizeof expected, scratch,
                                                    sizeof scratch) == 0);
    assert(!programs && !erases);
    for (unsigned i = 0; i < sizeof expected; ++i) {
        prepare();
        expected[i] ^= 1;
        assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, expected, sizeof expected,
                                                        scratch,
                                                        sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases && !memcmp(flash, original, sizeof flash));
        expected[i] ^= 1;
    }
    /* A replacement record may be perfectly CRC-valid but is still not the
     * snapshot this caller inspected. Do not silently adopt its new image. */
    uint8_t changed[SECTOR];
    memcpy(changed, desired, sizeof changed);
    changed[10] ^= 1;
    prepare();
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, SLOT, FIRST, changed, flash + SLOT - FIRST,
                                            sizeof expected));
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, expected, sizeof expected, scratch,
                                                    sizeof scratch) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs && !erases && !memcmp(flash, original, SECTOR));
    prepare();
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, NULL, sizeof expected, scratch,
                                                    sizeof scratch) == HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, expected, sizeof expected - 1,
                                                    scratch,
                                                    sizeof scratch) == HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, scratch, sizeof scratch, scratch,
                                                    sizeof scratch) == HPCLANG_RPL_FLASH_INVALID);
    uint8_t buffers[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    assert(hpclang_rpl_flash_sector_replay_expected(&io, SLOT, buffers, sizeof expected,
                                                    buffers + sizeof expected - 1,
                                                    sizeof scratch) == HPCLANG_RPL_FLASH_INVALID);
    assert(!reads && !programs && !erases);
    memcpy(buffers, expected, sizeof expected);
    assert(!hpclang_rpl_flash_sector_replay_expected(&io, SLOT, buffers, sizeof expected,
                                                     buffers + sizeof expected, sizeof scratch));
    assert(!memcmp(buffers, expected, sizeof expected));
    assert(!memcmp(flash, desired, SECTOR));
}

static void prepare_publication(void)
{
    prepare();
    publishing = 1;
    memset(flash + SLOT - FIRST, 255, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES + 1);
}

static void check_publication(void)
{
    const unsigned offset = SLOT - FIRST, bytes = HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
    assert(!erases);
    assert(!memcmp(flash, original, offset));
    assert(flash[offset + bytes] == 255);
    assert(!memcmp(flash + offset + bytes + 1, original + offset + bytes + 1,
                   sizeof flash - offset - bytes - 1));
    if (flash[offset] == 0x7e)
        assert(!memcmp(flash + offset, original + offset, bytes));
    else
        assert(flash[offset] == 0xff || flash[offset] == 0xfe || flash[offset] == 0x7f);
}

static int publish(void)
{
    int result = hpclang_rpl_flash_publish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                                  HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
    check_publication();
    return result;
}

static int finish_publication(void)
{
    int result = hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                                 HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
    check_publication();
    return result;
}

static void retry_publication(void)
{
    reset_counters();
    assert(finish_publication() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash + SLOT - FIRST, original + SLOT - FIRST,
                   HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES));
    reset_counters();
    assert(finish_publication() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!programs && !erases);
}

static void publication_tests(void)
{
    prepare_publication();
    assert(publish() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned total_reads = reads, total_programs = programs;
    assert(total_programs > 0);
    publishing = 0;
    reset_counters();
    assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash, desired, SECTOR));
    for (unsigned cut = 1; cut <= total_reads; ++cut) {
        prepare_publication();
        fail_read = cut;
        int result = publish();
        assert(result == HPCLANG_RPL_FLASH_FAILED || result == HPCLANG_RPL_FLASH_DIRTY);
        if (result == HPCLANG_RPL_FLASH_FAILED)
            assert(!programs);
        retry_publication();
    }
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xffff};
    for (unsigned cut = 1; cut <= total_programs; ++cut) {
        for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
            prepare_publication();
            fail_program = cut;
            failed_program_bits = masks[mask];
            assert(publish() == HPCLANG_RPL_FLASH_DIRTY);
            retry_publication();
        }
    }
    /* Occupation anywhere, including the next end marker, forbids all writes. */
    for (unsigned i = 0; i <= HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        prepare_publication();
        flash[SLOT - FIRST + i] = 0;
        assert(hpclang_rpl_flash_publish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                                HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_OCCUPIED);
        assert(!programs && !erases);
    }
    prepare_publication();
    silent_program = 1;
    assert(publish() == HPCLANG_RPL_FLASH_DIRTY);
    assert(flash[SLOT - FIRST] == 255);
    retry_publication();
    publishing = 0;
}

static void partial_publication(void)
{
    prepare_publication();
    fail_program = 3;
    failed_program_bits = 0x5555;
    assert(publish() == HPCLANG_RPL_FLASH_DIRTY && flash[SLOT - FIRST] == 255);
    reset_counters();
}

static void publication_resumption_tests(void)
{
    partial_publication();
    assert(finish_publication() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned total_reads = reads, total_programs = programs;
    assert(total_programs);
    for (unsigned cut = 1; cut <= total_reads; ++cut) {
        partial_publication();
        fail_read = cut;
        assert(finish_publication() == HPCLANG_RPL_FLASH_DIRTY);
        retry_publication();
    }
    for (unsigned cut = 1; cut <= total_programs; ++cut) {
        partial_publication();
        fail_program = cut;
        failed_program_bits = cut & 1u ? 0x5555 : 0xffff;
        assert(finish_publication() == HPCLANG_RPL_FLASH_DIRTY);
        retry_publication();
    }
    for (unsigned status = 0; status < 256; ++status) {
        prepare_publication();
        memcpy(flash + SLOT - FIRST, original + SLOT - FIRST,
               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        flash[SLOT - FIRST] = (uint8_t)status;
        int result = hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                                     HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        if (status == 0xff || status == 0x7f || status == 0xfe || status == 0x7e) {
            assert(result == 0 && flash[SLOT - FIRST] == 0x7e);
            check_publication();
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs && !erases);
        }
    }
    const uint8_t valid_statuses[] = {0xff, 0x7f, 0xfe, 0x7e};
    for (unsigned i = 0; i < sizeof valid_statuses; ++i) {
        prepare_publication();
        memcpy(flash + SLOT - FIRST, original + SLOT - FIRST,
               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        flash[SLOT - FIRST] = valid_statuses[i];
        flash[SLOT - FIRST + 1] &= (uint8_t)~2u; /* Cannot restore this required bit. */
        assert(hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases);
    }
    for (unsigned i = 1; i < sizeof valid_statuses; ++i) {
        partial_publication();
        flash[SLOT - FIRST] = valid_statuses[i];
        assert(hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases);
    }
    prepare_publication();
    assert(hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                           HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 1) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(!reads && !programs && !erases);
    prepare_publication();
    flash[SLOT - FIRST + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES] = 0x7e;
    assert(hpclang_rpl_flash_finish_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                           HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs && !erases);
    partial_publication();
    silent_program = 1;
    assert(finish_publication() == HPCLANG_RPL_FLASH_DIRTY);
    retry_publication();
    publishing = 0;
}

static void prepare_retirement(void)
{
    prepare();
    publishing = 1;
    memcpy(flash, desired, SECTOR);
}

static int retire(void)
{
    int result = hpclang_rpl_flash_retire_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                                 HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
    assert(!erases && !memcmp(flash, desired, SECTOR));
    assert(!memcmp(flash + SECTOR, original + SECTOR, SLOT - FIRST - SECTOR));
    assert(!memcmp(flash + SLOT - FIRST + 1, original + SLOT - FIRST + 1,
                   sizeof flash - (SLOT - FIRST + 1)));
    return result;
}

static void retirement_tests(void)
{
    prepare_retirement();
    assert(retire() == HPCLANG_RPL_FLASH_WRITTEN);
    assert(!flash[SLOT - FIRST] && programs == 1);
    unsigned total_reads = reads;
    reset_counters();
    assert(retire() == HPCLANG_RPL_FLASH_WRITTEN && !programs);
    for (unsigned cut = 1; cut <= total_reads; ++cut) {
        prepare_retirement();
        fail_read = cut;
        assert(retire() == HPCLANG_RPL_FLASH_DIRTY);
        reset_counters();
        assert(retire() == HPCLANG_RPL_FLASH_WRITTEN);
    }
    prepare_retirement();
    fail_program = 1;
    assert(retire() == HPCLANG_RPL_FLASH_DIRTY);
    reset_counters();
    assert(retire() == HPCLANG_RPL_FLASH_WRITTEN);
    /* Every possible partial retirement status, not just a byte-boundary cut. */
    for (unsigned status = 0; status < 256; ++status) {
        prepare_retirement();
        flash[SLOT - FIRST] = (uint8_t)status;
        int result = retire();
        if ((status & 0x7e) == status) {
            assert(result == HPCLANG_RPL_FLASH_WRITTEN && !flash[SLOT - FIRST]);
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
        }
    }
    for (unsigned i = 1; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        prepare_retirement();
        flash[SLOT - FIRST + i] ^= 1;
        assert(hpclang_rpl_flash_retire_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases && flash[SLOT - FIRST] == 0x7e);
    }
    for (unsigned i = 0; i < SECTOR; ++i) {
        prepare_retirement();
        flash[i] ^= 1;
        assert(hpclang_rpl_flash_retire_sector(&io.flash, SLOT, original + SLOT - FIRST,
                                               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases && flash[SLOT - FIRST] == 0x7e);
    }
    prepare_retirement();
    silent_program = 1;
    assert(retire() == HPCLANG_RPL_FLASH_DIRTY);
    publishing = 0;
}

static void prepare_discovery(void)
{
    reset_counters();
    memset(flash, 255, sizeof flash);
    flash[0x20100] = 0xf0;
    flash[0x20101] = 9;
}

static void discovery_image(uint32_t address)
{
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, address, FIRST, desired,
                                            flash + address - FIRST,
                                            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES));
}

static void discovery_tests(void)
{
    uint32_t slot = 17, target = 19;
    prepare_discovery();
    assert(!hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                          &target));
    assert(slot == 17 && target == 19);
    discovery_image(SLOT);
    /* The target bank need not be readable as an allocation chain yet. */
    memset(flash, 0, SECTOR);
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == 1);
    assert(slot == SLOT && target == FIRST && !programs && !erases);
    slot = 17;
    target = 19;
    flash[0x20101] = 8;
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_DIRTY);
    flash[0x20101] = 9;
    flash[0x3ffff] = 0;
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == 17 && target == 19);
    flash[0x3ffff] = 255;
    discovery_image(SLOT + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == 17 && target == 19);

    /* A valid self-addressed capsule inside a deleted object's payload is not
     * a transaction. Discovery must follow the outer allocation's extent. */
    prepare_discovery();
    discovery_image(SLOT + 4);
    unsigned extent = (HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES + 8) * 2 - 2;
    flash[SLOT - FIRST] = 0;
    flash[SLOT - FIRST + 1] = (uint8_t)extent;
    flash[SLOT - FIRST + 2] = (uint8_t)(extent >> 8);
    flash[SLOT - FIRST + 3] = (uint8_t)(extent >> 16);
    assert(!hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                          &target));
    assert(slot == 17 && target == 19);
    prepare_discovery();
    discovery_image(SLOT);
    flash[SLOT - FIRST + 25] ^= 1;
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_DIRTY);
    discovery_image(SLOT);
    reset_counters();
    fail_read = 10;
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_FAILED);
    assert(slot == 17 && target == 19 && !programs && !erases);
    prepare_discovery();
    memcpy(flash + SLOT - FIRST, backup, sizeof backup);
    discovery_image(SLOT + sizeof backup);
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == 1);
    assert(slot == SLOT + sizeof backup && target == FIRST);
    slot = 17;
    target = 19;
    flash[SLOT - FIRST + 29] ^= 1; /* Bad live-object CRC before journal. */
    assert(hpclang_rpl_flash_find_sector(&io.flash, 0x160000, scratch, sizeof scratch, &slot,
                                         &target) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == 17 && target == 19);
}

static void prepare_preflight(void)
{
    prepare_discovery();
    for (unsigned i = 0; i < 6; ++i) {
        flash[i * 0x20000 + 0x100] = 0xf0;
        flash[i * 0x20000 + 0x101] = (uint8_t)(8 + i);
    }
    memcpy(desired, flash, SECTOR);
}

static void preflight_tests(void)
{
    uint8_t workspace[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint32_t slot = 17, target = 19;
    prepare_preflight();
    assert(!hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                               &target));
    assert(slot == 17 && target == 19);
    memset(flash, 0, SECTOR);
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    discovery_image(SLOT);
    memcpy(original, flash, sizeof flash);
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == 1);
    assert(slot == SLOT && target == FIRST && !programs && !erases);
    assert(!memcmp(original, flash, sizeof flash));
    /* The discovered candidate can now be replayed and revalidated using the
     * same durable record, while the caller retains ownership. */
    assert(hpclang_rpl_flash_sector_replay(&io, slot, scratch, sizeof scratch) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash, desired, SECTOR));
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == 1);

    publishing = 1;
    assert(hpclang_rpl_flash_retire_sector(&io.flash, slot, workspace,
                                           HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    publishing = 0;
    assert(!hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                               &target));

    prepare_preflight();
    desired[0x100] = 0; /* CRC-valid image with wrong bank identity. */
    discovery_image(SLOT);
    slot = 17;
    target = 19;
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == 17 && target == 19 && !programs && !erases);
    prepare_preflight();
    desired[0x105] = 0x53; /* CRC-valid capsule containing an invalid live object. */
    discovery_image(SLOT);
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_preflight();
    discovery_image(SLOT);
    flash[SECTOR + 3] = 0; /* Damage in the target bank outside the image. */
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    flash[SECTOR + 3] = 255;
    flash[0xa0100] = 0; /* An unrelated bank must also remain valid. */
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    prepare_preflight();
    discovery_image(SLOT);
    discovery_image(0x180105); /* Conflicting journals in different banks. */
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == 17 && target == 19 && !programs && !erases);
    prepare_preflight();
    discovery_image(SLOT);
    fail_read = 1;
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_FAILED);
    assert(slot == 17 && target == 19 && !programs && !erases);
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace - 1, &slot,
                                              &target) == HPCLANG_RPL_FLASH_INVALID);
}

static void stale_journal_tests(void)
{
    uint8_t workspace[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint32_t slot = 17, target = 19;
    prepare_preflight();
    discovery_image(SLOT);
    /* A later, valid allocation can fit entirely inside the journal's target.
     * Overlay validation sees the old empty tail, hiding the later object.
     * Neither a valid journal CRC nor exact journal-byte pinning proves that
     * replay is still authorized. Do not feed this result to the raw replayer. */
    memcpy(flash + 0x105, backup, sizeof backup);
    memcpy(original, flash, sizeof flash);
    assert(
        !hpclang_rpl_flash_find_sector(&io.flash, FIRST, scratch, sizeof scratch, &slot, &target));
    assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                              &target) == HPCLANG_RPL_FLASH_SECTOR_READY);
    assert(slot == SLOT && target == FIRST);
    assert(memcmp(flash, workspace + 24, SECTOR));
    assert(!programs && !erases && !memcmp(flash, original, sizeof flash));

    /* Public write preparation must not treat structural preflight as erase
     * authority, nor clean up an unowned sector journal as a failed append. */
    const uint8_t pending[] = {0x7e, 0x7c, 0x02};
    for (unsigned i = 0; i < sizeof pending; ++i) {
        flash[SLOT - FIRST] = pending[i];
        memcpy(original, flash, sizeof flash);
        reset_counters();
        assert(hpclang_rpl_flash_prepare_write(&io.flash) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !erases && !memcmp(flash, original, sizeof flash));
    }
}

static void restart_retirement_tests(void)
{
    uint8_t workspace[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    const uint8_t statuses[] = {0x02, 0x40, 0x7c};
    for (unsigned i = 0; i < sizeof statuses; ++i) {
        prepare_preflight();
        discovery_image(SLOT);
        memcpy(original, flash, sizeof flash);
        flash[SLOT - FIRST] = statuses[i];
        memset(workspace, 0xa5, sizeof workspace);
        uint32_t slot = 17, target = 19;
        assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                                  &target) == HPCLANG_RPL_FLASH_SECTOR_RETIRING);
        assert(slot == SLOT && target == FIRST && workspace[0] == 0x7e);
        assert(flash[SLOT - FIRST] == statuses[i] && !programs && !erases);
        /* A retirement cut never permits erasing/replaying the target. */
        assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!programs && !erases);
        flash[0] ^= 1;
        slot = 17;
        target = 19;
        assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                                  &target) == HPCLANG_RPL_FLASH_DIRTY);
        assert(slot == 17 && target == 19 && !programs && !erases);
        flash[0] ^= 1;
        memset(workspace, 0, sizeof workspace);
        assert(hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                                  &target) == HPCLANG_RPL_FLASH_SECTOR_RETIRING);
        publishing = 1;
        assert(hpclang_rpl_flash_retire_sector(&io.flash, slot, workspace,
                                               HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ==
               HPCLANG_RPL_FLASH_WRITTEN);
        publishing = 0;
        assert(!hpclang_rpl_flash_preflight_sector(&io.flash, workspace, sizeof workspace, &slot,
                                                   &target));
    }
}

static void non_header_sector_test(void)
{
    prepare();
    sector_address = FIRST + SECTOR;
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, SLOT, sector_address, desired,
                                            flash + SLOT - FIRST,
                                            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES));
    uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    memcpy(journal, flash + SLOT - FIRST, sizeof journal);
    assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!memcmp(flash + SECTOR, desired, SECTOR));
    assert(!memcmp(flash, original, SECTOR));
    assert(!memcmp(flash + SLOT - FIRST, journal, sizeof journal));
    assert(last_program == sector_address + SECTOR - 2);
    reset_counters();
    assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
           HPCLANG_RPL_FLASH_WRITTEN);
    assert(!erases && !programs);
    sector_address = FIRST;
}

int main(void)
{
    for (unsigned i = 0; i < sizeof original; ++i)
        original[i] = (uint8_t)(i * 31u + 11u);
    for (unsigned i = 0; i < SECTOR; ++i)
        desired[i] = (uint8_t)(i * 73u);
    /* Include erased words: these must not need programming. */
    memset(desired + 128, 255, 128);
    assert(!hpclang_rpl_flash_sector_encode(FIRST, END, SLOT, FIRST, desired,
                                            original + SLOT - FIRST,
                                            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES));
    prepare();
    assert(replay() == HPCLANG_RPL_FLASH_WRITTEN);
    unsigned total_reads = reads, total_programs = programs;
    assert(erases == 1 && total_programs == (SECTOR - 128) / 2);
    assert(last_program == FIRST + 0x100);
    retry();

    /* Cut every read, including initial journal reads, post-erase blank checks
     * and final verification. Resume with no surviving scratch state. */
    for (unsigned cut = 1; cut <= total_reads; ++cut) {
        prepare();
        fail_read = cut;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        retry();
    }
    for (unsigned cut = 1; cut <= total_programs; ++cut) {
        prepare();
        fail_program = cut;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        retry();
    }
    for (unsigned cut = 0; cut <= SECTOR; ++cut) {
        prepare();
        erase_prefix = cut;
        erase_error = 1;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs);
        retry();
    }
    /* Single completed/missing bits plus interleaved masks. Exercise early
     * body writes, a middle write and the final identity publication. */
    const unsigned positions[] = {1, total_programs / 2, total_programs};
    for (unsigned pattern = 0; pattern < 36; ++pattern) {
        uint16_t mask = pattern < 16    ? (uint16_t)(1u << pattern)
                        : pattern < 32  ? (uint16_t)~(1u << (pattern - 16))
                        : pattern == 32 ? 0
                        : pattern == 33 ? UINT16_MAX
                        : pattern == 34 ? 0x5555
                                        : 0xaaaa;
        for (unsigned i = 0; i < sizeof positions / sizeof positions[0]; ++i) {
            prepare();
            fail_program = positions[i];
            failed_program_bits = mask;
            assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
            retry();
        }
        prepare();
        masked_erase = 1;
        failed_erase_bits = mask;
        assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs);
        retry();
    }
    prepare();
    erase_prefix = SECTOR - 1;
    assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
    assert(!programs);
    retry();
    prepare();
    silent_program = 1;
    assert(replay() == HPCLANG_RPL_FLASH_DIRTY);
    retry();

    /* No invalid or unpublished journal authorizes an erase. */
    for (unsigned i = 0; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        prepare();
        flash[SLOT - FIRST + i] ^= 1;
        assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!erases && !programs && !memcmp(flash, original, SECTOR));
    }
    for (unsigned status = 0; status < 256; ++status) {
        if (status == 0x7e)
            continue;
        prepare();
        flash[SLOT - FIRST] = (uint8_t)status;
        assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(!erases && !programs);
    }
    prepare();
    assert(hpclang_rpl_flash_sector_replay(&io, SLOT, scratch, sizeof scratch - 1) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(!reads && !programs && !erases);
    expected_replay_tests();
    non_header_sector_test();
    publication_tests();
    publication_resumption_tests();
    retirement_tests();
    discovery_tests();
    preflight_tests();
    stale_journal_tests();
    restart_retirement_tests();
    return 0;
}
