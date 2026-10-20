#include "rpl-recovery-installation.h"
#include <assert.h>
#include <string.h>
#include "recovery-identity-vectors.h"

enum
{
    FIRST = 0x140000,
    END = 0x200000
};
static uint8_t flash[END - FIRST], before[END - FIRST], catalog_data[160];
static uint32_t fail_address;
static int read_flash(void *context, uint32_t at, uint16_t *value)
{
    assert(context == flash && !(at & 1) && at >= FIRST && at + 1 < END);
    if (at == fail_address)
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
        if (!scenario) {
            assert(!result && slot == FIRST + 0x105);
        } else {
            int expected = scenario <= 3 || scenario == 10 ? HPCLANG_RPL_FLASH_EXISTS
                           : scenario == 4 || scenario == 5 || scenario == 11
                               ? HPCLANG_RPL_FLASH_FAILED
                           : scenario == 9 ? HPCLANG_RPL_FLASH_INVALID
                                           : HPCLANG_RPL_FLASH_DIRTY;
            assert(result == expected && slot == UINT32_MAX);
        }
        assert(!memcmp(before, flash, sizeof flash));
        assert(!memcmp(original, ram, sizeof ram));
        assert(!memcmp(old_catalog, catalog_data, sizeof catalog_data));
    }
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {flash, read_flash, NULL, FIRST, END};
    const struct hpclang_rpl_reader reader = {read_catalog, catalog_data};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 120, 35, 0};
    placement_tests(&io, &catalog);
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
