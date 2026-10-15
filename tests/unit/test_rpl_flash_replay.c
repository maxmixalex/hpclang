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
static int publishing;

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
        assert(address >= FIRST && address < FIRST + SECTOR);
    }
    ++programs;
    if (silent_program)
        return 1;
    uint32_t offset = address - FIRST;
    /* A failed pulse may clear only some of the intended bits. */
    flash[offset] &= (uint8_t)value;
    if (programs == fail_program)
        return 0;
    flash[offset + 1] &= (uint8_t)(value >> 8u);
    return 1;
}

static int erase(void *context, uint32_t address)
{
    assert(context == flash && address == FIRST);
    ++erases;
    memset(flash, 255, erase_prefix);
    return !erase_error;
}

static const struct hpclang_rpl_flash_replay_io io = {{flash, read16, program16, FIRST, END},
                                                      erase};

static void reset_counters(void)
{
    reads = programs = erases = fail_read = fail_program = 0;
    erase_prefix = SECTOR;
    erase_error = silent_program = 0;
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
    }
    for (unsigned cut = 1; cut <= total_programs; ++cut) {
        prepare_publication();
        fail_program = cut;
        assert(publish() == HPCLANG_RPL_FLASH_DIRTY);
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
    const uint8_t backup[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                              0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                              0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
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
    publication_tests();
    retirement_tests();
    discovery_tests();
    preflight_tests();
    return 0;
}
