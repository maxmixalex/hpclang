#include "rpl-recovery-installation.h"
#include "rpl-port.h"
#include <stddef.h>
#include <string.h>

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

int hpclang_rpl_recovery_installation_plan_space(
    const struct hpclang_rpl_flash_io *io, const uint8_t *record, uint32_t bytes,
    const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target,
    struct hpclang_rpl_flash_recovery_space *space)
{
    if (!space)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t unused;
    int result = hpclang_rpl_recovery_installation_plan(io, record, bytes, ports, catalog, &unused);
    if (result)
        return result;
    return hpclang_rpl_flash_plan_recovery_space(io, record, bytes, target, space);
}

struct template_reader
{
    const uint8_t *record;
    uint32_t bytes;
};

static int template_nibble(void *context, uint32_t at, uint8_t *value)
{
    const struct template_reader *source = context;
    if (at / 2 >= source->bytes)
        return 0;
    *value = (source->record[at / 2] >> (4 * (at & 1))) & 15;
    return 1;
}

static uint32_t template_word(const uint8_t *record, uint32_t at, unsigned count)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i, ++at)
        result |= (uint32_t)((record[at / 2] >> (4 * (at & 1))) & 15) << (4 * i);
    return result;
}

static void template_put(uint8_t *record, uint32_t at, uint32_t value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i, ++at) {
        unsigned shift = 4 * (at & 1);
        record[at / 2] =
            (uint8_t)((record[at / 2] & ~(15u << shift)) | ((value >> (4 * i)) & 15) << shift);
    }
}

static int prepare_space(const struct hpclang_rpl_flash_io *io, const uint8_t *record,
                         uint32_t bytes, const struct hpclang_rpl_recovery_port_view ports[2],
                         const struct hpclang_rpl_flash_catalog *catalog, uint32_t target,
                         struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim)
{
    if (!claim)
        return hpclang_rpl_recovery_installation_plan_space(io, record, bytes, ports, catalog,
                                                            target, space);
    uint32_t unused;
    int result = hpclang_rpl_recovery_installation_plan(io, record, bytes, ports, catalog, &unused);
    return result ? result
                  : hpclang_rpl_flash_plan_creation_space(io, record, bytes, target, space, claim);
}

static int prepare_installation(const struct hpclang_rpl_flash_io *io,
                                const struct hpclang_rpl_flash_recovery_layout *layout,
                                uint32_t identity_nibble,
                                const struct hpclang_rpl_recovery_port_view ports[2],
                                const struct hpclang_rpl_flash_catalog *catalog, uint32_t target,
                                uint8_t *staging, uint32_t capacity,
                                struct hpclang_rpl_flash_recovery_space *space,
                                uint32_t claim_nibble, uint32_t *claim_slot)
{
    if (!layout || !layout->record || !space || !staging || capacity < layout->bytes)
        return HPCLANG_RPL_FLASH_INVALID;
    uintptr_t source_address = (uintptr_t)layout->record, destination = (uintptr_t)staging;
    if (source_address <= destination ? destination - source_address < layout->bytes
                                      : source_address - destination < layout->bytes)
        return HPCLANG_RPL_FLASH_INVALID;
    struct hpclang_rpl_flash_recovery_space planned;
    uint32_t planned_claim;
    int result = prepare_space(io, layout->record, layout->bytes, ports, catalog, target, &planned,
                               claim_slot ? &planned_claim : NULL);
    if (result)
        return result;
    uint32_t end = 13u + template_word(layout->record, 13u, 5);
    if (end < 108 || identity_nibble < 10 || identity_nibble > end - 108)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t identity = 8u + identity_nibble;
    if (template_word(layout->record, identity + 24, 8) != identity_nibble)
        return HPCLANG_RPL_FLASH_INVALID;
    uint8_t expected[48];
    for (unsigned i = 0; i < 48; ++i)
        expected[i] = (uint8_t)template_word(layout->record, identity + 2 * i, 2);
    struct template_reader source = {layout->record, layout->bytes};
    const struct hpclang_rpl_reader reader = {template_nibble, &source};
    if (hpclang_rpl_recovery_identity_match(&reader, 8, expected) != 1)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t offsets[2] = {layout->recovery_binding, layout->admission_binding};
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t at = offsets[i];
        if (at < 18 || at > end - 36 || (at < identity + 96 && identity < at + 32) ||
            template_word(layout->record, at, 8) != 0x42435048u ||
            template_word(layout->record, at + 8, 8) != 0x31444e49u)
            return HPCLANG_RPL_FLASH_INVALID;
    }
    if ((offsets[0] < offsets[1] + 32 && offsets[1] < offsets[0] + 32) ||
        template_word(layout->record, offsets[0] + 16, 8) !=
            template_word(layout->record, offsets[1] + 16, 8) ||
        template_word(layout->record, offsets[0] + 24, 8) !=
            template_word(layout->record, offsets[1] + 24, 8))
        return HPCLANG_RPL_FLASH_INVALID;
    if (claim_slot && (claim_nibble < 18 || claim_nibble > end - 12 ||
                       (claim_nibble < identity + 96 && identity < claim_nibble + 8) ||
                       (claim_nibble < offsets[0] + 32 && offsets[0] < claim_nibble + 8) ||
                       (claim_nibble < offsets[1] + 32 && offsets[1] < claim_nibble + 8)))
        return HPCLANG_RPL_FLASH_INVALID;
    memcpy(staging, layout->record, layout->bytes);
    for (unsigned i = 0; i < 2; ++i) {
        template_put(staging, offsets[i] + 16, planned.journal_slot, 8);
        template_put(staging, offsets[i] + 24, planned.authority_slot, 8);
    }
    for (unsigned i = 16; i < 96; ++i)
        template_put(staging, identity + i, 0, 1);
    if (claim_slot)
        template_put(staging, claim_nibble, planned_claim, 8);
    if (!hpclang_rpl_recovery_identity_seal(staging + 4, layout->bytes - 4, identity_nibble))
        return HPCLANG_RPL_FLASH_INVALID;
    struct hpclang_rpl_flash_recovery_space checked;
    uint32_t checked_claim;
    result = prepare_space(io, staging, layout->bytes, ports, catalog, target, &checked,
                           claim_slot ? &checked_claim : NULL);
    if (result)
        return result;
    if (checked.library_slot != planned.library_slot ||
        checked.journal_slot != planned.journal_slot ||
        checked.authority_slot != planned.authority_slot ||
        (claim_slot && checked_claim != planned_claim))
        return HPCLANG_RPL_FLASH_DIRTY;
    *space = checked;
    if (claim_slot)
        *claim_slot = checked_claim;
    return 0;
}

int hpclang_rpl_recovery_installation_prepare(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *staging,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space)
{
    return prepare_installation(io, layout, identity_nibble, ports, catalog, target, staging,
                                capacity, space, 0, NULL);
}

int hpclang_rpl_recovery_installation_prepare_creation(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, uint32_t claim_nibble,
    const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *staging,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot)
{
    if (!claim_slot)
        return HPCLANG_RPL_FLASH_INVALID;
    return prepare_installation(io, layout, identity_nibble, ports, catalog, target, staging,
                                capacity, space, claim_nibble, claim_slot);
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

static uint32_t binding_word(const uint8_t *data)
{
    return data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

int hpclang_rpl_recovery_installation_creation(const struct hpclang_rpl_flash_io *io,
                                               uint32_t library_number,
                                               const struct hpclang_rpl_flash_catalog *catalog,
                                               const uint8_t expected_binding[80],
                                               uint32_t claim_slot, const uint8_t *name,
                                               uint32_t name_length,
                                               enum hpclang_rpl_flash_creation_phase *phase)
{
    if (!io || !io->read16 || !expected_binding || !name || !phase ||
        memcmp(expected_binding, "HPCMEM2", 8) || claim_slot < io->first || claim_slot >= io->end ||
        HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES > io->end - claim_slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal = binding_word(expected_binding + 60);
    uint32_t authority = binding_word(expected_binding + 64);
    struct hpclang_rpl_flash_installed_recovery installed;
    int transaction = hpclang_rpl_recovery_installation_stream(
        io, library_number, catalog, expected_binding + 8, authority, journal, &installed);
    if (transaction < 0)
        return transaction;
    if (installed.library_slot != binding_word(expected_binding + 56) ||
        installed.target != binding_word(expected_binding + 68))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct object_reader source = {io, journal + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES - 4u};
    for (unsigned i = 0; i < 8; ++i) {
        uint8_t value;
        if (!read_nibble(&source, i, &value))
            return HPCLANG_RPL_FLASH_FAILED;
        if (value != ((expected_binding[72 + i / 2] >> (4 * (i & 1))) & 15))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    uint8_t record[HPCLANG_RPL_FLASH_CREATION_CLAIM_BYTES];
    source.first = claim_slot;
    for (unsigned i = 0; i < sizeof record; ++i) {
        uint8_t low, high;
        if (!read_nibble(&source, 2 * i, &low) || !read_nibble(&source, 2 * i + 1, &high))
            return HPCLANG_RPL_FLASH_FAILED;
        record[i] = low | high << 4;
    }
    enum hpclang_rpl_flash_creation_phase candidate;
    if (hpclang_rpl_flash_creation_inspect(io->first, io->end, claim_slot, record, sizeof record,
                                           expected_binding, name, name_length, &candidate))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct streamed_view view = {io, installed.target, journal, authority, transaction != 0};
    const struct hpclang_rpl_flash_io restored = {&view, streamed_read, NULL, io->first, io->end};
    struct object_reader mapped = {&restored, claim_slot & ~0x1ffffu};
    const struct hpclang_rpl_reader bank = {read_nibble, &mapped};
    uint32_t address = 2u * (claim_slot - mapped.first);
    struct hpclang_rpl_flash_allocation allocation;
    int result = hpclang_rpl_flash_allocation_find(&bank, 0x20au, 0x40000u, address, &allocation);
    if (result < 0)
        return installation_span_result(result);
    if (result != 1 || allocation.address != address || !allocation.deleted ||
        allocation.nibbles != 2u * sizeof record)
        return HPCLANG_RPL_FLASH_DIRTY;
    *phase = candidate;
    return 0;
}
