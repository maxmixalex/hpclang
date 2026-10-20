#include "rpl-recovery-installation.h"
#include "rpl-port.h"
#include <stddef.h>

static int installation_span_result(int result)
{
    return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                  : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_recovery_installation_plan(const struct hpclang_rpl_flash_io *io,
                                           const uint8_t *record, uint32_t bytes,
                                           const struct hpclang_rpl_recovery_port_view ports[2],
                                           const struct hpclang_rpl_flash_catalog *catalog,
                                           uint32_t *slot)
{
    if (!slot || !ports || !catalog || !catalog->reader || !catalog->reader->read_nibble)
        return HPCLANG_RPL_FLASH_INVALID;
    for (unsigned port = 0; port < 2; ++port)
        if (!ports[port].reader || !ports[port].reader->read_nibble)
            return HPCLANG_RPL_FLASH_INVALID;
    uint32_t selected;
    int result = hpclang_rpl_flash_plan_library(io, record, bytes, &selected);
    if (result)
        return result;
    /* plan_library validated the complete native envelope, title and CRC.
     * Library ID follows the byte title and its repeated length. */
    uint32_t at = 11u + record[9];
    uint32_t number = record[at] | (uint32_t)(record[at + 1] & 15u) << 8;
    int found = 0;
    for (unsigned port = 0; port < 2; ++port) {
        struct hpclang_rpl_port_cursor cursor = {ports[port].first, ports[port].end, 0};
        struct hpclang_rpl_port_entry entry;
        while ((result = hpclang_rpl_port_next(ports[port].reader, &cursor, &entry)) > 0)
            if (entry.prologue == 0x02b40u && entry.library_number == number)
                found = 1;
        if (result)
            return installation_span_result(result);
    }
    struct hpclang_rpl_catalog_insertion insertion;
    result = hpclang_rpl_catalog_plan_insert(catalog->reader, catalog->address, catalog->nibbles,
                                             number, &insertion);
    if (result < 0)
        return installation_span_result(result);
    if (!result || found)
        return HPCLANG_RPL_FLASH_EXISTS;
    *slot = selected;
    return 0;
}

struct object_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t first;
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    const struct object_reader *reader = context;
    if (at / 2u >= reader->io->end - reader->first)
        return 0;
    uint32_t address = reader->first + at / 2u;
    uint16_t word;
    if (!reader->io->read16(reader->io->context, address & ~1u, &word))
        return 0;
    *value = (uint8_t)((word >> (8u * (address & 1u) + 4u * (at & 1u))) & 15u);
    return 1;
}

int hpclang_rpl_recovery_installation_match(const struct hpclang_rpl_flash_io *io,
                                            uint32_t library_number,
                                            const struct hpclang_rpl_flash_catalog *catalog,
                                            const uint8_t expected[48], uint32_t *slot)
{
    if (!expected || !slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t found;
    int result = hpclang_rpl_flash_locate_installation(io, library_number, catalog, &found);
    if (result)
        return result;
    struct object_reader mapped = {io, found + 4u};
    const struct hpclang_rpl_reader reader = {read_nibble, &mapped};
    result = hpclang_rpl_recovery_identity_match(&reader, 0, expected);
    if (result != 1)
        return result < 0 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY;
    *slot = found;
    return 0;
}

struct restored_reader
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t target, journal, authority;
    const uint8_t *image;
};

static int restored_read(void *context, uint32_t at, uint16_t *value)
{
    const struct restored_reader *view = context;
    if (view->image && at >= view->target && at - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        uint32_t offset = at - view->target;
        *value = view->image[offset] | (uint16_t)view->image[offset + 1] << 8;
    } else if (!view->io->read16(view->io->context, at, value)) {
        return 0;
    }
    if (at == (view->journal & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->journal & 1)));
    if (at == (view->authority & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->authority & 1)));
    return 1;
}

static int finish_preflight(const struct hpclang_rpl_flash_io *io, uint32_t library_number,
                            const struct hpclang_rpl_flash_catalog *catalog,
                            const uint8_t expected[48], uint32_t authority, uint32_t journal,
                            uint32_t target, int phase,
                            struct hpclang_rpl_flash_installed_recovery *installed)
{
    uint32_t library;
    int result =
        hpclang_rpl_recovery_installation_match(io, library_number, catalog, expected, &library);
    if (result)
        return result;
    /* Read the actual allocation extent, including native padding. A valid
     * object supplied only by the journal cannot serve as its recovery root. */
    struct object_reader allocation = {io, library};
    uint32_t extent = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!read_nibble(&allocation, 2 + i, &nibble))
            return HPCLANG_RPL_FLASH_FAILED;
        extent |= (uint32_t)nibble << (4 * i);
    }
    uint32_t bytes = (extent + 2) / 2;
    if (library < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < library + bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    *installed = (struct hpclang_rpl_flash_installed_recovery){library, journal, authority, target};
    return phase;
}

int hpclang_rpl_recovery_installation_preflight(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected[48], uint32_t authority,
    uint32_t journal, uint8_t *scratch, uint32_t capacity,
    struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!installed || !expected)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t target = 0;
    int phase =
        hpclang_rpl_flash_preflight_bound(io, authority, journal, scratch, capacity, &target);
    if (phase < 0)
        return phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_inspect(io->first, io->end, journal, scratch,
                                         HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image,
                                         &journal_phase))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct restored_reader view = {io, target, journal, authority, phase ? image : NULL};
    const struct hpclang_rpl_flash_io restored = {&view, restored_read, NULL, io->first, io->end};
    return finish_preflight(&restored, library_number, catalog, expected, authority, journal,
                            target, phase, installed);
}

struct streamed_view
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t target, journal, authority;
    int pending;
};

static int streamed_read(void *context, uint32_t at, uint16_t *value)
{
    const struct streamed_view *view = context;
    if (view->pending && at >= view->target && at - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        uint32_t source = view->journal + 24u + at - view->target;
        uint16_t first, second;
        if (!view->io->read16(view->io->context, source & ~1u, &first))
            return 0;
        if (source & 1u) {
            if (!view->io->read16(view->io->context, (source + 1u) & ~1u, &second))
                return 0;
            *value = (uint16_t)((first >> 8u) | (second << 8u));
        } else {
            *value = first;
        }
    } else if (!view->io->read16(view->io->context, at, value)) {
        return 0;
    }
    if (at == (view->journal & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->journal & 1)));
    if (at == (view->authority & ~1u))
        *value &= (uint16_t)~(255u << (8 * (view->authority & 1)));
    return 1;
}

int hpclang_rpl_recovery_installation_stream(const struct hpclang_rpl_flash_io *io,
                                             uint32_t library_number,
                                             const struct hpclang_rpl_flash_catalog *catalog,
                                             const uint8_t expected[48], uint32_t authority,
                                             uint32_t journal,
                                             struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!io || !io->read16 || !expected || !installed || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u || journal < io->first ||
        journal >= io->end || HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES > io->end - journal ||
        authority < io->first || authority >= io->end ||
        HPCLANG_RPL_FLASH_AUTHORITY_BYTES > io->end - authority)
        return HPCLANG_RPL_FLASH_INVALID;
    struct object_reader source = {io, journal};
    const struct hpclang_rpl_reader reader = {read_nibble, &source};
    struct hpclang_rpl_flash_sector_descriptor descriptor;
    int result = hpclang_rpl_flash_sector_inspect_reader(io->first, io->end, journal, &reader, 0,
                                                         &descriptor);
    if (result)
        return result == HPCLANG_RPL_SPAN_UNAVAILABLE ? HPCLANG_RPL_FLASH_FAILED
                                                      : HPCLANG_RPL_FLASH_DIRTY;
    uint8_t record[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    source.first = authority;
    for (unsigned i = 0; i < sizeof record; ++i) {
        uint8_t low, high;
        if (!read_nibble(&source, 2 * i, &low) || !read_nibble(&source, 2 * i + 1, &high))
            return HPCLANG_RPL_FLASH_FAILED;
        record[i] = low | high << 4;
    }
    enum hpclang_rpl_flash_authority_phase authority_phase;
    if (hpclang_rpl_flash_authority_inspect_descriptor(io->first, io->end, authority, record,
                                                       sizeof record, journal, &descriptor,
                                                       &authority_phase) ||
        authority_phase == HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED ||
        (authority_phase != HPCLANG_RPL_FLASH_AUTHORITY_ARMED &&
         descriptor.phase != HPCLANG_RPL_FLASH_SECTOR_RETIRED))
        return HPCLANG_RPL_FLASH_DIRTY;
    int phase = authority_phase == HPCLANG_RPL_FLASH_AUTHORITY_CLOSED ? 0
                : descriptor.phase == HPCLANG_RPL_FLASH_SECTOR_READY  ? 1
                                                                      : 2;
    struct streamed_view view = {io, descriptor.target, journal, authority, phase != 0};
    const struct hpclang_rpl_flash_io restored = {&view, streamed_read, NULL, io->first, io->end};
    if (phase == 2)
        for (unsigned at = descriptor.target;
             at < descriptor.target + HPCLANG_RPL_FLASH_SECTOR_BYTES; at += 2) {
            uint16_t actual, desired;
            if (!io->read16(io->context, at, &actual) || !streamed_read(&view, at, &desired))
                return HPCLANG_RPL_FLASH_FAILED;
            if (actual != desired)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t slot = index ? authority : journal;
        uint32_t size =
            index ? HPCLANG_RPL_FLASH_AUTHORITY_BYTES : HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
        struct object_reader mapped = {&restored, slot & ~0x1ffffu};
        const struct hpclang_rpl_reader bank = {read_nibble, &mapped};
        struct hpclang_rpl_flash_allocation allocation;
        uint32_t address = (slot - mapped.first) * 2u;
        if (hpclang_rpl_flash_allocation_find(&bank, 0x20au, 0x40000u, address, &allocation) != 1 ||
            allocation.address != address || allocation.nibbles != 2u * size)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return finish_preflight(&restored, library_number, catalog, expected, authority, journal,
                            descriptor.target, phase, installed);
}
