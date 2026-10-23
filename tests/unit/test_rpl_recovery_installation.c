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
static uint8_t fail_after_staging_expected = 0xa5;
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
    if (at == fail_address ||
        (fail_after_staging && *fail_after_staging != fail_after_staging_expected))
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
    uint8_t image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
    uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    uint8_t scratch[2 * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    const struct hpclang_rpl_flash_installed_recovery sentinel = {1, 2, 3, 4};
    for (unsigned scenario = 0; scenario < 20; ++scenario) {
        const uint32_t JOURNAL = scenario < 14 ? 0x160105 : FIRST + 0x2005;
        const uint32_t AUTHORITY = scenario < 14 ? 0x180105 : JOURNAL + sizeof journal;
        unsigned bank = scenario == 5 ? 0 : 3;
        install(0, bank);
        if (scenario >= 14) {
            /* Real placement may put the pair behind the target bank header.
             * Preserve a complete deleted allocation up to that free tail. */
            uint32_t extent = 2 * (JOURNAL - FIRST - 0x105) - 2;
            flash[0x105] = 0;
            flash[0x106] = extent;
            flash[0x107] = extent >> 8;
            flash[0x108] = extent >> 16;
        }
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
        if (scenario == 10 || scenario == 15 || scenario == 16)
            journal[0] = 0x7c;
        if (scenario == 17)
            journal[0] = authority[0] = 0;
        if (scenario == 11) {
            journal[0] = 0;
            authority[0] = 0x7c;
        }
        if (scenario == 12)
            authority[0] = 0;
        memcpy(flash + JOURNAL - FIRST, journal, sizeof journal);
        memcpy(flash + AUTHORITY - FIRST, authority, sizeof authority);
        if (scenario == 0 || scenario == 5 || scenario == 14 || scenario == 16 || scenario == 17 ||
            scenario == 18 || scenario == 19)
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
        if (scenario == 19)
            fail_address = AUTHORITY & ~1u;
        if (scenario == 13 || scenario == 18) {
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
        if (scenario < 3 || scenario == 9 || scenario == 11 || scenario == 14 || scenario == 15) {
            const int expected[] = {1, 2, 0};
            int wanted = scenario == 14                     ? 1
                         : scenario == 15 || scenario == 11 ? 2
                         : scenario == 9                    ? 0
                                                            : expected[scenario];
            assert(phase == wanted);
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

static void creation_space_tests(const struct hpclang_rpl_flash_io *io)
{
    uint8_t record[4096];
    unsigned bytes = ((vectors[0].span + 15) & ~7u) / 2;
    install(0, 0);
    memcpy(record, flash + 0x105, bytes);
    if (vectors[0].span & 1u)
        record[4 + vectors[0].span / 2] |= 0xf0;
    const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
    for (unsigned scenario = 0; scenario < 8; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, bytes);
        for (unsigned bank = 0; bank < 6; ++bank)
            tail_space(bank, 0);
        tail_space(0, bytes + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        tail_space(1, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
        tail_space(2, HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES);
        uint32_t target = END - HPCLANG_RPL_FLASH_SECTOR_BYTES;
        if (scenario == 1)
            tail_space(2, HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES - 4);
        if (scenario == 2)
            target = FIRST + 0x5f000;
        if (scenario == 3)
            fail_address = END - 2;
        if (scenario == 4)
            flash[END - FIRST - 1] = 0;
        if (scenario == 6) {
            tail_space(0, bytes + HPCLANG_RPL_FLASH_AUTHORITY_BYTES +
                              HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES +
                              HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES);
            tail_space(1, 0);
            tail_space(2, 0);
        }
        if (scenario == 7) {
            /* The first library candidate consumes the only journal tail. */
            tail_space(0, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
            tail_space(1, bytes + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        }
        memcpy(before, flash, sizeof flash);
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        uint32_t claim = UINT32_MAX;
        int result = hpclang_rpl_flash_plan_creation_space(io, record, bytes, target, &space,
                                                           scenario == 5 ? NULL : &claim);
        if (!scenario || scenario >= 6) {
            assert(!result);
            uint32_t slots[4] = {space.library_slot, space.journal_slot, space.authority_slot,
                                 claim};
            uint32_t sizes[4] = {bytes, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
                                 HPCLANG_RPL_FLASH_AUTHORITY_BYTES,
                                 HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES};
            for (unsigned i = 0; i < 4; ++i) {
                assert((slots[i] & 3u) == 1 && slots[i] >= FIRST && slots[i] + sizes[i] < END);
                assert(slots[i] >= target + HPCLANG_RPL_FLASH_SECTOR_BYTES ||
                       slots[i] + sizes[i] <= target);
                for (unsigned j = 0; j < i; ++j)
                    assert(slots[i] >= slots[j] + sizes[j] || slots[j] >= slots[i] + sizes[i]);
            }
            if (scenario == 6)
                assert(claim == space.authority_slot + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
            else
                assert(claim == FIRST + 0x60000 - HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES - 3);
        } else {
            int expected = scenario == 3   ? HPCLANG_RPL_FLASH_FAILED
                           : scenario == 4 ? HPCLANG_RPL_FLASH_DIRTY
                           : scenario == 5 ? HPCLANG_RPL_FLASH_INVALID
                                           : HPCLANG_RPL_FLASH_NOSPACE;
            assert(result == expected && claim == UINT32_MAX &&
                   !memcmp(&space, &sentinel, sizeof space));
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

static void creation_preparation_tests(const struct hpclang_rpl_flash_io *io,
                                       const struct hpclang_rpl_flash_catalog *catalog)
{
    uint8_t original[sizeof creation_original], staging[sizeof original + 16];
    struct ram_port ram = {.fail = UINT32_MAX};
    const struct hpclang_rpl_reader empty = {read_port, &ram};
    const struct hpclang_rpl_recovery_port_view ports[2] = {{&empty, 0, sizeof ram.data},
                                                            {&empty, 0, sizeof ram.data}};
    const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
    for (unsigned scenario = 0; scenario < 9; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, ((vectors[0].span + 15) & ~7u) / 2);
        word(120, 0, 3);
        memcpy(original, creation_original, sizeof original);
        memset(staging, 0xa5, sizeof staging);
        struct hpclang_rpl_flash_recovery_layout layout = {original, sizeof original,
                                                           creation_recovery_binding_nibble,
                                                           creation_admission_binding_nibble};
        uint32_t at = creation_admission_creation_nibble;
        if (scenario == 1)
            at = UINT32_MAX;
        if (scenario == 2)
            at = 8;
        if (scenario == 3)
            at = 8 + creation_identity_object_nibble;
        if (scenario == 4)
            at = creation_recovery_binding_nibble + 30;
        if (scenario == 5)
            at = creation_admission_binding_nibble - 6;
        if (scenario == 6)
            fail_after_staging = staging;
        if (scenario == 8)
            fail_address = END - 2;
        memcpy(before, flash, sizeof flash);
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        uint32_t claim = UINT32_MAX;
        int result = hpclang_rpl_recovery_installation_prepare_creation(
            io, &layout, creation_identity_object_nibble, at, ports, catalog, FIRST, staging,
            sizeof staging, &space, scenario == 7 ? NULL : &claim);
        fail_after_staging = NULL;
        if (!scenario) {
            assert(!result && !memcmp(staging, creation_expected, sizeof creation_expected));
            assert(space.library_slot == FIRST + 0x20105);
            assert(space.journal_slot == space.library_slot + sizeof creation_original);
            assert(claim == space.authority_slot + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        } else {
            assert(result < 0 && claim == UINT32_MAX && !memcmp(&space, &sentinel, sizeof space));
            if (scenario == 6)
                assert(result == HPCLANG_RPL_FLASH_FAILED &&
                       !memcmp(staging, creation_expected, sizeof creation_expected));
            else
                for (unsigned i = 0; i < sizeof staging; ++i)
                    assert(staging[i] == 0xa5);
        }
        for (unsigned i = sizeof original; i < sizeof staging; ++i)
            assert(staging[i] == 0xa5);
        assert(!memcmp(original, creation_original, sizeof original));
        assert(!memcmp(before, flash, sizeof flash));
    }
}

static void in_place_creation_tests(const struct hpclang_rpl_flash_io *io,
                                    const struct hpclang_rpl_flash_catalog *catalog)
{
    uint8_t storage[sizeof creation_original + 16], original[sizeof creation_original];
    uint8_t separate[sizeof creation_original];
    uint8_t *owned = storage + 8;
    struct ram_port ram = {.fail = UINT32_MAX};
    const struct hpclang_rpl_reader empty = {read_port, &ram};
    const struct hpclang_rpl_recovery_port_view ports[2] = {{&empty, 0, sizeof ram.data},
                                                            {&empty, 0, sizeof ram.data}};
    const struct hpclang_rpl_flash_recovery_space sentinel = {1, 2, 3};
    for (unsigned scenario = 0; scenario < 10; ++scenario) {
        install(0, 0);
        memset(flash + 0x105, 255, ((vectors[0].span + 15) & ~7u) / 2);
        word(120, 0, 3);
        memset(storage, 0xa5, sizeof storage);
        memcpy(owned, creation_original, sizeof creation_original);
        if (scenario == 9)
            owned[30] ^= 1;
        memcpy(original, owned, sizeof original);
        struct hpclang_rpl_flash_recovery_layout layout = {owned, sizeof creation_original,
                                                           creation_recovery_binding_nibble,
                                                           creation_admission_binding_nibble};
        if (scenario == 6)
            fail_address = END - 2;
        if (scenario == 8) {
            unsigned changed = 0;
            while (changed < sizeof original && original[changed] == creation_expected[changed])
                ++changed;
            assert(changed < sizeof original);
            fail_after_staging = owned + changed;
            fail_after_staging_expected = original[changed];
        }
        memcpy(before, flash, sizeof flash);
        struct hpclang_rpl_flash_recovery_space space = sentinel;
        uint32_t claim = UINT32_MAX;
        int result =
            scenario == 1
                ? hpclang_rpl_recovery_installation_prepare_creation(
                      io, &layout, creation_identity_object_nibble,
                      creation_admission_creation_nibble, ports, catalog, FIRST, owned,
                      sizeof original, &space, &claim)
                : hpclang_rpl_recovery_installation_prepare_creation_in_place(
                      io, &layout, creation_identity_object_nibble,
                      scenario == 5 ? 8 : creation_admission_creation_nibble, ports, catalog, FIRST,
                      scenario == 2   ? owned + 1
                      : scenario == 3 ? separate
                                      : owned,
                      sizeof original - (scenario == 4), &space, scenario == 7 ? NULL : &claim);
        fail_after_staging = NULL;
        fail_after_staging_expected = 0xa5;
        if (!scenario) {
            assert(!result && !memcmp(owned, creation_expected, sizeof original));
            assert(space.library_slot == FIRST + 0x20105 &&
                   claim == space.authority_slot + HPCLANG_RPL_FLASH_AUTHORITY_BYTES);
        } else {
            assert(result < 0 && claim == UINT32_MAX && !memcmp(&space, &sentinel, sizeof space));
            assert(!memcmp(owned, scenario == 8 ? creation_expected : original, sizeof original));
        }
        for (unsigned i = 0; i < 8; ++i)
            assert(storage[i] == 0xa5 && storage[sizeof storage - 1 - i] == 0xa5);
        assert(!memcmp(before, flash, sizeof flash));
    }
    fail_address = UINT32_MAX;
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

static void claim_binding_word(uint8_t *binding, unsigned at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        binding[at + i] = (uint8_t)(value >> (8 * i));
}

static unsigned close_reads, close_read_failure;
static int close_read(void *context, uint32_t at, uint16_t *value)
{
    if (++close_reads == close_read_failure)
        return 0;
    return read_flash(context, at, value);
}

static int close_silent_program(void *context, uint32_t at, uint16_t value)
{
    (void)context;
    (void)at;
    (void)value;
    ++programs;
    return 1;
}

static void creation_close_tests(const struct hpclang_rpl_flash_io *source, uint32_t slot,
                                 const uint8_t *record)
{
    const unsigned bytes = HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES;
    struct hpclang_rpl_flash_io io = *source;
    io.read16 = close_read;
    io.program16 = program_flash;
    for (unsigned phase = 0; phase < 256; ++phase) {
        install(0, 3);
        memcpy(flash + slot - FIRST, record, bytes);
        flash[slot - FIRST + 16] = phase;
        flash[slot - FIRST - 1] = 0x23;
        flash[slot - FIRST + bytes] = 0x53; /* Occupied successor, not an erased tail. */
        memcpy(before, flash, sizeof flash);
        close_reads = close_read_failure = programs = fail_program = 0;
        int result = hpclang_rpl_flash_close_creation(&io, slot, record, bytes);
        if (!(phase & ~0x7eu)) {
            assert(!result && programs == (phase != 0));
            before[slot - FIRST + 16] = 0;
            programs = 0;
            assert(!hpclang_rpl_flash_close_creation(&io, slot, record, bytes));
            assert(!programs);
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
        }
        assert(!memcmp(before, flash, sizeof flash));
    }
    for (unsigned at = 0; at < bytes; ++at) {
        if (at == 16)
            continue;
        install(0, 3);
        memcpy(flash + slot - FIRST, record, bytes);
        flash[slot - FIRST + 16] = 0x7e;
        flash[slot - FIRST + at] ^= 1;
        memcpy(before, flash, sizeof flash);
        close_reads = close_read_failure = programs = fail_program = 0;
        assert(hpclang_rpl_flash_close_creation(&io, slot, record, bytes) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!programs && !memcmp(before, flash, sizeof flash));
    }
    /* Every preflight read and both read/verify accesses around the phase write. */
    for (unsigned cut = 1; cut <= bytes + 2; ++cut) {
        install(0, 3);
        memcpy(flash + slot - FIRST, record, bytes);
        flash[slot - FIRST + 16] = 0x7e;
        memcpy(before, flash, sizeof flash);
        close_reads = programs = fail_program = 0;
        close_read_failure = cut;
        assert(hpclang_rpl_flash_close_creation(&io, slot, record, bytes) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(programs == (cut == bytes + 2));
        if (programs)
            before[slot - FIRST + 16] = 0;
        assert(!memcmp(before, flash, sizeof flash));
        close_read_failure = programs = 0;
        assert(!hpclang_rpl_flash_close_creation(&io, slot, record, bytes));
        assert(programs == (cut != bytes + 2));
        before[slot - FIRST + 16] = 0;
        assert(!memcmp(before, flash, sizeof flash));
    }
    const uint16_t masks[] = {0, 0x5555, 0xaaaa, 0xffff};
    for (unsigned i = 0; i < sizeof masks / sizeof masks[0]; ++i) {
        install(0, 3);
        memcpy(flash + slot - FIRST, record, bytes);
        flash[slot - FIRST + 16] = 0x7e;
        memcpy(before, flash, sizeof flash);
        close_reads = close_read_failure = programs = 0;
        fail_program = 1;
        partial_program_mask = masks[i];
        assert(hpclang_rpl_flash_close_creation(&io, slot, record, bytes) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(programs == 1);
        before[slot - FIRST + 16] &= (uint8_t)~(masks[i] >> 8);
        assert(!memcmp(before, flash, sizeof flash));
        fail_program = programs = 0;
        assert(!hpclang_rpl_flash_close_creation(&io, slot, record, bytes));
        assert(programs == (masks[i] != 0xffff));
        before[slot - FIRST + 16] = 0;
        assert(!memcmp(before, flash, sizeof flash));
    }
    install(0, 3);
    memcpy(flash + slot - FIRST, record, bytes);
    flash[slot - FIRST + 16] = 0x7e;
    memcpy(before, flash, sizeof flash);
    close_reads = close_read_failure = programs = fail_program = 0;
    io.program16 = close_silent_program;
    assert(hpclang_rpl_flash_close_creation(&io, slot, record, bytes) == HPCLANG_RPL_FLASH_DIRTY);
    assert(programs == 1 && !memcmp(before, flash, sizeof flash));
    io.program16 = program_flash;
    programs = 0;
    assert(!hpclang_rpl_flash_close_creation(&io, slot, record, bytes));
    assert(programs == 1);
    before[slot - FIRST + 16] = 0;
    assert(!memcmp(before, flash, sizeof flash));
    /* A later-phase or damaged expected record is not the original intent. */
    for (unsigned fault = 0; fault < 4; ++fault) {
        uint8_t invalid[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES];
        memcpy(invalid, record, sizeof invalid);
        if (fault == 0)
            invalid[16] = 0;
        if (fault == 1)
            invalid[bytes - 1] ^= 1;
        programs = 0;
        assert(hpclang_rpl_flash_close_creation(&io, slot, fault == 2 ? NULL : invalid,
                                                bytes - (fault == 3)) == HPCLANG_RPL_FLASH_INVALID);
        assert(!programs && !memcmp(before, flash, sizeof flash));
    }
    partial_program_mask = close_reads = close_read_failure = programs = fail_program = 0;
}

static void creation_publication_tests(const struct hpclang_rpl_flash_io *source)
{
    const uint32_t slot = FIRST + 0xa0105;
    uint8_t record[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES], binding[80] = {0};
    memcpy(binding, "HPCMEM2", 8);
    memcpy(binding + 8, vectors[0].descriptor, 48);
    claim_binding_word(binding, 56, FIRST + 0x60105);
    claim_binding_word(binding, 60, FIRST + 0x20105);
    claim_binding_word(binding, 64, FIRST + 0x20105 + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES);
    claim_binding_word(binding, 68, FIRST);
    claim_binding_word(binding, 76, 32768);
    assert(!hpclang_rpl_flash_creation_encode(
        FIRST, END, slot, binding, (const uint8_t *)"HPCLEASE", 8, record, sizeof record));
    creation_close_tests(source, slot, record);
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = program_flash;
    install(0, 3);
    programs = fail_program = 0;
    assert(!hpclang_rpl_flash_publish_creation_intent(&io, slot, record, sizeof record));
    unsigned writes = programs;
    assert(writes && !memcmp(flash + slot - FIRST, record, sizeof record));
    const uint16_t masks[] = {0, 0x5555, 0xaaaa, 0xffff};
    for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask)
        for (unsigned cut = 1; cut <= writes; ++cut) {
            install(0, 3);
            memcpy(before, flash, sizeof flash);
            programs = 0;
            fail_program = cut;
            partial_program_mask = masks[mask];
            assert(hpclang_rpl_flash_publish_creation_intent(&io, slot, record, sizeof record) ==
                   HPCLANG_RPL_FLASH_DIRTY);
            assert(!memcmp(before, flash, slot - FIRST));
            assert(!memcmp(before + slot - FIRST + sizeof record,
                           flash + slot - FIRST + sizeof record, END - slot - sizeof record));
            fail_program = programs = 0;
            assert(!hpclang_rpl_flash_finish_creation_intent(&io, slot, record, sizeof record));
            assert(!memcmp(flash + slot - FIRST, record, sizeof record));
            assert(!memcmp(before, flash, slot - FIRST));
            assert(!memcmp(before + slot - FIRST + sizeof record,
                           flash + slot - FIRST + sizeof record, END - slot - sizeof record));
            programs = 0;
            assert(!hpclang_rpl_flash_finish_creation_intent(&io, slot, record, sizeof record));
            assert(!programs);
        }
    for (unsigned phase = 0; phase < 256; ++phase) {
        install(0, 3);
        memcpy(flash + slot - FIRST, record, sizeof record);
        flash[slot - FIRST + 16] = phase;
        memcpy(before, flash, sizeof flash);
        programs = fail_program = 0;
        int result = hpclang_rpl_flash_finish_creation_intent(&io, slot, record, sizeof record);
        if (phase == 0xff || phase == 0x7f)
            assert(!result && !memcmp(flash + slot - FIRST, record, sizeof record));
        else
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs &&
                   !memcmp(before, flash, sizeof flash));
        memcpy(flash, before, sizeof flash);
        programs = 0;
        result = hpclang_rpl_flash_complete_creation(&io, slot, record, sizeof record);
        if (phase == 0x7f || phase == 0x7e) {
            assert(!result && flash[slot - FIRST + 16] == 0x7e);
            before[slot - FIRST + 16] = 0x7e;
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY && !programs);
        }
        assert(!memcmp(before, flash, sizeof flash));
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        install(0, 3);
        memcpy(flash + slot - FIRST, record, sizeof record);
        if (fault == 0)
            flash[slot - FIRST + 24] ^= 1;
        if (fault == 1)
            flash[slot - FIRST + sizeof record] = 0;
        if (fault == 2)
            fail_address = (slot + 22) & ~1u;
        memcpy(before, flash, sizeof flash);
        programs = fail_program = 0;
        int result =
            fault == 3 ? hpclang_rpl_flash_publish_creation_intent(&io, slot, record, sizeof record)
                       : hpclang_rpl_flash_finish_creation_intent(&io, slot, record, sizeof record);
        assert(result < 0 && !programs && !memcmp(before, flash, sizeof flash));
    }
}

static void creation_claim_tests(const struct hpclang_rpl_flash_io *io,
                                 const struct hpclang_rpl_flash_catalog *catalog)
{
    enum
    {
        JOURNAL = FIRST + 0x2005,
        AUTHORITY = JOURNAL + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
        CLAIM = AUTHORITY + HPCLANG_RPL_FLASH_AUTHORITY_BYTES
    };
    uint8_t image[HPCLANG_RPL_FLASH_SECTOR_BYTES];
    uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    uint8_t claim[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES], binding[80];
    for (unsigned scenario = 0; scenario < 22; ++scenario) {
        install(0, 3);
        uint32_t extent = 2 * (JOURNAL - FIRST - 0x105) - 2;
        claim_binding_word(flash, 0x105, extent << 8);
        memcpy(image, flash, sizeof image);
        assert(!hpclang_rpl_flash_sector_encode(FIRST, END, JOURNAL, FIRST, image, journal,
                                                sizeof journal));
        assert(!hpclang_rpl_flash_authority_encode(FIRST, END, AUTHORITY, JOURNAL, journal,
                                                   sizeof journal, authority, sizeof authority));
        memset(binding, 0, sizeof binding);
        memcpy(binding, "HPCMEM2", 8);
        memcpy(binding + 8, vectors[0].descriptor, 48);
        claim_binding_word(binding, 56, FIRST + 0x60105);
        claim_binding_word(binding, 60, JOURNAL);
        claim_binding_word(binding, 64, AUTHORITY);
        claim_binding_word(binding, 68, FIRST);
        memcpy(binding + 72, journal + sizeof journal - 4, 4);
        claim_binding_word(binding, 76, 32768);
        uint32_t claim_slot = CLAIM + (scenario == 15 ? 4 : 0);
        assert(!hpclang_rpl_flash_creation_encode(
            FIRST, END, claim_slot, binding, (const uint8_t *)"HPCLEASE", 8, claim, sizeof claim));
        const uint8_t phases[] = {0x7f, 0x7e, 0x7c, 0, 0, 0x7e};
        claim[16] = scenario < 6 ? phases[scenario] : 0x7e;
        if (scenario == 4 || scenario == 17)
            journal[0] = authority[0] = 0;
        if (scenario == 5 || scenario == 18)
            journal[0] = 0x7c;
        if (scenario == 16)
            authority[0] = 0x7f;
        memcpy(flash + JOURNAL - FIRST, journal, sizeof journal);
        memcpy(flash + AUTHORITY - FIRST, authority, sizeof authority);
        if (scenario == 15)
            claim_binding_word(flash, CLAIM - FIRST, (2 * (sizeof claim + 4) - 2) << 8);
        memcpy(flash + claim_slot - FIRST, claim, sizeof claim);
        if (scenario != 4 && scenario != 5)
            memset(flash, 255, sizeof image);
        const uint8_t *name = (const uint8_t *)"HPCLEASE";
        if (scenario == 6)
            flash[claim_slot - FIRST + 16] = 0xff;
        if (scenario == 7)
            flash[claim_slot - FIRST + sizeof claim - 1] ^= 1;
        if (scenario == 8)
            binding[24] ^= 1;
        if (scenario == 9)
            binding[56] ^= 4;
        if (scenario == 10)
            binding[69] ^= 0x10;
        if (scenario == 11)
            binding[72] ^= 1;
        if (scenario == 12)
            binding[76] ^= 1;
        if (scenario == 13)
            name = (const uint8_t *)"OTHERONE";
        if (scenario == 14)
            claim_slot += 4;
        if (scenario == 19) {
            uint8_t other[sizeof journal];
            assert(!hpclang_rpl_flash_sector_encode(FIRST, END, 0x1e0105, FIRST, image, other,
                                                    sizeof other));
            memcpy(flash + 0x1e0105 - FIRST, other, sizeof other);
        }
        if (scenario == 20)
            fail_address = (claim_slot + 20) & ~1u;
        if (scenario == 21)
            memset(flash + claim_slot - FIRST, 255, sizeof claim);
        memcpy(before, flash, sizeof flash);
        enum hpclang_rpl_flash_creation_phase phase = 99;
        int result = hpclang_rpl_recovery_installation_creation(io, 1701, catalog, binding,
                                                                claim_slot, name, 8, &phase);
        if (scenario < 6) {
            const enum hpclang_rpl_flash_creation_phase expected[] = {
                HPCLANG_RPL_FLASH_CREATION_INTENT,  HPCLANG_RPL_FLASH_CREATION_CREATED,
                HPCLANG_RPL_FLASH_CREATION_CLOSING, HPCLANG_RPL_FLASH_CREATION_CLOSED,
                HPCLANG_RPL_FLASH_CREATION_CLOSED,  HPCLANG_RPL_FLASH_CREATION_CREATED};
            assert(!result && phase == expected[scenario]);
        } else {
            assert(result < 0 && phase == 99);
        }
        assert(!memcmp(before, flash, sizeof flash));
        if (!scenario) {
            /* Activation must validate intact actual Flash as well as its
             * proposed image; a recovery-only restored view is insufficient. */
            memcpy(flash, image, sizeof image);
            flash[claim_slot - FIRST + 16] = 0x7e;
            for (unsigned status = 0; status < 256; ++status) {
                flash[AUTHORITY - FIRST] = status;
                memcpy(before, flash, sizeof flash);
                result = hpclang_rpl_recovery_installation_creation_staged(
                    io, 1701, catalog, binding, claim_slot, name, 8);
                assert(status == 0x7f ? result == 0 : result < 0);
                const struct hpclang_rpl_flash_installed_recovery sentinel = {1, 2, 3, 4};
                struct hpclang_rpl_flash_installed_recovery staged = sentinel;
                result = hpclang_rpl_recovery_installation_staged(io, 1701, catalog, binding + 8,
                                                                  AUTHORITY, JOURNAL, &staged);
                if (status == 0x7f) {
                    assert(!result && staged.library_slot == FIRST + 0x60105 &&
                           staged.journal_slot == JOURNAL && staged.authority_slot == AUTHORITY &&
                           staged.target == FIRST);
                } else {
                    assert(result < 0 && !memcmp(&staged, &sentinel, sizeof staged));
                }
                assert(!memcmp(before, flash, sizeof flash));
            }
            flash[AUTHORITY - FIRST] = 0x7f;
            for (unsigned status = 0; status < 256; ++status) {
                flash[claim_slot - FIRST + 16] = status;
                memcpy(before, flash, sizeof flash);
                result = hpclang_rpl_recovery_installation_creation_staged(
                    io, 1701, catalog, binding, claim_slot, name, 8);
                assert(status == 0x7e ? result == 0 : result < 0);
                assert(!memcmp(before, flash, sizeof flash));
            }
            flash[claim_slot - FIRST + 16] = 0x7e;
            flash[0x100] ^= 1;
            memcpy(before, flash, sizeof flash);
            assert(hpclang_rpl_recovery_installation_creation_staged(io, 1701, catalog, binding,
                                                                     claim_slot, name, 8) < 0);
            assert(!memcmp(before, flash, sizeof flash));
            flash[0x100] ^= 1;
            fail_address = (claim_slot + 20) & ~1u;
            memcpy(before, flash, sizeof flash);
            assert(hpclang_rpl_recovery_installation_creation_staged(io, 1701, catalog, binding,
                                                                     claim_slot, name, 8) < 0);
            assert(!memcmp(before, flash, sizeof flash));
        }
    }
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {flash, read_flash, NULL, FIRST, END};
    const struct hpclang_rpl_reader reader = {read_catalog, catalog_data};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 120, 35, 0};
    creation_claim_tests(&io, &catalog);
    creation_publication_tests(&io);
    creation_preparation_tests(&io, &catalog);
    in_place_creation_tests(&io, &catalog);
    placement_tests(&io, &catalog);
    space_tests(&io);
    creation_space_tests(&io);
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
