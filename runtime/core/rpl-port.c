#include <stddef.h>
#include "rpl-backup.h"
#include "rpl-port.h"

static int word(const struct hpclang_rpl_reader *reader, uint32_t at, uint32_t count,
                uint32_t *value)
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at + i, &nibble))
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (nibble > 15)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result |= (uint32_t)nibble << (4 * i);
    }
    *value = result;
    return 0;
}

int hpclang_rpl_catalog_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, hpclang_rpl_catalog_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t count;
    int result = word(reader, address, 3, &count);
    if (result)
        return result;
    if (count > (limit - 3) / 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = address + 3 + 16 * i, number;
        struct hpclang_rpl_library_binding binding;
        result = word(reader, record, 3, &number);
        if (result)
            return result;
        result = word(reader, record + 3, 5, &binding.address);
        if (result)
            return result;
        result = word(reader, record + 8, 5, &binding.bank_switch);
        if (result)
            return result;
        result = word(reader, record + 13, 3, &binding.flags);
        if (result)
            return result;
        result = visit(context, record, number, &binding);
        if (result)
            return result;
    }
    return 0;
}

struct catalog_rebind_search
{
    const struct hpclang_rpl_catalog_rebind *rebind;
    uint32_t field;
    int found;
};

static int catalog_rebind_visit(void *context, uint32_t record, uint32_t number,
                                const struct hpclang_rpl_library_binding *binding)
{
    struct catalog_rebind_search *search = context;
    if (number != search->rebind->number)
        return HPCLANG_RPL_SPAN_OK;
    if (search->found || binding->bank_switch != search->rebind->bank_switch)
        return HPCLANG_RPL_SPAN_MALFORMED;
    /* Match the atomic-nibble RAM writer's interrupted states. The semantic
     * ID and bank remain intact; a whole-word old/new comparison would reject
     * a valid restart between two nibble writes. */
    for (unsigned shift = 0; shift < 20; shift += 4) {
        uint32_t actual = (binding->address >> shift) & 15u;
        if (actual != ((search->rebind->previous >> shift) & 15u) &&
            actual != ((search->rebind->replacement >> shift) & 15u))
            return HPCLANG_RPL_SPAN_MALFORMED;
    }
    search->found = 1;
    search->field = record + 3u;
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_catalog_rebind_field(const struct hpclang_rpl_reader *reader, uint32_t address,
                                     uint32_t limit,
                                     const struct hpclang_rpl_catalog_rebind *rebind,
                                     uint32_t *field)
{
    if (!rebind || !field)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (rebind->number > 0xfffu || rebind->bank_switch > 0xfffffu || rebind->previous > 0xfffffu ||
        rebind->replacement > 0xfffffu)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct catalog_rebind_search search = {rebind, 0, 0};
    int result = hpclang_rpl_catalog_visit(reader, address, limit, catalog_rebind_visit, &search);
    if (result)
        return result;
    if (search.found)
        *field = search.field;
    return search.found;
}

int hpclang_rpl_catalog_plan_insert(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t limit, uint32_t number,
                                    struct hpclang_rpl_catalog_insertion *plan)
{
    if (!reader || !reader->read_nibble || !plan)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t count;
    int result = word(reader, address, 3, &count);
    if (result)
        return result;
    if (count > (limit - 3) / 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t end = address + 3 + count * 16, slot = end, previous = 0;
    int found = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = address + 3 + i * 16, id;
        result = word(reader, record, 3, &id);
        if (result)
            return result;
        if (i && id <= previous)
            return HPCLANG_RPL_SPAN_MALFORMED;
        previous = id;
        if (id == number)
            found = 1;
        if (id > number && slot == end)
            slot = record;
        /* Binding words belong to different mappings. Check their encoding
         * without treating them as main-RAM pointers or dereferencing them. */
        uint32_t ignored;
        result = word(reader, record + 3, 5, &ignored);
        if (!result)
            result = word(reader, record + 8, 5, &ignored);
        if (!result)
            result = word(reader, record + 13, 3, &ignored);
        if (result)
            return result;
    }
    if (found)
        return 0;
    if (count == 0xfff || end > 0x100000u - 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *plan = (struct hpclang_rpl_catalog_insertion){slot, end, count};
    return 1;
}

struct catalog_removal_search
{
    struct hpclang_rpl_catalog_removal plan;
    uint32_t number;
    uint32_t previous;
    int found;
};

int hpclang_rpl_home_library_attached(const struct hpclang_rpl_reader *reader, uint32_t home,
                                      uint32_t limit, uint32_t number)
{
    if (!reader || !reader->read_nibble)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (home < 0x80000u || home >= 0x100000u || limit > 0x100000u - home || limit < 8)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfffu)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t prologue, count;
    int result = word(reader, home, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x2a96u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, home + 5, 3, &count);
    if (result)
        return result;
    if (count > (limit - 8) / 13)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int found = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = home + 8 + 13 * i, id, ignored;
        result = word(reader, record, 3, &id);
        if (!result)
            result = word(reader, record + 3, 5, &ignored);
        if (!result)
            result = word(reader, record + 8, 5, &ignored);
        if (result)
            return result;
        found |= id == number;
    }
    return found;
}

static int catalog_removal_visit(void *context, uint32_t record, uint32_t number,
                                 const struct hpclang_rpl_library_binding *binding)
{
    struct catalog_removal_search *search = context;
    if (search->plan.count && number <= search->previous)
        return HPCLANG_RPL_SPAN_MALFORMED;
    search->previous = number;
    ++search->plan.count;
    search->plan.end = record + 16u;
    if (number == search->number) {
        search->found = 1;
        search->plan.slot = record;
        search->plan.binding = *binding;
    }
    return HPCLANG_RPL_SPAN_OK;
}

int hpclang_rpl_catalog_plan_remove(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t limit, uint32_t number,
                                    struct hpclang_rpl_catalog_removal *plan)
{
    if (!reader || !reader->read_nibble || !plan)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (number > 0xfffu)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct catalog_removal_search search = {.number = number};
    int result = hpclang_rpl_catalog_visit(reader, address, limit, catalog_removal_visit, &search);
    if (result)
        return result;
    if (search.found)
        *plan = search.plan;
    return search.found;
}

int hpclang_rpl_library_lookup(const struct hpclang_rpl_reader *reader, uint32_t address,
                               uint32_t limit, uint32_t number,
                               struct hpclang_rpl_library_binding *binding)
{
    if (!reader || !reader->read_nibble || !binding)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 3)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t count;
    int result = word(reader, address, 3, &count);
    if (result)
        return result;
    if (count > (limit - 3) / 16)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int found = 0;
    struct hpclang_rpl_library_binding next = {0};
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = address + 3 + 16 * i, id;
        result = word(reader, record, 3, &id);
        if (result)
            return result;
        if (id != number)
            continue;
        if (found)
            return HPCLANG_RPL_SPAN_MALFORMED;
        result = word(reader, record + 3, 5, &next.address);
        if (result)
            return result;
        result = word(reader, record + 8, 5, &next.bank_switch);
        if (result)
            return result;
        result = word(reader, record + 13, 3, &next.flags);
        if (result)
            return result;
        found = 1;
    }
    if (found)
        *binding = next;
    return found;
}

int hpclang_rpl_library_command(const struct hpclang_rpl_reader *reader, uint32_t first,
                                uint32_t end, uint32_t header, uint32_t number, uint32_t command,
                                uint32_t *target)
{
    if (!reader || !reader->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (end > 0x100000u || first >= end || header < first || header > end || end - header < 23)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfff || command > 0xfff)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t id, relative;
    int result = word(reader, header, 3, &id);
    if (result)
        return result;
    if (id != number)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, header + 13, 5, &relative);
    if (result || !relative)
        return result;
    uint32_t table = (header + 13 + relative) & 0xfffffu;
    if (table < first || table > end || end - table < 10)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue, length;
    result = word(reader, table, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x02a4e)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    result = word(reader, table + 5, 5, &length);
    if (result)
        return result;
    if (length < 5 || (length - 5) % 5)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (length > end - table - 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (command >= (length - 5) / 5)
        return 0;
    uint32_t field = table + 10 + 5 * command;
    result = word(reader, field, 5, &relative);
    if (result || !relative)
        return result;
    uint32_t resolved = (field + relative) & 0xfffffu;
    if (resolved < first || resolved >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = resolved;
    return 1;
}

int hpclang_rpl_library_attachment_resolve(const struct hpclang_rpl_reader *reader, uint32_t first,
                                           uint32_t end, uint32_t header, uint32_t number,
                                           struct hpclang_rpl_library_attachment_targets *targets)
{
    if (!reader || !reader->read_nibble || !targets)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (end > 0x100000u || first >= end || header < first || header > end || end - header < 23u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfffu)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t id;
    int result = word(reader, header, 3, &id);
    if (result)
        return result;
    if (id != number)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_library_attachment_targets next = {0};
    uint32_t *values[2] = {&next.hash, &next.messages};
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t field = header + 3 + 5 * i, relative;
        result = word(reader, field, 5, &relative);
        if (result)
            return result;
        if (!relative)
            continue;
        uint32_t target = (field + relative) & 0xfffffu;
        if (target < first || target >= end)
            return HPCLANG_RPL_SPAN_BOUNDS;
        next.present |= 1u << i;
        *values[i] = target;
    }
    *targets = next;
    return 1;
}

int hpclang_rpl_library_configuration(const struct hpclang_rpl_reader *reader, uint32_t first,
                                      uint32_t end, uint32_t header, uint32_t number,
                                      uint32_t *target)
{
    if (!reader || !reader->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (end > 0x100000u || first >= end || header < first || header > end || end - header < 23u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (number > 0xfffu)
        return HPCLANG_RPL_SPAN_MALFORMED;
    uint32_t id, relative;
    int result = word(reader, header, 3, &id);
    if (result)
        return result;
    if (id != number)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, header + 18u, 5, &relative);
    if (result || !relative)
        return result;
    uint32_t resolved = (header + 18u + relative) & 0xfffffu;
    if (resolved < first || resolved >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = resolved;
    return 1;
}

int hpclang_rpl_rom_bank_layout(const uint8_t *kib_sizes, uint32_t count, uint32_t directory,
                                uint32_t flash_bytes, const uint32_t native_bases[16],
                                uint32_t index, struct hpclang_rpl_rom_bank *bank)
{
    if (!kib_sizes || !native_bases || !bank)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!count || count > 16u || index >= 16u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (directory >= flash_bytes || count > flash_bytes - directory)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t at = directory + count;
    if (at & 1023u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    struct hpclang_rpl_rom_bank selected = {0};
    for (uint32_t i = 0; i < 16u; ++i) {
        if (i >= count) {
            if (native_bases[i])
                return HPCLANG_RPL_SPAN_MALFORMED;
            continue;
        }
        uint32_t bytes = (uint32_t)kib_sizes[i] * 1024u;
        if (!bytes || bytes > 0x20000u || native_bases[i] != at)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (bytes > flash_bytes - at)
            return HPCLANG_RPL_SPAN_BOUNDS;
        if (i == index)
            selected = (struct hpclang_rpl_rom_bank){at, bytes};
        at += bytes;
    }
    if (at != flash_bytes)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (index >= count)
        return 0;
    *bank = selected;
    return 1;
}

int hpclang_rpl_bank_lookup(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                            uint32_t eram_entry, uint32_t bank_switch,
                            struct hpclang_rpl_bank_binding *binding)
{
    if (!reader || !reader->read_nibble || !binding)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (flash_table > 0x100000u - 112 || eram_entry > 0x100000u - 5 || bank_switch >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    int found = 0;
    struct hpclang_rpl_bank_binding next = {0};
    for (uint32_t i = 0; i < 17; ++i) {
        uint32_t value;
        int result = word(reader, i < 16 ? flash_table + 7 * i : eram_entry, 5, &value);
        if (result)
            return result;
        if (!value)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (value == bank_switch) {
            if (found)
                return HPCLANG_RPL_SPAN_MALFORMED;
            next.kind = i < 16 ? HPCLANG_RPL_BANK_FLASH : HPCLANG_RPL_BANK_ERAM;
            next.index = i < 16 ? i : 0;
            found = 1;
        }
    }
    if (found)
        *binding = next;
    return found;
}

struct bank_lookup_observer
{
    const struct hpclang_rpl_reader *reader;
    hpclang_rpl_bank_dependency_fn dependency;
    void *context;
    int error;
};

static int bank_lookup_read(void *context, uint32_t address, uint8_t *value)
{
    struct bank_lookup_observer *observer = context;
    if (observer->error ||
        !observer->reader->read_nibble(observer->reader->context, address, value))
        return 0;
    if (*value <= 15u) {
        observer->error = observer->dependency(observer->context, address, *value);
        if (observer->error > 0)
            observer->error = HPCLANG_RPL_SPAN_MALFORMED;
        if (observer->error)
            return 0;
    }
    return 1;
}

int hpclang_rpl_library_configuration_guarded(const struct hpclang_rpl_reader *reader,
                                              uint32_t first, uint32_t end, uint32_t header,
                                              uint32_t number, uint32_t *target,
                                              hpclang_rpl_bank_dependency_fn dependency,
                                              void *context)
{
    if (!reader || !reader->read_nibble || !target || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {reader, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    uint32_t next = 0;
    int result = hpclang_rpl_library_configuration(&observed, first, end, header, number, &next);
    if (observer.error)
        return observer.error;
    if (result == 1)
        *target = next;
    return result;
}

static int rom_bank_capture(const struct hpclang_rpl_reader *rom,
                            const struct hpclang_rpl_reader *state, uint32_t operands,
                            uint32_t bases_address, uint32_t flash_bytes,
                            struct hpclang_rpl_rom_bank banks[16])
{
    uint32_t metadata_end = flash_bytes < 0x80000u ? flash_bytes : 0x80000u;
    if (metadata_end < 8 || operands > metadata_end - 8u || bases_address > 0x80000u - 64u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t count_address, directory, count;
    int result = word(rom, operands * 2u, 8, &count_address);
    if (result)
        return result;
    result = word(rom, (operands + 4u) * 2u, 8, &directory);
    if (result)
        return result;
    if (count_address >= metadata_end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = word(rom, count_address * 2u, 2, &count);
    if (result)
        return result;
    if (!count || count > 16u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (directory >= metadata_end || count > metadata_end - directory)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint8_t sizes[16];
    uint32_t bases[16];
    for (unsigned i = 0; i < count; ++i) {
        uint32_t size;
        result = word(rom, (directory + i) * 2u, 2, &size);
        if (result)
            return result;
        sizes[i] = (uint8_t)size;
    }
    for (unsigned i = 0; i < 16; ++i) {
        result = word(state, (bases_address + 4u * i) * 2u, 8, &bases[i]);
        if (result)
            return result;
    }
    for (unsigned i = 0; i < 16; ++i) {
        result =
            hpclang_rpl_rom_bank_layout(sizes, count, directory, flash_bytes, bases, i, &banks[i]);
        if (result < 0)
            return result;
    }
    return 0;
}

int hpclang_rpl_rom_bank_capture_guarded(
    const struct hpclang_rpl_reader *rom, const struct hpclang_rpl_reader *state, uint32_t operands,
    uint32_t bases_address, uint32_t flash_bytes, struct hpclang_rpl_rom_bank *banks,
    hpclang_rpl_bank_dependency_fn rom_dependency, void *rom_context,
    hpclang_rpl_bank_dependency_fn state_dependency, void *state_context)
{
    if (!rom || !rom->read_nibble || !state || !state->read_nibble || !banks || !rom_dependency ||
        !state_dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer ro = {rom, rom_dependency, rom_context, 0};
    struct bank_lookup_observer so = {state, state_dependency, state_context, 0};
    const struct hpclang_rpl_reader observed_rom = {bank_lookup_read, &ro};
    const struct hpclang_rpl_reader observed_state = {bank_lookup_read, &so};
    struct hpclang_rpl_rom_bank next[16] = {0};
    int result = rom_bank_capture(&observed_rom, &observed_state, operands, bases_address,
                                  flash_bytes, next);
    if (ro.error || so.error)
        return ro.error ? ro.error : so.error;
    if (!result)
        for (unsigned i = 0; i < 16; ++i)
            banks[i] = next[i];
    return result;
}

int hpclang_rpl_bank_lookup_guarded(const struct hpclang_rpl_reader *reader, uint32_t flash_table,
                                    uint32_t eram_entry, uint32_t bank_switch,
                                    struct hpclang_rpl_bank_binding *binding,
                                    hpclang_rpl_bank_dependency_fn dependency, void *context)
{
    if (!reader || !reader->read_nibble || !binding || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {reader, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    struct hpclang_rpl_bank_binding next;
    int result = hpclang_rpl_bank_lookup(&observed, flash_table, eram_entry, bank_switch, &next);
    if (observer.error)
        return observer.error;
    if (result == 1)
        *binding = next;
    return result;
}

int hpclang_rpl_bank_alias_lookup_guarded(const struct hpclang_rpl_reader *reader,
                                          uint32_t flash_table, uint32_t eram_entry,
                                          uint32_t requested_switch, uint32_t alias,
                                          uint32_t canonical,
                                          const struct hpclang_rpl_bank_identity_range *ranges,
                                          uint32_t range_count,
                                          struct hpclang_rpl_bank_binding *binding,
                                          hpclang_rpl_bank_dependency_fn dependency, void *context)
{
    if (!reader || !reader->read_nibble || !ranges || !range_count || !binding || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (requested_switch >= 0x100000u || alias >= 0x100000u || canonical >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (requested_switch != alias)
        return 0;
    unsigned covered = 0;
    for (uint32_t i = 0; i < range_count; ++i) {
        const struct hpclang_rpl_bank_identity_range *range = &ranges[i];
        if (!range->nibbles)
            return HPCLANG_RPL_SPAN_UNAVAILABLE;
        if (!range->length || range->address >= 0x100000u ||
            range->length > 0x100000u - range->address)
            return HPCLANG_RPL_SPAN_BOUNDS;
        if (alias >= range->address && alias - range->address < range->length)
            covered |= 1;
        if (canonical >= range->address && canonical - range->address < range->length)
            covered |= 2;
        for (uint32_t j = 0; j < range->length; ++j)
            if (range->nibbles[j] > 15u)
                return HPCLANG_RPL_SPAN_MALFORMED;
    }
    if (covered != 3)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    for (uint32_t i = 0; i < range_count; ++i) {
        const struct hpclang_rpl_bank_identity_range *range = &ranges[i];
        for (uint32_t j = 0; j < range->length; ++j) {
            uint8_t value;
            if (!reader->read_nibble(reader->context, range->address + j, &value))
                return HPCLANG_RPL_SPAN_UNAVAILABLE;
            if (value > 15u)
                return HPCLANG_RPL_SPAN_MALFORMED;
            if (value != range->nibbles[j])
                return HPCLANG_RPL_SPAN_UNSUPPORTED;
            int result = dependency(context, range->address + j, value);
            if (result)
                return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        }
    }
    return hpclang_rpl_bank_lookup_guarded(reader, flash_table, eram_entry, canonical, binding,
                                           dependency, context);
}

int hpclang_rpl_acptr_resolve(const struct hpclang_rpl_reader *source, uint32_t address,
                              uint32_t limit, const struct hpclang_rpl_reader *rom,
                              uint32_t flash_table, uint32_t eram_entry,
                              struct hpclang_rpl_banked_target *target)
{
    if (!source || !source->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 15u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue, pointer, bank_switch;
    int result = word(source, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x02baau)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    result = word(source, address + 5u, 5, &pointer);
    if (result)
        return result;
    if (pointer < 0x40000u || pointer >= 0x80000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    result = word(source, address + 10u, 5, &bank_switch);
    if (result)
        return result;
    struct hpclang_rpl_banked_target next;
    result = hpclang_rpl_bank_lookup(rom, flash_table, eram_entry, bank_switch, &next.bank);
    if (result != 1)
        return result;
    next.offset = pointer - 0x40000u;
    *target = next;
    return 1;
}

int hpclang_rpl_acptr_resolve_guarded(
    const struct hpclang_rpl_reader *source, uint32_t address, uint32_t limit,
    const struct hpclang_rpl_reader *rom, uint32_t flash_table, uint32_t eram_entry,
    struct hpclang_rpl_banked_target *target, hpclang_rpl_bank_dependency_fn source_dependency,
    void *source_context, hpclang_rpl_bank_dependency_fn rom_dependency, void *rom_context)
{
    if (!source || !source->read_nibble || !rom || !rom->read_nibble || !target ||
        !source_dependency || !rom_dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer source_observer = {source, source_dependency, source_context, 0};
    struct bank_lookup_observer rom_observer = {rom, rom_dependency, rom_context, 0};
    const struct hpclang_rpl_reader observed_source = {bank_lookup_read, &source_observer};
    const struct hpclang_rpl_reader observed_rom = {bank_lookup_read, &rom_observer};
    struct hpclang_rpl_banked_target next;
    int result = hpclang_rpl_acptr_resolve(&observed_source, address, limit, &observed_rom,
                                           flash_table, eram_entry, &next);
    if (source_observer.error)
        return source_observer.error;
    if (rom_observer.error)
        return rom_observer.error;
    if (result == 1)
        *target = next;
    return result;
}

int hpclang_rpl_flashptr_decode(const struct hpclang_rpl_reader *source, uint32_t address,
                                uint32_t limit, struct hpclang_rpl_flashptr *pointer)
{
    if (!source || !source->read_nibble || !pointer)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 12u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue;
    int result = word(source, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x026acu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_flashptr next;
    result = word(source, address + 5u, 3, &next.selector);
    if (result)
        return result;
    result = word(source, address + 8u, 4, &next.index);
    if (result)
        return result;
    *pointer = next;
    return 1;
}

int hpclang_rpl_flashptr_lookup(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                uint32_t bank, uint32_t index, uint32_t *target)
{
    if (!mapped || !mapped->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (header < 0x40000u || header > 0x80000u - 34u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (bank >= 16u || index > 0xffffu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t identity;
    int result = word(mapped, header, 4, &identity);
    if (result)
        return result;
    if (identity != (0x0fu | (bank << 8u)))
        return HPCLANG_RPL_SPAN_MALFORMED;
    return hpclang_rpl_flashptr_table_lookup(mapped, 0x40000u, 0x80000u, header + 29u, index,
                                             target);
}

int hpclang_rpl_flashptr_table_lookup(const struct hpclang_rpl_reader *mapped, uint32_t first,
                                      uint32_t end, uint32_t table, uint32_t index,
                                      uint32_t *target)
{
    if (!mapped || !mapped->read_nibble || !target)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (first < 0x40000u || end > 0x80000u || first >= end || table < first || table > end ||
        end - table < 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (index > 0xffffu)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t count;
    int result = word(mapped, table, 5, &count);
    if (result)
        return result;
    if (count > 0x10000u || count > (end - table - 5u) / 5u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (index >= count)
        return 0;
    uint32_t value;
    result = word(mapped, table + 5u + 5u * index, 5, &value);
    if (result || !value)
        return result;
    if (value >= 0x80000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = value;
    return 1;
}

int hpclang_rpl_flashptr_bank(const struct hpclang_rpl_reader *reader, uint32_t selectors,
                              uint32_t banks, uint32_t selector, uint32_t *bank)
{
    if (!reader || !reader->read_nibble || !bank)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (selectors > 0x100000u - 80u || banks > 0x100000u - 80u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (selector >= 16u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t configuration;
    int result = word(reader, selectors + 5u * selector, 5, &configuration);
    if (result || !configuration)
        return result;
    uint32_t selected = 0;
    int found = 0;
    for (uint32_t i = 0; i < 16u; ++i) {
        uint32_t candidate;
        result = word(reader, banks + 5u * i, 5, &candidate);
        if (result)
            return result;
        if (candidate == configuration) {
            if (found)
                return HPCLANG_RPL_SPAN_MALFORMED;
            selected = i;
            found = 1;
        }
    }
    if (found)
        *bank = selected;
    return found;
}

int hpclang_rpl_flashptr_decode_guarded(const struct hpclang_rpl_reader *source, uint32_t address,
                                        uint32_t limit, struct hpclang_rpl_flashptr *pointer,
                                        hpclang_rpl_bank_dependency_fn dependency, void *context)
{
    if (!source || !source->read_nibble || !pointer || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {source, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    struct hpclang_rpl_flashptr next = {UINT32_MAX, UINT32_MAX};
    int result = hpclang_rpl_flashptr_decode(&observed, address, limit, &next);
    if (observer.error)
        return observer.error;
    if (result == 1 && (next.selector > 0xfffu || next.index > 0xffffu))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (result == 1)
        *pointer = next;
    return result;
}

int hpclang_rpl_flashptr_table_lookup_guarded(const struct hpclang_rpl_reader *mapped,
                                              uint32_t first, uint32_t end, uint32_t table,
                                              uint32_t index, uint32_t *target,
                                              hpclang_rpl_bank_dependency_fn dependency,
                                              void *context)
{
    if (!mapped || !mapped->read_nibble || !target || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {mapped, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    uint32_t next = UINT32_MAX;
    int result = hpclang_rpl_flashptr_table_lookup(&observed, first, end, table, index, &next);
    if (observer.error)
        return observer.error;
    if (result == 1 && (!next || next >= 0x80000u))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (result == 1)
        *target = next;
    return result;
}

int hpclang_rpl_flashptr_bank_guarded(const struct hpclang_rpl_reader *reader, uint32_t selectors,
                                      uint32_t banks, uint32_t selector, uint32_t *bank,
                                      hpclang_rpl_bank_dependency_fn dependency, void *context)
{
    if (!reader || !reader->read_nibble || !bank || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {reader, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    uint32_t next = UINT32_MAX;
    int result = hpclang_rpl_flashptr_bank(&observed, selectors, banks, selector, &next);
    if (observer.error)
        return observer.error;
    if (result == 1 && next >= 16u)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (result == 1)
        *bank = next;
    return result;
}

int hpclang_rpl_flashptr_lookup_guarded(const struct hpclang_rpl_reader *mapped, uint32_t header,
                                        uint32_t bank, uint32_t index, uint32_t *target,
                                        hpclang_rpl_bank_dependency_fn dependency, void *context)
{
    if (!mapped || !mapped->read_nibble || !target || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct bank_lookup_observer observer = {mapped, dependency, context, 0};
    const struct hpclang_rpl_reader observed = {bank_lookup_read, &observer};
    uint32_t next = UINT32_MAX;
    int result = hpclang_rpl_flashptr_lookup(&observed, header, bank, index, &next);
    if (observer.error)
        return observer.error;
    if (result == 1 && (!next || next >= 0x80000u))
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (result == 1)
        *target = next;
    return result;
}

static int library(const struct hpclang_rpl_reader *reader, uint32_t limit,
                   struct hpclang_rpl_port_entry *entry, uint32_t *name_address)
{
    int result = hpclang_rpl_object_span(reader, entry->address, limit, &entry->span);
    if (result)
        return result;
    /* Length, title prefix/suffix, library ID, four relative table offsets, CRC. */
    if (entry->span < 41)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, entry->address + 10, 2, &entry->name_length);
    if (result)
        return result;
    if (entry->name_length * 2 > entry->span - 41)
        return HPCLANG_RPL_SPAN_MALFORMED;
    *name_address = entry->address + 12;
    uint32_t repeated;
    result = word(reader, *name_address + entry->name_length * 2, 2, &repeated);
    if (result)
        return result;
    if (repeated != entry->name_length)
        return HPCLANG_RPL_SPAN_MALFORMED;
    result = word(reader, *name_address + entry->name_length * 2 + 2, 3, &entry->library_number);
    if (result)
        return result;
    uint32_t crc = 0;
    for (uint32_t offset = 5; offset < entry->span - 4; ++offset) {
        uint32_t nibble;
        result = word(reader, entry->address + offset, 1, &nibble);
        if (result)
            return result;
        crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081);
    }
    uint32_t stored_crc;
    result = word(reader, entry->address + entry->span - 4, 4, &stored_crc);
    if (result)
        return result;
    if (stored_crc != crc)
        return HPCLANG_RPL_SPAN_MALFORMED;
    entry->value_address = entry->address;
    entry->value_span = entry->span;
    return 0;
}

static int library_target(const struct hpclang_rpl_reader *reader, uint32_t field, uint32_t first,
                          uint32_t end, uint32_t *target)
{
    uint32_t relative;
    int result = word(reader, field, 5, &relative);
    if (result)
        return result;
    if (!relative) {
        *target = 0;
        return 0;
    }
    uint32_t resolved = (field + relative) & 0xfffffu;
    if (resolved < first || resolved >= end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    *target = resolved;
    return 0;
}

int hpclang_rpl_library_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t limit, hpclang_rpl_library_visit_fn visit, void *context)
{
    if (!reader || !reader->read_nibble || !visit)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (address >= 0x100000u || limit > 0x100000u - address || limit < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t prologue;
    int result = word(reader, address, 5, &prologue);
    if (result)
        return result;
    if (prologue != 0x2b40u)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    struct hpclang_rpl_port_entry entry = {.address = address};
    uint32_t name;
    result = library(reader, limit, &entry, &name);
    if (result)
        return result;
    uint32_t fields = name + entry.name_length * 2 + 5;
    uint32_t first = fields + 20, end = address + entry.span - 4;
    uint32_t targets[4];
    for (unsigned i = 0; i < 4; ++i) {
        result = library_target(reader, fields + i * 5, first, end, &targets[i]);
        if (result)
            return result;
    }
    uint32_t links_end = 0;
    if (targets[2]) {
        uint32_t type, span;
        if (end - targets[2] < 10)
            return HPCLANG_RPL_SPAN_BOUNDS;
        result = word(reader, targets[2], 5, &type);
        if (result)
            return result;
        if (type != 0x2a4eu)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        result = hpclang_rpl_object_span(reader, targets[2], end - targets[2], &span);
        if (result)
            return result;
        if ((span - 10) % 5)
            return HPCLANG_RPL_SPAN_MALFORMED;
        links_end = targets[2] + span;
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (targets[i]) {
            result = visit(context, (enum hpclang_rpl_library_reference_kind)i, fields + i * 5,
                           targets[i], end - targets[i]);
            if (result)
                return result;
        }
    }
    for (uint32_t field = targets[2] ? targets[2] + 10 : 0; field < links_end; field += 5) {
        uint32_t target;
        result = library_target(reader, field, first, end, &target);
        if (result)
            return result;
        if (target) {
            result = visit(context, HPCLANG_RPL_LIBRARY_FUNCTION, field, target, end - target);
            if (result)
                return result;
        }
    }
    return 0;
}

int hpclang_rpl_port_next(const struct hpclang_rpl_reader *reader,
                          struct hpclang_rpl_port_cursor *cursor,
                          struct hpclang_rpl_port_entry *entry)
{
    if (!reader || !reader->read_nibble || !cursor || !entry)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (cursor->end > 0x100000 || cursor->address > cursor->end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (cursor->finished)
        return 0;
    uint32_t limit = cursor->end - cursor->address;
    if (limit < 5)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_port_entry next = {.address = cursor->address};
    int result = word(reader, next.address, 5, &next.prologue);
    if (result)
        return result;
    if (!next.prologue) {
        cursor->finished = 1;
        return 0;
    }
    uint32_t name_address;
    if (next.prologue == 0x02b62) {
        struct hpclang_rpl_backup_view backup;
        result = hpclang_rpl_backup_view(reader, next.address, limit, &backup);
        if (result)
            return result;
        next.span = backup.span;
        next.value_address = backup.value_address;
        next.value_span = backup.value_span;
        next.name_length = backup.name_length;
        name_address = backup.name_address;
    } else if (next.prologue == 0x02b40) {
        result = library(reader, limit, &next, &name_address);
        if (result)
            return result;
    } else {
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    }
    for (uint32_t i = 0; i < next.name_length; ++i) {
        uint32_t byte;
        result = word(reader, name_address + 2 * i, 2, &byte);
        if (result)
            return result;
        next.name[i] = (uint8_t)byte;
    }
    cursor->address += next.span;
    *entry = next;
    return 1;
}

int hpclang_rpl_port_find(const struct hpclang_rpl_reader *reader, uint32_t first, uint32_t end,
                          uint32_t target, struct hpclang_rpl_port_entry *entry)
{
    if (!entry)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (target >= 0x100000u)
        return HPCLANG_RPL_SPAN_BOUNDS;
    struct hpclang_rpl_port_cursor cursor = {first, end, 0};
    struct hpclang_rpl_port_entry current, match;
    int result, found = 0;
    while ((result = hpclang_rpl_port_next(reader, &cursor, &current)) > 0) {
        if (target >= current.address && target - current.address < current.span) {
            match = current;
            found = 1;
        }
    }
    if (result)
        return result;
    if (found)
        *entry = match;
    return found;
}

int hpclang_rpl_flash_next(const struct hpclang_rpl_reader *reader,
                           struct hpclang_rpl_port_cursor *cursor,
                           struct hpclang_rpl_port_entry *entry)
{
    if (!reader || !reader->read_nibble || !cursor || !entry)
        return HPCLANG_RPL_SPAN_MALFORMED;
    if (cursor->end > 0x100000u || cursor->address > cursor->end)
        return HPCLANG_RPL_SPAN_BOUNDS;
    if (cursor->finished)
        return 0;
    struct hpclang_rpl_port_cursor scan = *cursor;
    for (;;) {
        uint32_t available = scan.end - scan.address;
        if (available < 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t status;
        int result = word(reader, scan.address, 2u, &status);
        if (result)
            return result;
        if (status == 0xffu) {
            scan.finished = 1;
            *cursor = scan;
            return 0;
        }
        if (status != 0x53u && status != 0u)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        if (available < 8u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        uint32_t extent;
        result = word(reader, scan.address + 2u, 5u, &extent);
        if (result)
            return result;
        /* Extent is relative to its own field, not the slot or object prologue.
         * The final metadata nibble is not part of this size. */
        if (extent < 11u)
            return HPCLANG_RPL_SPAN_MALFORMED;
        if (extent > available - 2u)
            return HPCLANG_RPL_SPAN_BOUNDS;
        if (status == 0u) {
            /* PURGE hides the entry from enumeration. Existing references can
             * still target its retained payload; this is not a reclaim test. */
            scan.address += 2u + extent;
            continue;
        }
        struct hpclang_rpl_port_cursor object = {scan.address + 8u, scan.address + 2u + extent, 0};
        struct hpclang_rpl_port_entry next;
        result = hpclang_rpl_port_next(reader, &object, &next);
        if (result != 1)
            return result < 0 ? result : HPCLANG_RPL_SPAN_MALFORMED;
        scan.address += 2u + extent;
        *cursor = scan;
        *entry = next;
        return 1;
    }
}
