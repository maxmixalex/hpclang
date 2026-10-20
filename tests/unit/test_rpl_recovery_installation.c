#include "rpl-recovery-installation.h"
#include <assert.h>
#include <string.h>
#define HPCLANG_RECOVERY_BINDING_VECTORS
#include "recovery-identity-vectors.h"

enum
{
    FIRST = 0x140000,
    END = 0x200000
};
static uint8_t flash[END - FIRST], before[END - FIRST], catalog_data[160];
static uint32_t fail_address;
static const uint8_t *fail_after_staging;
static unsigned programs, fail_program;
static uint16_t partial_program_mask;
static int program_flash(void *context, uint32_t at, uint16_t value)
{
    assert(context == flash && !(at & 1) && at >= FIRST && at + 1 < END);
    unsigned offset = at - FIRST;
    uint16_t old = flash[offset] | (uint16_t)flash[offset + 1] << 8;
    assert((old & value) == value);
    if (++programs == fail_program) {
        uint16_t partial = old & (value | (uint16_t)~partial_program_mask);
        flash[offset] = partial;
        flash[offset + 1] = partial >> 8;
        return 0;
    }
    flash[offset] = value;
    flash[offset + 1] = value >> 8;
    return 1;
}
static int read_flash(void *context, uint32_t at, uint16_t *value)
{
    assert(context == flash && !(at & 1) && at >= FIRST && at + 1 < END);
    if (at == fail_address || (fail_after_staging && *fail_after_staging != 0xa5))
        return 0;
    *value = flash[at - FIRST] | (uint16_t)flash[at - FIRST + 1] << 8;
    return 1;
}
static int read_catalog(void *context, uint32_t at, uint8_t *value)
{
    assert(context == catalog_data);
    if (at >= sizeof catalog_data)
        return 0;
    *value = catalog_data[at];
    return 1;
}
static void word(unsigned at, unsigned value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        catalog_data[at + i] = (value >> (4 * i)) & 15;
}
static void install(unsigned v, unsigned bank)
{
    memset(flash, 255, sizeof flash);
    fail_address = UINT32_MAX;
    for (unsigned i = 0; i < 6; ++i) {
        flash[i * 0x20000 + 0x100] = 0xf0;
        flash[i * 0x20000 + 0x101] = i + 8;
    }
    unsigned slot = bank * 0x20000 + 0x105;
    unsigned bytes = ((vectors[v].span + 15) & ~7u) / 2;
    flash[slot] = 0x53;
    unsigned extent = 2 * bytes - 2;
    flash[slot + 1] = extent;
    flash[slot + 2] = extent >> 8;
    flash[slot + 3] = 0x10 | (extent >> 16);
    memcpy(flash + slot + 4, vectors[v].data, (vectors[v].span + 1) / 2);
    memset(catalog_data, 0, sizeof catalog_data);
    for (unsigned i = 0; i < 16; ++i)
        word(i * 7, 0x12345 + i * i, 5);
    unsigned name_length = vectors[v].data[5];
    word(120, 1, 3);
    word(123, 1701, 3);
    word(126, 0x40000 + 2 * (0x105 + 4) + 14 + 2 * name_length, 5);
    word(131, 0x12345 + (bank + 8) * (bank + 8), 5);
}
static void transaction_tests(const struct hpclang_rpl_flash_io *io,
                              const struct hpclang_rpl_flash_catalog *catalog)
{
    enum
    {
        JOURNAL = 0x160105,
        AUTHORITY = 0x180105
    };
    uint8_t image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
    uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    uint8_t scratch[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    const struct hpclang_rpl_flash_installed_recovery sentinel = {1, 2, 3, 4};
    for (unsigned scenario = 0; scenario < 14; ++scenario) {
        unsigned bank = scenario == 5 ? 0 : 3;
        install(0, bank);
        memcpy(image, flash, sizeof image);
        assert(!hpclang_rpl_flash_sector_encode(FIRST, END, JOURNAL, FIRST, image, journal,
                                                sizeof journal));
        assert(!hpclang_rpl_flash_authority_encode(FIRST, END, AUTHORITY, JOURNAL, journal,
                                                   sizeof journal, authority, sizeof authority));
        if (scenario == 1)
            journal[0] = 0x7c;
        if (scenario == 2 || scenario == 7 || scenario == 9)
            journal[0] = authority[0] = 0;
        if (scenario == 3)
            authority[0] = 0xff;
        if (scenario == 10)
            journal[0] = 0x7c;
        if (scenario == 11) {
            journal[0] = 0;
            authority[0] = 0x7c;
        }
        if (scenario == 12)
            authority[0] = 0;
        memcpy(flash + JOURNAL - FIRST, journal, sizeof journal);
        memcpy(flash + AUTHORITY - FIRST, authority, sizeof authority);
        if (scenario == 0 || scenario == 5)
            memset(flash, 255, sizeof image);
        if (scenario == 4)
            flash[AUTHORITY - FIRST + 12] ^= 1;
        if (scenario == 6)
            word(131, 0x12345 + 64, 5);
        if (scenario == 7)
            flash[0x101] ^= 1; /* Closed transactions cannot overlay old valid data. */
        if (scenario == 8)
            fail_address = END - 2;
        if (scenario == 10)
            flash[123] ^= 1;
        if (scenario == 13) {
            uint8_t other[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, 0x1e0105, FIRST, image, other,
                                                    sizeof other));
            memcpy(flash + 0x1e0105 - FIRST, other, sizeof other);
        }
        if (scenario == 9) {
            /* A later valid allocation must survive a closed transaction. */
            flash[0x105] = 0;
            flash[0x106] = 14;
            flash[0x107] = flash[0x108] = 0;
        }
        memcpy(before, flash, sizeof flash);
        struct hpclang_rpl_flash_installed_recovery result = sentinel;
        int phase = hpclang_rpl_recovery_installation_preflight(
            io, 1701, catalog, vectors[0].descriptor, AUTHORITY, JOURNAL, scratch, sizeof scratch,
            &result);
        struct hpclang_rpl_flash_installed_recovery streamed = sentinel;
        int streamed_phase = hpclang_rpl_recovery_installation_stream(
            io, 1701, catalog, vectors[0].descriptor, AUTHORITY, JOURNAL, &streamed);
        if (scenario < 3 || scenario == 9 || scenario == 11) {
            const int expected[] = {1, 2, 0};
            assert(phase == (scenario == 11 ? 2 : scenario == 9 ? 0 : expected[scenario]));
            assert(streamed_phase == phase && !memcmp(&streamed, &result, sizeof result));
            assert(result.library_slot == FIRST + bank * 0x20000 + 0x105);
            assert(result.journal_slot == JOURNAL && result.authority_slot == AUTHORITY &&
                   result.target == FIRST);
        } else {
            assert(phase < 0 && !memcmp(&result, &sentinel, sizeof result));
            assert(streamed_phase < 0 && !memcmp(&streamed, &sentinel, sizeof streamed));
        }
        assert(!memcmp(before, flash, sizeof flash));
    }
}

struct ram_port
{
    uint8_t data[4096];
    uint32_t fail;
};

static int read_port(void *context, uint32_t at, uint8_t *value)
{
    const struct ram_port *port = context;
    if (at >= sizeof port->data || at == port->fail)
        return 0;
    *value = port->data[at];
    return 1;
}

static void placement_tests(const struct hpclang_rpl_flash_io *io,
                            const struct hpclang_rpl_flash_catalog *catalog)
{
    static struct ram_port ram[2], original[2];
    uint8_t record[4096], old_catalog[sizeof catalog_data];
    unsigned bytes = ((vectors[0].span + 15) & ~7u) / 2;
    assert(bytes <= sizeof record && vectors[0].span + 5 < sizeof ram[0].data);
    install(0, 0);
    memcpy(record, flash + 0x105, bytes);
    if (vectors[0].span & 1u)
        record[4 + vectors[0].span / 2] |= 0xf0;
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, bytes);
        word(120, 0, 3);
        memset(ram, 0, sizeof ram);
        ram[0].fail = ram[1].fail = UINT32_MAX;
        struct hpclang_rpl_reader readers[2] = {{read_port, &ram[0]}, {read_port, &ram[1]}};
        struct hpclang_rpl_recovery_port_view ports[2] = {{&readers[0], 0, sizeof ram[0].data},
                                                          {&readers[1], 0, sizeof ram[1].data}};
        if (scenario == 1 || scenario == 2 || scenario == 7) {
            unsigned port = scenario == 2;
            for (unsigned i = 0; i < vectors[0].span; ++i)
                ram[port].data[i] = (vectors[0].data[i / 2] >> (4 * (i & 1))) & 15;
            if (scenario == 7)
                ram[port].data[vectors[0].span] = 1; /* Bad record after a match. */
        }
        if (scenario == 3) {
            word(120, 1, 3);
            word(123, 1701, 3);
        }
        if (scenario == 4 || scenario == 5)
            ram[scenario - 4].fail = 2;
        if (scenario == 6) {
            word(120, 2, 3);
            word(123, 1701, 3);
            word(139, 1701, 3); /* Duplicate native catalog IDs are malformed. */
        }
        if (scenario == 8)
            ports[0].end = 4; /* Missing complete terminator. */
        if (scenario == 9)
            ports[1].reader = NULL;
        if (scenario == 10)
            memcpy(flash + 0x105, record, bytes);
        if (scenario == 11)
            fail_address = END - 2;
        memcpy(before, flash, sizeof flash);
        memcpy(original, ram, sizeof ram);
        memcpy(old_catalog, catalog_data, sizeof catalog_data);
        uint32_t slot = UINT32_MAX;
        int result =
            hpclang_rpl_recovery_installation_plan(io, record, bytes, ports, catalog, &slot);
        const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        int planned = hpclang_rpl_recovery_installation_plan_space(io, record, bytes, ports,
                                                                   catalog, FIRST, &space);
        if (!scenario) {
            assert(!result && slot == FIRST + 0x105);
            assert(!planned && space.library_slot == FIRST + 0x20105 &&
                   space.journal_slot == space.library_slot + bytes &&
                   space.authority_slot ==
                       space.journal_slot + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        } else {
            int expected = scenario <= 3 || scenario == 10 ? HPCLANG_RPL_FLASH_EXISTS
                           : scenario == 4 || scenario == 5 || scenario == 11
                               ? HPCLANG_RPL_FLASH_FAILED
                           : scenario == 9 ? HPCLANG_RPL_FLASH_INVALID
                                           : HPCLANG_RPL_FLASH_DIRTY;
            assert(result == expected && slot == UINT32_MAX);
            assert(planned == expected && !memcmp(&space, &sentinel, sizeof space));
        }
        assert(!memcmp(before, flash, sizeof flash));
        assert(!memcmp(original, ram, sizeof ram));
        assert(!memcmp(old_catalog, catalog_data, sizeof catalog_data));
    }
}

static void tail_space(unsigned bank, unsigned bytes)
{
    unsigned start = bank * 0x20000u + 0x105u;
    unsigned tail = (bank + 1u) * 0x20000u - bytes - 3u;
    unsigned extent = 2u * (tail - start) - 2u;
    flash[start] = 0;
    flash[start + 1] = extent;
    flash[start + 2] = extent >> 8;
    flash[start + 3] = 0xf0 | (extent >> 16);
}

static void space_tests(const struct hpclang_rpl_flash_io *io)
{
    uint8_t record[4096], journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES], image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
    unsigned bytes = ((vectors[0].span + 15) & ~7u) / 2;
    install(0, 0);
    memcpy(record, flash + 0x105, bytes);
    if (vectors[0].span & 1u)
        record[4 + vectors[0].span / 2] |= 0xf0;
    const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
    for (unsigned scenario = 0; scenario < 11; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, bytes);
        for (unsigned bank = 0; bank < 6; ++bank)
            tail_space(bank, 0);
        tail_space(0, bytes + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        tail_space(1, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        uint32_t target = END - HPCLANG_RPL_FLASH_SECTOR_BYTES;
        if (scenario == 1)
            tail_space(1, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4);
        if (scenario == 2)
            target = FIRST + 0x3f000; /* The only journal tail intersects target. */
        if (scenario == 3)
            target++;
        if (scenario == 4)
            fail_address = END - 2;
        if (scenario == 5)
            flash[END - FIRST - 1] = 0;
        if (scenario == 6)
            target = FIRST - HPCLANG_RPL_FLASH_SECTOR_BYTES;
        if (scenario == 7)
            target = END;
        if (scenario == 8)
            tail_space(0, bytes); /* Library fits, authority does not. */
        if (scenario == 10) {
            tail_space(0, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
            tail_space(1, bytes + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        }
        memcpy(before, flash, sizeof flash);
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        int result = hpclang_rpl_flash_plan_recovery_space(io, record, bytes, target,
                                                           scenario == 9 ? NULL : &space);
        if (!scenario || scenario == 10) {
            assert(!result);
            unsigned library_end = FIRST + (scenario ? 0x40000 : 0x20000);
            unsigned journal_end = FIRST + (scenario ? 0x20000 : 0x40000);
            assert(space.library_slot == library_end - bytes - sizeof authority - 3);
            assert(space.journal_slot == journal_end - sizeof journal - 3);
            assert(space.authority_slot == space.library_slot + bytes);
            memcpy(image, flash + target - FIRST, sizeof image);
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, space.journal_slot, target, image,
                                                    journal, sizeof journal));
            assert(!hpclang_rpl_flash_authority_encode(FIRST, END, space.authority_slot,
                                                       space.journal_slot, journal, sizeof journal,
                                                       authority, sizeof authority));
        } else {
            int expected = scenario == 4   ? HPCLANG_RPL_FLASH_FAILED
                           : scenario == 5 ? HPCLANG_RPL_FLASH_DIRTY
                           : scenario == 3 || (scenario >= 6 && scenario != 8)
                               ? HPCLANG_RPL_FLASH_INVALID
                               : HPCLANG_RPL_FLASH_NOSPACE;
            assert(result == expected && !memcmp(&space, &sentinel, sizeof space));
        }
        assert(!memcmp(before, flash, sizeof flash));
    }
}

static void preparation_tests(const struct hpclang_rpl_flash_io *io,
                              const struct hpclang_rpl_flash_catalog *catalog)
{
    uint8_t original[sizeof binding_original], saved[sizeof original];
    uint8_t staging[sizeof original + 16], old_catalog[sizeof catalog_data];
    struct ram_port ram = {.fail = UINT32_MAX};
    const struct hpclang_rpl_reader empty = {read_port, &ram};
    const struct hpclang_rpl_recovery_port_view ports[2] = {{&empty, 0, sizeof ram.data},
                                                            {&empty, 0, sizeof ram.data}};
    const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
    for (unsigned scenario = 0; scenario < 11; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, ((vectors[0].span + 15) & ~7u) / 2);
        word(120, 0, 3);
        memcpy(original, binding_original, sizeof original);
        memset(staging, 0xa5, sizeof staging);
        struct hpclang_rpl_flash_recovery_layout layout = {
            original, sizeof original, recovery_binding_nibble, admission_binding_nibble};
        uint32_t identity = identity_object_nibble, capacity = sizeof staging;
        if (scenario == 1)
            identity = UINT32_MAX;
        if (scenario == 2)
            layout.recovery_binding = UINT32_MAX;
        if (scenario == 3)
            layout.admission_binding = layout.recovery_binding;
        if (scenario == 4)
            layout.admission_binding = 8 + identity;
        if (scenario == 5)
            original[30] ^= 1;
        if (scenario == 6)
            capacity = sizeof original - 1;
        if (scenario == 7)
            fail_address = END - 2;
        if (scenario == 8)
            fail_after_staging = staging;
        if (scenario == 10)
            identity++;
        memcpy(saved, original, sizeof saved);
        memcpy(before, flash, sizeof flash);
        memcpy(old_catalog, catalog_data, sizeof catalog_data);
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        int result = hpclang_rpl_recovery_installation_prepare(
            io, &layout, identity, ports, catalog, FIRST, scenario == 9 ? original : staging,
            capacity, &space);
        fail_after_staging = NULL;
        if (!scenario) {
            assert(!result && !memcmp(staging, binding_expected, sizeof binding_expected));
            assert(space.library_slot == FIRST + 0x20105);
            assert(space.journal_slot == space.library_slot + sizeof binding_original);
            assert(space.authority_slot ==
                   space.journal_slot + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        } else {
            assert(result < 0 && !memcmp(&space, &sentinel, sizeof space));
            if (scenario == 8) {
                assert(result == HPCLANG_RPL_FLASH_FAILED);
                assert(!memcmp(staging, binding_expected, sizeof binding_expected));
            } else {
                for (unsigned i = 0; i < sizeof staging; ++i)
                    assert(staging[i] == 0xa5);
            }
        }
        for (unsigned i = sizeof original; i < sizeof staging; ++i)
            assert(staging[i] == 0xa5);
        assert(!memcmp(original, saved, sizeof saved));
        assert(!memcmp(before, flash, sizeof flash));
        assert(!memcmp(old_catalog, catalog_data, sizeof catalog_data));
    }
}

static void selected_creation_tests(const struct hpclang_rpl_flash_io *source)
{
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_flash;
    for (unsigned scenario = 0; scenario < 14; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, ((vectors[0].span + 15) & ~7u) / 2);
        uint32_t slot = FIRST + (scenario < 6 ? scenario : 4) * 0x20000 + 0x105;
        if (scenario == 6)
            slot += 4; /* No holes, even in otherwise erased storage. */
        if (scenario == 7)
            slot--;
        if (scenario == 8)
            slot = END;
        if (scenario == 9)
            fail_address = END - 2;
        if (scenario == 10)
            flash[END - FIRST - 1] = 0;
        if (scenario == 11)
            memcpy(flash + 0x105, binding_original, sizeof binding_original);
        if (scenario == 12) {
            tail_space(4, 0);
            slot = FIRST + 5 * 0x20000 - 3;
        }
        programs = 0;
        fail_program = scenario == 13 ? 1 : 0;
        memcpy(before, flash, sizeof flash);
        int result = hpclang_rpl_flash_create_library_at(&io, binding_original,
                                                         sizeof binding_original, slot);
        if (scenario < 6) {
            assert(!result && programs);
            assert(!memcmp(flash + slot - FIRST, binding_original, sizeof binding_original));
            assert(!memcmp(flash, before, slot - FIRST));
            unsigned after = slot - FIRST + sizeof binding_original;
            assert(!memcmp(flash + after, before + after, sizeof flash - after));
        } else if (scenario == 13) {
            assert(result == HPCLANG_RPL_FLASH_FAILED && programs > 1);
            assert(flash[slot - FIRST] == 0); /* Verified discard, never a live library. */
            assert(!memcmp(flash, before, slot - FIRST));
            unsigned after = slot - FIRST + sizeof binding_original;
            assert(!memcmp(flash + after, before + after, sizeof flash - after));
        } else {
            int expected = scenario <= 7    ? HPCLANG_RPL_FLASH_OCCUPIED
                           : scenario == 8  ? HPCLANG_RPL_FLASH_INVALID
                           : scenario == 9  ? HPCLANG_RPL_FLASH_FAILED
                           : scenario == 10 ? HPCLANG_RPL_FLASH_DIRTY
                           : scenario == 11 ? HPCLANG_RPL_FLASH_EXISTS
                                            : HPCLANG_RPL_FLASH_NOSPACE;
            assert(result == expected && !programs);
            assert(!memcmp(before, flash, sizeof flash));
        }
    }
    fail_program = 0;
}

static void interrupted_library_tests(const struct hpclang_rpl_flash_io *source)
{
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_flash;
    const unsigned slot = 0x105;
    for (unsigned status = 1; status < 255; ++status) {
        if (status == 0x53 || (status & 0x53) != 0x53)
            continue;
        const uint16_t masks[] = {0, 0xaaaa, 0xffff};
        for (unsigned cut = 1; cut <= 3; ++cut)
            for (unsigned mask = 0; mask < 3; ++mask) {
                install(0, 0);
                memcpy(flash + slot, binding_original, sizeof binding_original);
                flash[slot] = status;
                memcpy(before, flash, sizeof flash);
                programs = 0;
                fail_program = cut;
                partial_program_mask = masks[mask];
                assert(hpclang_rpl_flash_recover_tails(&io) == HPCLANG_RPL_FLASH_DIRTY);
                assert(programs == cut && flash[slot] != 0x53);
                fail_program = 0;
                partial_program_mask = 0;
                assert(!hpclang_rpl_flash_recover_tails(&io));
                assert(flash[slot] == 0 && flash[slot + 3] == (before[slot + 3] & 15));
                before[slot] = 0;
                before[slot + 3] &= 15;
                assert(!memcmp(before, flash, sizeof flash));
                unsigned completed = programs;
                assert(!hpclang_rpl_flash_recover_tails(&io) && programs == completed);
            }
    }
    for (unsigned fault = 0; fault < 7; ++fault) {
        install(0, 0);
        memcpy(flash + slot, binding_original, sizeof binding_original);
        flash[slot] = 0xfb;
        if (fault == 0)
            flash[slot] = 0x52; /* Partial removal of a live library is not fresh creation. */
        if (fault == 1)
            flash[slot + 30] ^= 1;
        if (fault == 2)
            memcpy(flash + slot + sizeof binding_original, binding_original,
                   sizeof binding_original);
        if (fault == 3)
            flash[sizeof flash - 1] = 0;
        if (fault == 4)
            fail_address = END - 2;
        if (fault == 5)
            flash[slot + 3] |= 0x20;
        if (fault == 6)
            flash[slot + sizeof binding_original - 1] = 0; /* Non-erased padding. */
        memcpy(before, flash, sizeof flash);
        programs = 0;
        int result = hpclang_rpl_flash_recover_tails(&io);
        assert(result == (fault == 4 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY));
        assert(!programs && !memcmp(before, flash, sizeof flash));
    }
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {flash, read_flash, NULL, FIRST, END};
    const struct hpclang_rpl_reader reader = {read_catalog, catalog_data};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 120, 35, 0};
    placement_tests(&io, &catalog);
    space_tests(&io);
    preparation_tests(&io, &catalog);
    selected_creation_tests(&io);
    interrupted_library_tests(&io);
    for (unsigned v = 0; v < sizeof vectors / sizeof vectors[0]; ++v)
        for (unsigned bank = 0; bank < 6; ++bank) {
            install(v, bank);
            uint32_t slot = UINT32_MAX;
            memcpy(before, flash, sizeof flash);
            assert(!hpclang_rpl_recovery_installation_match(&io, 1701, &catalog,
                                                            vectors[v].descriptor, &slot));
            assert(slot == FIRST + bank * 0x20000 + 0x105);
            assert(!memcmp(before, flash, sizeof flash));
        }
    for (unsigned fault = 0; fault < 13; ++fault) {
        install(fault == 9 ? 1 : 0, 0); /* Another valid library with the same ID. */
        uint8_t expected[48];
        memcpy(expected, vectors[0].descriptor, sizeof expected);
        if (fault == 0)
            flash[0x105] = 0; /* A deleted root is not installed. */
        if (fault == 1) {
            unsigned bytes = ((vectors[0].span + 15) & ~7u) / 2;
            memcpy(flash + 0x20105, flash + 0x105, bytes);
        }
        if (fault == 2)
            word(126, 0x40223, 5);
        if (fault == 3)
            word(131, 0x12345 + 81, 5);
        if (fault == 4)
            word(7 * 9, 0x12345 + 64, 5);
        if (fault == 5)
            expected[47] ^= 1;
        if (fault == 6)
            fail_address = END - 2;
        if (fault == 7) {
            word(120, 2, 3);
            memcpy(catalog_data + 139, catalog_data + 123, 16);
        }
        if (fault == 8)
            flash[END - FIRST - 1] = 0;
        if (fault == 10)
            flash[0x108] &= 15; /* Correct object inside the wrong envelope type. */
        if (fault == 11 || fault == 12) {
            unsigned extent =
                flash[0x106] | (unsigned)flash[0x107] << 8 | (unsigned)(flash[0x108] & 15) << 16;
            /* Neither extra bytes nor a truncated half-byte extent are owned. */
            extent += fault == 11 ? 8 : 1;
            flash[0x106] = extent;
            flash[0x107] = extent >> 8;
            flash[0x108] = 0x10 | (extent >> 16);
        }
        memcpy(before, flash, sizeof flash);
        uint32_t slot = UINT32_MAX;
        assert(hpclang_rpl_recovery_installation_match(&io, 1701, &catalog, expected, &slot) < 0);
        assert(slot == UINT32_MAX && !memcmp(before, flash, sizeof flash));
    }
    transaction_tests(&io, &catalog);
    return 0;
}
