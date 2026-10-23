#include "rpl-flash-replay.h"
#include <stddef.h>
#include <string.h>

static uint16_t image_word(const uint8_t *image, uint32_t offset)
{
    return (uint16_t)(image[offset] | (uint16_t)image[offset + 1u] << 8u);
}

struct sector_overlay
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t target;
    const uint8_t *image;
};

static int overlay_read(void *context, uint32_t address, uint16_t *value)
{
    const struct sector_overlay *overlay = context;
    if (address >= overlay->target && address - overlay->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        *value = image_word(overlay->image, address - overlay->target);
        return 1;
    }
    return overlay->io->read16(overlay->io->context, address, value);
}

int hpclang_rpl_flash_preflight_sector(const struct hpclang_rpl_flash_io *io, uint8_t *scratch,
                                       uint32_t capacity, uint32_t *slot, uint32_t *target)
{
    if (!io || !io->read16 || !scratch || !slot || !target ||
        capacity < 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal = io->end, destination = 0;
    int damaged = 0, phase = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        uint32_t found, candidate_target;
        int result =
            hpclang_rpl_flash_find_sector(io, bank, scratch, capacity, &found, &candidate_target);
        if (result == HPCLANG_RPL_FLASH_DIRTY) {
            damaged = 1;
            continue;
        }
        if (result < 0)
            return result;
        if (result > 0) {
            if (journal != io->end)
                return HPCLANG_RPL_FLASH_DIRTY;
            journal = found;
            destination = candidate_target;
            phase = result;
        }
    }
    if (journal == io->end)
        return damaged ? HPCLANG_RPL_FLASH_DIRTY : 0;
    /* Later discovery may have overwritten scratch. Reload the selected image
     * and keep it separate from the validation scanner's workspace. */
    uint32_t found, candidate_target;
    int result = hpclang_rpl_flash_find_sector(io, journal & ~0x1ffffu, scratch, capacity, &found,
                                               &candidate_target);
    if (result != phase || found != journal || candidate_target != destination)
        return result < 0 ? result : HPCLANG_RPL_FLASH_DIRTY;
    if (phase == HPCLANG_RPL_FLASH_SECTOR_RETIRING) {
        for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
            uint16_t word;
            if (!io->read16(io->context, destination + offset, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (word != image_word(scratch + 24u, offset))
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    struct sector_overlay overlay = {io, destination, scratch + 24u};
    const struct hpclang_rpl_flash_io restored = {&overlay, overlay_read, NULL, io->first, io->end};
    unsigned journals = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        result = hpclang_rpl_flash_find_sector(
            &restored, bank, scratch + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
            HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &found, &candidate_target);
        if (result < 0)
            return result;
        if (result > 0) {
            if (result != phase || found != journal || candidate_target != destination ||
                ++journals != 1)
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    if (journals != 1)
        return HPCLANG_RPL_FLASH_DIRTY;
    *slot = journal;
    *target = destination;
    /* Normalize only the owned RAM copy for retire_sector's expected-record
     * contract. Durable status is unchanged; the returned phase controls use. */
    scratch[0] = 0x7eu;
    return phase;
}

struct bound_overlay
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t authority, journal, target, bank;
    const uint8_t *image;
    int hide_journal;
};

static int bound_read(void *context, uint32_t address, uint16_t *value)
{
    const struct bound_overlay *view = context;
    if (view->image && address >= view->target &&
        address - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES)
        *value = image_word(view->image, address - view->target);
    else if (!view->io->read16(view->io->context, address, value))
        return 0;
    if (address == (view->authority & ~1u))
        *value &= (uint16_t)~(255u << (8u * (view->authority & 1u)));
    if (view->hide_journal && address == (view->journal & ~1u))
        *value &= (uint16_t)~(255u << (8u * (view->journal & 1u)));
    return 1;
}

static int bound_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct bound_overlay *view = context;
    uint32_t physical = view->bank + address / 2u;
    uint16_t word;
    if (address >= 0x40000u || !bound_read(view, physical & ~1u, &word))
        return 0;
    *value = (uint8_t)((word >> (4u * (address & 3u))) & 15u);
    return 1;
}

static int load_bytes(const struct hpclang_rpl_flash_io *io, uint32_t address, uint8_t *data,
                      uint32_t bytes)
{
    uint16_t word = 0;
    for (uint32_t i = 0; i < bytes; ++i) {
        uint32_t at = address + i;
        if ((!i || !(at & 1u)) && !io->read16(io->context, at & ~1u, &word))
            return 0;
        data[i] = (uint8_t)(word >> (8u * (at & 1u)));
    }
    return 1;
}

static int bound_membership(struct bound_overlay *view)
{
    const struct hpclang_rpl_reader reader = {bound_nibble, view};
    struct hpclang_rpl_flash_allocation allocation;
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t slot = index ? view->journal : view->authority;
        uint32_t size =
            index ? HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES : HPCLANG_RPL_FLASH_AUTHORITY_BYTES;
        view->bank = slot & ~0x1ffffu;
        uint16_t identity;
        if (!bound_read(view, view->bank + 0x100u, &identity))
            return HPCLANG_RPL_FLASH_FAILED;
        if (identity != (0xf0u | ((8u + (view->bank - view->io->first) / 0x20000u) << 8u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint32_t address = (slot - view->bank) * 2u;
        int membership =
            hpclang_rpl_flash_allocation_find(&reader, 0x20au, 0x40000u, address, &allocation);
        if (membership != 1 || allocation.address != address || allocation.nibbles != 2u * size)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return 0;
}

int hpclang_rpl_flash_bound_membership(const struct hpclang_rpl_flash_io *io,
                                       uint32_t authority_slot, const uint8_t *authority,
                                       uint32_t authority_bytes, uint32_t journal_slot,
                                       const uint8_t *journal, uint32_t journal_bytes)
{
    if (!io || !io->read16 || (io->first & 0x1ffffu) || io->first > io->end ||
        io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    enum hpclang_rpl_flash_authority_phase phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    uint32_t target;
    const uint8_t *image;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            authority_bytes, journal_slot, journal, journal_bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, journal, journal_bytes,
                                         &target, &image, &journal_phase) ||
        phase == HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED ||
        (phase != HPCLANG_RPL_FLASH_AUTHORITY_ARMED &&
         journal_phase != HPCLANG_RPL_FLASH_SECTOR_RETIRED))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct bound_overlay view = {io,
                                 authority_slot,
                                 journal_slot,
                                 target,
                                 0,
                                 journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED ? NULL : image,
                                 1};
    return bound_membership(&view);
}

int hpclang_rpl_flash_preflight_bound(const struct hpclang_rpl_flash_io *io,
                                      uint32_t authority_slot, uint32_t journal_slot,
                                      uint8_t *scratch, uint32_t capacity, uint32_t *target)
{
    if (!io || !io->read16 || !scratch || !target ||
        capacity < 2u * HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u || authority_slot < io->first ||
        authority_slot >= io->end ||
        HPCLANG_RPL_FLASH_AUTHORITY_BYTES >= io->end - authority_slot || journal_slot < io->first ||
        journal_slot >= io->end || HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES >= io->end - journal_slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    const uint32_t bytes = HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
    if (!load_bytes(io, authority_slot, authority, sizeof authority) ||
        !load_bytes(io, journal_slot, scratch, bytes))
        return HPCLANG_RPL_FLASH_FAILED;
    enum hpclang_rpl_flash_authority_phase phase;
    enum hpclang_rpl_flash_sector_phase journal_phase;
    uint32_t destination;
    const uint8_t *image;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            sizeof authority, journal_slot, scratch, bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, scratch, bytes,
                                         &destination, &image, &journal_phase) ||
        phase == HPCLANG_RPL_FLASH_AUTHORITY_UNPUBLISHED ||
        (phase != HPCLANG_RPL_FLASH_AUTHORITY_ARMED &&
         journal_phase != HPCLANG_RPL_FLASH_SECTOR_RETIRED))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct bound_overlay view = {
        io, authority_slot, journal_slot, destination, authority_slot & ~0x1ffffu, NULL, 0};
    struct hpclang_rpl_flash_io masked = {&view, bound_read, NULL, io->first, io->end};
    /* The validated bound pair may live after the damaged bank header. Resolve
     * discovery against its restored view, keeping the trusted image separate
     * from scanner scratch. Retiring records still require actual target
     * equality before any overlay can hide a discrepancy. */
    if (journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRING) {
        for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
            uint16_t actual;
            if (!io->read16(io->context, destination + offset, &actual))
                return HPCLANG_RPL_FLASH_FAILED;
            if (actual != image_word(image, offset))
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    view.image = journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED ? NULL : image;
    uint32_t found = io->end, restored = 0;
    int result = 0;
    for (uint32_t bank = io->first; bank < io->end; bank += 0x20000u) {
        uint32_t candidate, candidate_target;
        int scanned = hpclang_rpl_flash_find_sector(&masked, bank, scratch + bytes, bytes,
                                                    &candidate, &candidate_target);
        if (scanned < 0)
            return scanned;
        if (scanned > 0) {
            if (result)
                return HPCLANG_RPL_FLASH_DIRTY;
            result = scanned;
            found = candidate;
            restored = candidate_target;
        }
    }
    if (journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED) {
        if (result != 0)
            return HPCLANG_RPL_FLASH_DIRTY;
    } else if (result <= 0 || result != (int)journal_phase || found != journal_slot ||
               restored != destination) {
        return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (!load_bytes(io, journal_slot, scratch, bytes))
        return HPCLANG_RPL_FLASH_FAILED;
    enum hpclang_rpl_flash_sector_phase reloaded_phase;
    uint32_t reloaded_target;
    if (hpclang_rpl_flash_authority_inspect(io->first, io->end, authority_slot, authority,
                                            sizeof authority, journal_slot, scratch, bytes,
                                            &phase) ||
        hpclang_rpl_flash_sector_inspect(io->first, io->end, journal_slot, scratch, bytes,
                                         &reloaded_target, &image, &reloaded_phase) ||
        reloaded_phase != journal_phase || reloaded_target != destination)
        return HPCLANG_RPL_FLASH_DIRTY;
    /* Resolve allocation membership on the restored view. Hide only the two
     * already validated transaction statuses; payload bytes stay unchanged. */
    view.hide_journal = 1;
    view.image = journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED ? NULL : image;
    int membership = bound_membership(&view);
    if (membership)
        return membership;
    if (phase == HPCLANG_RPL_FLASH_AUTHORITY_CLOSED)
        return 0;
    if (journal_phase == HPCLANG_RPL_FLASH_SECTOR_RETIRED) {
        for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_BYTES; i += 2u) {
            uint16_t word;
            if (!io->read16(io->context, destination + i, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (word != image_word(scratch + 24u, i))
                return HPCLANG_RPL_FLASH_DIRTY;
        }
        result = HPCLANG_RPL_FLASH_SECTOR_RETIRING;
    }
    scratch[0] = 0x7e;
    *target = destination;
    return result;
}

static uint32_t layout_word(const uint8_t *record, uint32_t at, unsigned count)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i, ++at)
        value |= (uint32_t)((record[at / 2u] >> (4u * (at & 1u))) & 15u) << (4u * i);
    return value;
}

static int layout_binding(const struct hpclang_rpl_flash_recovery_layout *layout,
                          uint32_t *journal_slot, uint32_t *authority_slot)
{
    if (!layout || !layout->record || layout->bytes < 16u || layout->bytes >= 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    const uint8_t *record = layout->record;
    uint32_t end = 13u + layout_word(record, 13u, 5u);
    if (layout_word(record, 8u, 5u) != 0x02b40u || end > 2u * layout->bytes || end < 36u)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t offsets[2] = {layout->recovery_binding, layout->admission_binding};
    uint32_t journal = 0, authority = 0;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t at = offsets[i];
        if (at < 18u || at > end - 36u || layout_word(record, at, 8u) != 0x42435048u ||
            layout_word(record, at + 8u, 8u) != 0x31444e49u)
            return HPCLANG_RPL_FLASH_INVALID;
        uint32_t j = layout_word(record, at + 16u, 8u);
        uint32_t a = layout_word(record, at + 24u, 8u);
        if (i &&
            (j != journal || a != authority || (offsets[0] < at + 32u && at < offsets[0] + 32u)))
            return HPCLANG_RPL_FLASH_INVALID;
        journal = j;
        authority = a;
    }
    *journal_slot = journal;
    *authority_slot = authority;
    return 0;
}

struct proposed_transaction
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t journal_slot, authority_slot, target;
    const uint8_t *journal, *authority, *image;
};

static int proposed_read(void *context, uint32_t address, uint16_t *value)
{
    const struct proposed_transaction *view = context;
    if (!view->io->read16(view->io->context, address, value))
        return 0;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t at = address + i;
        uint8_t byte;
        if (at >= view->journal_slot &&
            at - view->journal_slot < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) {
            uint32_t offset = at - view->journal_slot;
            byte = offset ? view->journal[offset] : 0;
        } else if (at >= view->authority_slot &&
                   at - view->authority_slot < HPCLANG_RPL_FLASH_AUTHORITY_BYTES) {
            uint32_t offset = at - view->authority_slot;
            byte = offset ? view->authority[offset] : 0;
        } else if (at >= view->target && at - view->target < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
            byte = view->image[at - view->target];
        } else {
            continue;
        }
        *value = (uint16_t)((*value & ~(255u << (8u * i))) | ((uint16_t)byte << (8u * i)));
    }
    return 1;
}

int hpclang_rpl_flash_plan_installed(const struct hpclang_rpl_flash_io *io,
                                     const struct hpclang_rpl_flash_recovery_layout *layout,
                                     const struct hpclang_rpl_flash_catalog *catalog,
                                     const uint8_t *journal, uint32_t journal_bytes,
                                     struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!io || !io->read16 || !installed)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t j, a, target;
    int result = layout_binding(layout, &j, &a);
    if (result)
        return result;
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_decode(io->first, io->end, j, journal, journal_bytes, &target,
                                        &image) ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, a, j, journal, journal_bytes,
                                           authority, sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t library;
    result =
        hpclang_rpl_flash_match_installation(io, layout->record, layout->bytes, catalog, &library);
    if (result)
        return result;
    if (library < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < library + layout->bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    /* Require untouched storage, not reusable tombstones or publication retries. */
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t slot = index ? a : j;
        uint32_t size = index ? sizeof authority : journal_bytes;
        for (uint32_t offset = 0; offset < size; ++offset) {
            uint32_t at = slot + offset;
            uint16_t word;
            if (!io->read16(io->context, at & ~1u, &word))
                return HPCLANG_RPL_FLASH_FAILED;
            if (((word >> (8u * (at & 1u))) & 255u) != 255u)
                return HPCLANG_RPL_FLASH_OCCUPIED;
        }
    }
    /* The proposed deleted envelopes test actual allocation boundaries without
     * asking ordinary Stock scanners to interpret unpublished private records.
     * No durable status is changed. A hole before either allocation is rejected
     * by the full erased-tail check, even if the requested bytes are all FF. */
    struct proposed_transaction view = {io, j, a, target, journal, authority, image};
    const struct hpclang_rpl_flash_io proposed = {&view, proposed_read, NULL, io->first, io->end};
    struct bound_overlay membership = {&proposed, a, j, target, 0, NULL, 1};
    result = bound_membership(&membership);
    if (result)
        return result;
    uint32_t matched;
    result = hpclang_rpl_flash_match_installation(&proposed, layout->record, layout->bytes, catalog,
                                                  &matched);
    if (result)
        return result;
    if (matched != library)
        return HPCLANG_RPL_FLASH_DIRTY;
    *installed = (struct hpclang_rpl_flash_installed_recovery){library, j, a, target};
    return 0;
}

int hpclang_rpl_flash_preflight_staged_installed(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    const struct hpclang_rpl_flash_catalog *catalog, uint8_t *scratch, uint32_t capacity,
    struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!io || !io->read16 || !installed || !scratch ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES || (io->first & 0x1ffffu) ||
        io->first > io->end || io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal, authority;
    int result = layout_binding(layout, &journal, &authority);
    if (result)
        return result;
    if (journal < io->first || journal >= io->end ||
        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES >= io->end - journal || authority < io->first ||
        authority >= io->end || HPCLANG_RPL_FLASH_AUTHORITY_BYTES >= io->end - authority)
        return HPCLANG_RPL_FLASH_INVALID;
    uint8_t actual[HPCLANG_RPL_FLASH_AUTHORITY_BYTES], expected[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!load_bytes(io, journal, scratch, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ||
        !load_bytes(io, authority, actual, sizeof actual))
        return HPCLANG_RPL_FLASH_FAILED;
    uint32_t target;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_decode(io->first, io->end, journal, scratch,
                                        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image) ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, authority, journal, scratch,
                                           HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, expected,
                                           sizeof expected))
        return HPCLANG_RPL_FLASH_DIRTY;
    expected[0] = 0x7f;
    if (memcmp(actual, expected, sizeof actual))
        return HPCLANG_RPL_FLASH_DIRTY;
    /* Mask only the two known statuses. The current target must validate
     * without a repair overlay: staging has not authorized any target write. */
    struct bound_overlay view = {io, authority, journal, target, 0, NULL, 1};
    result = bound_membership(&view);
    if (result)
        return result;
    const struct hpclang_rpl_flash_io masked = {&view, bound_read, NULL, io->first, io->end};
    uint32_t library;
    result = hpclang_rpl_flash_match_installation(&masked, layout->record, layout->bytes, catalog,
                                                  &library);
    if (result)
        return result;
    if (library < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < library + layout->bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    view.image = image;
    result = bound_membership(&view);
    if (result)
        return result;
    uint32_t proposed;
    result = hpclang_rpl_flash_match_installation(&masked, layout->record, layout->bytes, catalog,
                                                  &proposed);
    if (result)
        return result;
    if (proposed != library)
        return HPCLANG_RPL_FLASH_DIRTY;
    *installed = (struct hpclang_rpl_flash_installed_recovery){library, journal, authority, target};
    return 0;
}

struct cancellation_view
{
    const struct hpclang_rpl_flash_io *io;
    uint32_t slots[2];
    uint8_t headers[2][4];
};

static int cancellation_read(void *context, uint32_t address, uint16_t *value)
{
    const struct cancellation_view *view = context;
    if (!view->io->read16(view->io->context, address, value))
        return 0;
    for (unsigned half = 0; half < 2; ++half)
        for (unsigned index = 0; index < 2; ++index) {
            uint32_t at = address + half;
            if (at >= view->slots[index] && at - view->slots[index] < 4u) {
                uint8_t byte = view->headers[index][at - view->slots[index]];
                *value =
                    (uint16_t)((*value & ~(255u << (8u * half))) | ((uint16_t)byte << (8u * half)));
            }
        }
    return 1;
}

static int cancellation_slot(const struct hpclang_rpl_flash_io *io, uint32_t slot, uint32_t size)
{
    uint32_t offset = slot & 0x1ffffu;
    return slot >= io->first && slot < io->end && offset >= 0x105u && offset % 4u == 1u &&
           size < 0x20000u - offset;
}

static int cancellation_byte(const struct hpclang_rpl_flash_io *io, uint32_t at, uint8_t byte)
{
    uint16_t old, observed;
    unsigned shift = 8u * (at & 1u);
    if (!io->read16(io->context, at & ~1u, &old))
        return 0;
    uint16_t value = (uint16_t)((old & ~(255u << shift)) | (uint16_t)byte << shift);
    return (old & value) == value &&
           (old == value || io->program16(io->context, at & ~1u, value)) &&
           io->read16(io->context, at & ~1u, &observed) && observed == value;
}

static int cancellation_status(const uint8_t old[4], const uint8_t desired[4], unsigned metadata)
{
    /* FF may have a partially programmed extent. Once publication status was
     * attempted, the extent is complete; only cancellation can clear metadata. */
    if (old[0] == 0xffu)
        return 1;
    if (old[1] != desired[1] || old[2] != desired[2] || (old[3] & 15u) != desired[3])
        return 0;
    unsigned stored_metadata = old[3] >> 4u;
    if (old[0] == 0x7fu || old[0] == 0xfeu || old[0] == 0x7eu)
        return !(stored_metadata & ~metadata);
    /* Status clearing starts only after zero metadata has been verified.
     * Bit zero was removed first, preventing transient native status 53. */
    return !stored_metadata && !(old[0] & 1u);
}

static int commit_cancellation(const struct cancellation_view *view, const uint8_t original[2][4])
{
    const struct hpclang_rpl_flash_io *io = view->io;
    for (unsigned index = 0; index < 2; ++index) {
        uint8_t current[4];
        if (!load_bytes(io, view->slots[index], current, 4) || memcmp(current, original[index], 4))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t slot = view->slots[index];
        for (unsigned byte = 1; byte < 4; ++byte)
            if (!cancellation_byte(io, slot + byte, view->headers[index][byte]))
                return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t status = original[index][0];
        if ((status & 0x53u) == 0x53u && !cancellation_byte(io, slot, (uint8_t)(status & ~1u)))
            return HPCLANG_RPL_FLASH_DIRTY;
        if (!cancellation_byte(io, slot, 0))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t current[4];
        if (!load_bytes(io, slot, current, 4) || memcmp(current, view->headers[index], 4))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return 0;
}

int hpclang_rpl_flash_cancel_staged_pair(const struct hpclang_rpl_flash_io *io,
                                         uint32_t journal_slot, uint32_t authority_slot,
                                         const uint8_t *journal, uint32_t bytes)
{
    uint32_t target;
    const uint8_t *image;
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->read16 || !io->program16 ||
        !cancellation_slot(io, journal_slot, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES) ||
        !cancellation_slot(io, authority_slot, sizeof authority) ||
        hpclang_rpl_flash_sector_decode(io->first, io->end, journal_slot, journal, bytes, &target,
                                        &image) ||
        hpclang_rpl_flash_authority_encode(io->first, io->end, authority_slot, journal_slot,
                                           journal, bytes, authority, sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    authority[0] = 0x7f;
    const uint8_t *records[2] = {journal, authority};
    const uint32_t sizes[2] = {bytes, sizeof authority};
    struct cancellation_view view = {.io = io, .slots = {journal_slot, authority_slot}};
    uint8_t original[2][4];
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t extent = 2u * sizes[index] - 2u;
        view.headers[index][0] = 0;
        for (unsigned byte = 1; byte < 4; ++byte)
            view.headers[index][byte] = (uint8_t)(extent >> (8u * (byte - 1u)));
        if (!load_bytes(io, view.slots[index], original[index], 4))
            return HPCLANG_RPL_FLASH_DIRTY;
        uint8_t status = original[index][0], metadata = original[index][3] >> 4;
        if (original[index][1] != view.headers[index][1] ||
            original[index][2] != view.headers[index][2] ||
            (original[index][3] & 15u) != view.headers[index][3] ||
            (metadata & ~(index ? 4u : 3u)) ||
            (status != records[index][0] && (metadata || (status & ~0x7eu))))
            return HPCLANG_RPL_FLASH_DIRTY;
        if (index && (status != 0x7fu || metadata != 4u) && memcmp(original[0], view.headers[0], 4))
            return HPCLANG_RPL_FLASH_DIRTY;
        for (uint32_t at = 4; at < sizes[index]; ++at) {
            uint8_t actual;
            if (!load_bytes(io, view.slots[index] + at, &actual, 1) || actual != records[index][at])
                return HPCLANG_RPL_FLASH_DIRTY;
        }
    }
    /* An unarmed, same-owned cancellation must never conceal a changed target. */
    for (uint32_t at = 0; at < HPCLANG_RPL_FLASH_SECTOR_BYTES; at += 2) {
        uint16_t actual;
        if (!io->read16(io->context, target + at, &actual) || actual != image_word(image, at))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return commit_cancellation(&view, original);
}

int hpclang_rpl_flash_cancel_installed(const struct hpclang_rpl_flash_io *io,
                                       const struct hpclang_rpl_flash_recovery_layout *layout,
                                       const struct hpclang_rpl_flash_catalog *catalog)
{
    if (!io || !io->read16 || !io->program16 || (io->first & 0x1ffffu) || io->first > io->end ||
        io->end - io->first != 6u * 0x20000u)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal, authority;
    int result = layout_binding(layout, &journal, &authority);
    if (result)
        return result;
    const uint32_t sizes[2] = {HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES,
                               HPCLANG_RPL_FLASH_AUTHORITY_BYTES};
    if (!cancellation_slot(io, journal, sizes[0]) || !cancellation_slot(io, authority, sizes[1]) ||
        (journal < authority + sizes[1] && authority < journal + sizes[0]))
        return HPCLANG_RPL_FLASH_INVALID;
    struct cancellation_view view = {.io = io, .slots = {journal, authority}};
    uint8_t original[2][4];
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t extent = 2u * sizes[index] - 2u;
        view.headers[index][0] = 0;
        for (unsigned byte = 1; byte < 4; ++byte)
            view.headers[index][byte] = (uint8_t)(extent >> (8u * (byte - 1u)));
        if (!load_bytes(io, view.slots[index], original[index], 4))
            return HPCLANG_RPL_FLASH_DIRTY;
        for (unsigned byte = 1; byte < 4; ++byte)
            if ((original[index][byte] & view.headers[index][byte]) != view.headers[index][byte])
                return HPCLANG_RPL_FLASH_DIRTY;
        if (!cancellation_status(original[index], view.headers[index], index ? 4u : 3u))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    int journal_cancelled = !memcmp(original[0], view.headers[0], 4);
    uint8_t authority_status = original[1][0];
    if (authority_status != 0xffu && authority_status != 0x7fu && authority_status != 0xfeu) {
        if (!journal_cancelled || memcmp(original[1] + 1, view.headers[1] + 1, 3))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    /* A cleared required authority metadata bit is cancellation evidence only
     * after the journal barrier. Ordinary publication cannot clear this bit. */
    if (!(original[1][3] & 0x40u) && !journal_cancelled)
        return HPCLANG_RPL_FLASH_DIRTY;
    const struct hpclang_rpl_flash_io proposed = {&view, cancellation_read, NULL, io->first,
                                                  io->end};
    struct bound_overlay membership = {&proposed, authority, journal, 0, 0, NULL, 1};
    result = bound_membership(&membership);
    if (result)
        return result;
    uint32_t library;
    result = hpclang_rpl_flash_match_installation(&proposed, layout->record, layout->bytes, catalog,
                                                  &library);
    if (result)
        return result;
    /* Recheck both envelopes before the first write. The caller's exclusive
     * ownership keeps payloads, mappings and the installed identity stable. */
    result = commit_cancellation(&view, original);
    if (result)
        return result;
    uint32_t matched;
    result =
        hpclang_rpl_flash_match_installation(io, layout->record, layout->bytes, catalog, &matched);
    return !result && matched == library ? HPCLANG_RPL_FLASH_WRITTEN : HPCLANG_RPL_FLASH_DIRTY;
}

int hpclang_rpl_flash_preflight_installed(const struct hpclang_rpl_flash_io *io,
                                          const struct hpclang_rpl_flash_recovery_layout *layout,
                                          const struct hpclang_rpl_flash_catalog *catalog,
                                          uint8_t *scratch, uint32_t capacity,
                                          struct hpclang_rpl_flash_installed_recovery *installed)
{
    if (!installed)
        return HPCLANG_RPL_FLASH_INVALID;
    uint32_t journal, authority;
    int result = layout_binding(layout, &journal, &authority);
    if (result)
        return result;
    uint32_t target = 0;
    int phase =
        hpclang_rpl_flash_preflight_bound(io, authority, journal, scratch, capacity, &target);
    if (phase < 0)
        return phase;
    /* Closed preflight leaves target unchanged but retains the loaded journal.
     * Decode it to protect the installed allocation even during cleanup. */
    enum hpclang_rpl_flash_sector_phase journal_phase;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_inspect(io->first, io->end, journal, scratch,
                                         HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image,
                                         &journal_phase))
        return HPCLANG_RPL_FLASH_DIRTY;
    struct bound_overlay view = {io, authority, journal, target, 0, phase ? image : NULL, 1};
    const struct hpclang_rpl_flash_io restored = {&view, bound_read, NULL, io->first, io->end};
    uint32_t library;
    result = hpclang_rpl_flash_match_installation(&restored, layout->record, layout->bytes, catalog,
                                                  &library);
    if (result)
        return result;
    if (library < target + HPCLANG_RPL_FLASH_SECTOR_BYTES && target < library + layout->bytes)
        return HPCLANG_RPL_FLASH_DIRTY;
    *installed = (struct hpclang_rpl_flash_installed_recovery){library, journal, authority, target};
    return phase;
}

static int sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                         const uint8_t *expected_record, uint8_t *scratch, uint32_t capacity)
{
    if (!io || !io->flash.read16 || !io->flash.program16 || !io->erase_sector || !scratch ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    const struct hpclang_rpl_flash_io *flash = &io->flash;
    if ((flash->first & 0x1ffffu) || flash->first > flash->end ||
        flash->end - flash->first != 6u * 0x20000u || slot < flash->first || slot >= flash->end ||
        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES >= flash->end - slot)
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word = 0;
    for (uint32_t i = 0; i < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES; ++i) {
        uint32_t at = slot + i;
        if ((!i || !(at & 1u)) && !flash->read16(flash->context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        scratch[i] = (uint8_t)(word >> (8u * (at & 1u)));
    }
    if (expected_record && memcmp(expected_record, scratch, HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES))
        return HPCLANG_RPL_FLASH_DIRTY;
    uint32_t target;
    const uint8_t *image;
    if (hpclang_rpl_flash_sector_decode(flash->first, flash->end, slot, scratch,
                                        HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES, &target, &image))
        return HPCLANG_RPL_FLASH_INVALID;
    int matches = 1;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        if (!flash->read16(flash->context, target + offset, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        if (word != image_word(image, offset))
            matches = 0;
    }
    if (matches)
        return HPCLANG_RPL_FLASH_WRITTEN;
    if (!io->erase_sector(flash->context, target))
        return HPCLANG_RPL_FLASH_DIRTY;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        if (!flash->read16(flash->context, target + offset, &word) || word != UINT16_MAX)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    /* Keep a bank's identity erased until its first sector is complete. Stock
     * may rebuild its library catalog after a reset between program commands;
     * an early identity would expose a partly restored allocation chain. */
    uint32_t identity = (target & 0x1ffffu) ? HPCLANG_RPL_FLASH_SECTOR_BYTES : 0x100u;
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = image_word(image, offset);
        if (offset != identity && expected != UINT16_MAX &&
            !flash->program16(flash->context, target + offset, expected))
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    for (uint32_t offset = 0; offset < HPCLANG_RPL_FLASH_SECTOR_BYTES; offset += 2u) {
        uint16_t expected = offset == identity ? UINT16_MAX : image_word(image, offset);
        if (!flash->read16(flash->context, target + offset, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    if (identity < HPCLANG_RPL_FLASH_SECTOR_BYTES) {
        uint16_t expected = image_word(image, identity);
        if ((expected != UINT16_MAX &&
             !flash->program16(flash->context, target + identity, expected)) ||
            !flash->read16(flash->context, target + identity, &word) || word != expected)
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return HPCLANG_RPL_FLASH_WRITTEN;
}

int hpclang_rpl_flash_sector_replay(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                                    uint8_t *scratch, uint32_t capacity)
{
    return sector_replay(io, slot, NULL, scratch, capacity);
}

int hpclang_rpl_flash_sector_replay_expected(const struct hpclang_rpl_flash_replay_io *io,
                                             uint32_t slot, const uint8_t *expected,
                                             uint32_t expected_bytes, uint8_t *scratch,
                                             uint32_t capacity)
{
    if (!expected || expected_bytes != HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    uintptr_t a = (uintptr_t)expected, b = (uintptr_t)scratch;
    if ((a >= b ? a - b : b - a) < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES)
        return HPCLANG_RPL_FLASH_INVALID;
    return sector_replay(io, slot, expected, scratch, capacity);
}

int hpclang_rpl_flash_sector_replay_bound(const struct hpclang_rpl_flash_replay_io *io,
                                          uint32_t authority_slot, uint32_t journal_slot,
                                          const uint8_t *expected, uint32_t expected_bytes,
                                          uint8_t *scratch, uint32_t capacity)
{
    uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];
    if (!io || !io->flash.read16 || !io->flash.program16 || !io->erase_sector || !scratch ||
        capacity < HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES ||
        hpclang_rpl_flash_authority_encode(io->flash.first, io->flash.end, authority_slot,
                                           journal_slot, expected, expected_bytes, authority,
                                           sizeof authority))
        return HPCLANG_RPL_FLASH_INVALID;
    uint16_t word = 0;
    for (uint32_t i = 0; i < sizeof authority; ++i) {
        uint32_t at = authority_slot + i;
        if ((!i || !(at & 1u)) && !io->flash.read16(io->flash.context, at & ~1u, &word))
            return HPCLANG_RPL_FLASH_DIRTY;
        if ((uint8_t)(word >> (8u * (at & 1u))) != authority[i])
            return HPCLANG_RPL_FLASH_DIRTY;
    }
    return hpclang_rpl_flash_sector_replay_expected(io, journal_slot, expected, expected_bytes,
                                                    scratch, capacity);
}
