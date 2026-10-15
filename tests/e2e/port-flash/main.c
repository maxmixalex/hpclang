#include "stock-flash.h"
#include "rpl-flash-record.h"
#include "rpl-stock-memory.h"
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <hpclang/object.h>
#include <hpclang/port.h>

volatile uint32_t hpclang_e2e_result;
struct hpclang_sst_device hpclang_flash_probe_device;
static uint8_t block[512];
static uint8_t saved_sector[4096];

/* Independent native HP50g STO capture: backup FLASHONE contains integer 987. */
static const uint8_t native_record[] = {
    0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46, 0x4c, 0x41, 0x53, 0x48, 0x4f,
    0x4e, 0x45, 0x08, 0x14, 0x26, 0x90, 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};

uint16_t hpclang_flash_read16(void *context, uint32_t address);

static unsigned plan_program_calls;

static int plan_read(void *context, uint32_t address, uint16_t *value)
{
    *value = hpclang_flash_read16(context, address);
    return 1;
}

static int plan_program(void *context, uint32_t address, uint16_t value)
{
    (void)context;
    (void)address;
    (void)value;
    ++plan_program_calls;
    return 0;
}

static int intent_program(void *context, uint32_t address, uint16_t value)
{
    (void)context;
    return hpclang_stock_flash_program_word(address, value) == HPCLANG_SST_OK;
}

static int packed_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint8_t *bytes = context;
    *value = (bytes[address / 2u] >> ((address & 1u) * 4u)) & 15u;
    return 1;
}

static uint8_t *replacement_record(uint32_t *bytes)
{
    uint8_t value[] = {0x14, 0x26, 0x90, 0, 0, 0x86, 0x09}; /* ZINT 986. */
    const struct hpclang_rpl_reader source = {packed_nibble, value};
    uint8_t *backup = NULL, *replacement = NULL;
    uint32_t span;
    if (!hpclang_rpl_backup_encode(&source, 0, 14u, (const uint8_t *)"FLASHONE", 8u, &backup,
                                   &span)) {
        const struct hpclang_rpl_reader encoded = {packed_nibble, backup};
        (void)hpclang_rpl_flash_backup_record(&encoded, 0, span, &replacement, bytes);
    }
    free(backup);
    return replacement;
}

static uint32_t fingerprint(void)
{
    uint32_t value = 2166136261u;
    for (uint32_t address = 0; address < 0x200000u; address += 2u)
        value = (value ^ hpclang_flash_read16(0, address)) * 16777619u;
    return value;
}

static uint32_t unused_sector(void)
{
    /* Only the disposable Stock test image is eligible: require a completely
     * unused bank tail, not merely an all-FF sector inside a live object. */
    for (uint32_t bank = 0x140000u; bank < 0x200000u; bank += 0x20000u) {
        uint32_t index = (bank - 0x140000u) / 0x20000u;
        if (hpclang_flash_read16(0, bank + 0x100u) != (0xf0u | ((index + 8u) << 8u)))
            return 0;
        struct hpclang_rpl_reader reader;
        struct hpclang_rpl_flash_space space;
        if (!hpclang_rpl_stock_port2_reader(index, &reader) ||
            hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space))
            return 0;
        if (space.address != 0x20au)
            continue;
        if ((hpclang_flash_read16(0, bank + 0x104u) & 0xff00u) != 0xff00u)
            continue;
        uint32_t address;
        for (address = bank + 0x106u; address < bank + 0x20000u; address += 2u)
            if (hpclang_flash_read16(0, address) != UINT16_MAX)
                break;
        if (address == bank + 0x20000u)
            return bank + 0x1f000u;
    }
    return 0;
}

int main(void)
{
    const volatile uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    uint32_t original[2] = {l1[0], l1[1]};
    uint32_t before = fingerprint();
    unsigned failure = 1;
    for (unsigned pass = 0; pass < 3; ++pass) {
        if (hpclang_stock_flash_identify(&hpclang_flash_probe_device) != HPCLANG_SST_OK)
            goto done;
        failure = 2;
        if (hpclang_flash_probe_device.bytes != 0x200000u ||
            hpclang_flash_probe_device.sector_bytes != 4096u)
            goto done;
        failure = 3;
        if (l1[0] != original[0] || l1[1] != original[1])
            goto done;
        failure = 4;
        if (fingerprint() != before)
            goto done;
        failure = 1;
    }
    failure = 5;
    uint32_t scratch = unused_sector();
    if (!scratch)
        goto done;
    failure = 6;
    if (hpclang_stock_flash_program_word(0x13fffeu, 0) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_erase_sector(0x13f000u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program_word(0x200000u, 0) != HPCLANG_SST_INVALID)
        goto done;
    failure = 7;
    if (hpclang_stock_flash_program_word(scratch + 2u, 0x1234u) != HPCLANG_SST_OK ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u ||
        hpclang_flash_read16(0, scratch) != UINT16_MAX)
        goto done;
    failure = 8;
    struct hpclang_rpl_reader reader;
    struct hpclang_rpl_flash_space space = {123, 456};
    if (!hpclang_rpl_stock_port2_reader((scratch - 0x140000u) / 0x20000u, &reader) ||
        hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) != HPCLANG_RPL_SPAN_MALFORMED ||
        space.address != 123 || space.nibbles != 456)
        goto done;
    if (hpclang_stock_flash_program_word(scratch + 2u, UINT16_MAX) != HPCLANG_SST_NEEDS_ERASE ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u ||
        hpclang_stock_flash_program_word(scratch + 4094u, 0x5678u) != HPCLANG_SST_OK ||
        hpclang_flash_read16(0, scratch + 4094u) != 0x5678u)
        goto done;
    failure = 9;
    const uint8_t conflict[4] = {0, 0, 0xff, 0xff};
    if (hpclang_stock_flash_program(scratch, conflict, sizeof(conflict)) !=
            HPCLANG_SST_NEEDS_ERASE ||
        hpclang_flash_read16(0, scratch) != UINT16_MAX ||
        hpclang_flash_read16(0, scratch + 2u) != 0x1234u)
        goto done;
    failure = 10;
    if (hpclang_stock_flash_program(0x1ffffeu, conflict, sizeof(conflict)) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, UINT32_MAX - 1u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch + 1u, conflict, 2u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, 3u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, conflict, 0u) != HPCLANG_SST_INVALID ||
        hpclang_stock_flash_program(scratch, 0, 2u) != HPCLANG_SST_INVALID)
        goto done;
    failure = 11;
    for (uint32_t i = 0; i < sizeof(block); ++i)
        block[i] = (uint8_t)(i * 37u + 11u);
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (hpclang_stock_flash_program(scratch + 16u, block, sizeof(block)) != HPCLANG_SST_OK)
            goto done;
        for (uint32_t i = 0; i < sizeof(block); i += 2u)
            if (hpclang_flash_read16(0, scratch + 16u + i) !=
                ((uint16_t)block[i] | (uint16_t)block[i + 1u] << 8u))
                goto done;
    }
    failure = 12;
    if (hpclang_flash_read16(0, scratch + 14u) != UINT16_MAX ||
        hpclang_flash_read16(0, scratch + 16u + sizeof(block)) != UINT16_MAX ||
        l1[0] != original[0] || l1[1] != original[1])
        goto done;
    failure = 13;
    if (hpclang_stock_flash_erase_sector(scratch) != HPCLANG_SST_OK || fingerprint() != before ||
        l1[0] != original[0] || l1[1] != original[1])
        goto done;
    failure = 14;
    if (hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) || space.address != 0x20au ||
        space.nibbles != 0x3fdf4u)
        goto done;
    failure = 15;
    struct hpclang_object *object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (object || errno != ENOENT) {
        hpclang_object_release(object);
        goto done;
    }
    uint32_t bank = scratch & ~0x1ffffu;
    for (uint32_t i = 0; i < sizeof saved_sector; i += 2u) {
        uint16_t value = hpclang_flash_read16(0, bank + i);
        saved_sector[i] = (uint8_t)value;
        saved_sector[i + 1u] = (uint8_t)(value >> 8u);
    }
    /* Model interruption before the extent/status were published. Recovery
     * must discard the dirty body before a later append can use this bank. */
    failure = 20;
    if (hpclang_stock_flash_program_word(bank + 0x11au, 0) != HPCLANG_SST_OK ||
        hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) != HPCLANG_RPL_SPAN_MALFORMED ||
        hpclang_stock_flash_recover_tails() != HPCLANG_RPL_FLASH_WRITTEN ||
        hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space) || space.address <= 0x234u ||
        l1[0] != original[0] || l1[1] != original[1])
        goto done;
    uint32_t slot = bank + space.address / 2u;
    failure = 21;
    uint32_t recovered = fingerprint();
    if (hpclang_stock_flash_recover_tail((bank - 0x140000u) / 0x20000u) !=
            HPCLANG_RPL_FLASH_WRITTEN ||
        hpclang_stock_flash_recover_tails() != HPCLANG_RPL_FLASH_WRITTEN ||
        hpclang_stock_flash_recover_tail(6) != HPCLANG_RPL_FLASH_INVALID ||
        fingerprint() != recovered)
        goto done;
    failure = 16;
    if (hpclang_stock_flash_append(slot, native_record, sizeof native_record) !=
        HPCLANG_RPL_FLASH_WRITTEN)
        goto done;
    failure = 17;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    int64_t value;
    int valid = !hpclang_object_integer_i64(object, &value) && value == 987;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    failure = 18;
    if (hpclang_stock_flash_append(slot, native_record, sizeof native_record) !=
            HPCLANG_RPL_FLASH_OCCUPIED ||
        hpclang_stock_flash_append(bank + 0x1fff0u, native_record, sizeof native_record) !=
            HPCLANG_RPL_FLASH_INVALID)
        goto done;
    /* This bank was independently checked empty. Restore its original prefix
     * sector before Stock resumes, including every native bank-header byte. */
    failure = 19;
    if (hpclang_stock_flash_erase_sector(bank) != HPCLANG_SST_OK ||
        hpclang_stock_flash_program(bank, saved_sector, sizeof saved_sector) != HPCLANG_SST_OK ||
        fingerprint() != before || l1[0] != original[0] || l1[1] != original[1])
        goto done;
    /* Recreate a completed body whose final status write stopped at 73.
     * The expected record remains in RAM; finishing must only change 73->53. */
    block[0] = saved_sector[0x104u];
    for (uint32_t i = 0; i < sizeof native_record; ++i)
        block[i + 1u] = native_record[i];
    block[1] = 0x73;
    block[sizeof native_record + 1u] = 0xff;
    failure = 22;
    if (hpclang_stock_flash_program(bank + 0x104u, block, sizeof native_record + 2u) !=
            HPCLANG_SST_OK ||
        hpclang_stock_flash_finish_commit(bank + 0x105u, native_record, sizeof native_record) !=
            HPCLANG_RPL_FLASH_WRITTEN)
        goto done;
    failure = 23;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 987;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    failure = 24;
    uint32_t committed = fingerprint();
    if (hpclang_stock_flash_finish_commit(bank + 0x105u, native_record, sizeof native_record) !=
            HPCLANG_RPL_FLASH_WRITTEN ||
        fingerprint() != committed || l1[0] != original[0] || l1[1] != original[1])
        goto done;
    failure = 25;
    if (hpclang_stock_flash_erase_sector(bank) != HPCLANG_SST_OK ||
        hpclang_stock_flash_program(bank, saved_sector, sizeof saved_sector) != HPCLANG_SST_OK ||
        fingerprint() != before)
        goto done;
    /* With no retained transaction intent, recovery must keep an interrupted
     * backup invisible. Preserve its complete body rather than committing it. */
    const struct
    {
        uint8_t status, discarded_body;
    } interrupted[] = {{0x73, 0}, {0x03, 0}, {0xfe, 0}, {0x72, 0}, {0x72, 1}, {0x72, 2}};
    for (unsigned i = 0; i < sizeof interrupted / sizeof interrupted[0]; ++i) {
        for (uint32_t j = 0; j < sizeof native_record; ++j)
            block[j + 1u] = native_record[j];
        block[1] = interrupted[i].status;
        if (interrupted[i].discarded_body) {
            block[4] &= 15u;
            for (uint32_t j = 4; j < sizeof native_record; ++j)
                block[j + 1u] = interrupted[i].discarded_body == 2 ? 0xff : 0;
            block[10] = 0;
        }
        failure = 31;
        if (hpclang_stock_flash_program(bank + 0x104u, block, sizeof native_record + 2u) !=
                HPCLANG_SST_OK ||
            hpclang_stock_flash_recover_tails() != HPCLANG_RPL_FLASH_WRITTEN)
            goto done;
        block[1] = 0;
        failure = 32;
        for (uint32_t offset = 0; offset < sizeof native_record + 2u; offset += 2u)
            if (hpclang_flash_read16(0, bank + 0x104u + offset) !=
                ((uint16_t)block[offset] | (uint16_t)block[offset + 1u] << 8u))
                goto done;
        object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
        if (object || errno != ENOENT) {
            hpclang_object_release(object);
            goto done;
        }
        failure = 33;
        uint32_t discarded = fingerprint();
        if (hpclang_stock_flash_recover_tails() != HPCLANG_RPL_FLASH_WRITTEN ||
            fingerprint() != discarded || l1[0] != original[0] || l1[1] != original[1])
            goto done;
        failure = 34;
        if (hpclang_stock_flash_erase_sector(bank) != HPCLANG_SST_OK ||
            hpclang_stock_flash_program(bank, saved_sector, sizeof saved_sector) !=
                HPCLANG_SST_OK ||
            fingerprint() != before)
            goto done;
    }
    /* Independently locate the expected first-fit tail, preserving its entire
     * sector (including existing native objects) for restoration afterwards. */
    failure = 26;
    uint32_t create_slot = 0;
    for (uint32_t index = 0; index < 6u; ++index) {
        if (!hpclang_rpl_stock_port2_reader(index, &reader) ||
            hpclang_rpl_flash_tail(&reader, 0x20au, 0x40000u, &space))
            goto done;
        if (!create_slot && space.nibbles >= sizeof native_record * 4u)
            create_slot = 0x140000u + index * 0x20000u + space.address / 2u;
    }
    if (!create_slot || (create_slot & 4095u) + 2u * sizeof native_record > sizeof saved_sector)
        goto done;
    uint32_t create_sector = create_slot & ~4095u;
    for (uint32_t i = 0; i < sizeof saved_sector; i += 2u) {
        uint16_t word = hpclang_flash_read16(0, create_sector + i);
        saved_sector[i] = (uint8_t)word;
        saved_sector[i + 1u] = (uint8_t)(word >> 8u);
    }
    /* A previous launch left this hidden backup. The public create operation
     * must recover it without an explicit call to an internal recovery API. */
    for (uint32_t i = 0; i < sizeof block; ++i)
        block[i] = 0xff;
    uint32_t prefix = create_slot & 1u;
    if (prefix)
        block[0] = (uint8_t)hpclang_flash_read16(0, create_slot & ~1u);
    for (uint32_t i = 0; i < sizeof native_record; ++i)
        block[prefix + i] = native_record[i];
    block[prefix] = 0x72;
    if (hpclang_stock_flash_program(create_slot & ~1u, block,
                                    (sizeof native_record + prefix + 1u) & ~1u) != HPCLANG_SST_OK)
        goto done;
    failure = 27;
    struct hpclang_object *source = NULL;
    if (hpclang_object_create_integer_i64(987, &source))
        goto done;
    int created = hpclang_port_backup_create(2, (const uint8_t *)"FLASHONE", 8, source);
    hpclang_object_release(source);
    if (created)
        goto done;
    if ((hpclang_flash_read16(0, create_slot & ~1u) >> (prefix * 8u)) & 255u)
        goto done;
    failure = 28;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 987;
    if (!valid) {
        hpclang_object_release(object);
        goto done;
    }
    committed = fingerprint();
    failure = 29;
    created = hpclang_port_backup_create(2, (const uint8_t *)"FLASHONE", 8, object);
    hpclang_object_release(object);
    if (created != -1 || errno != EEXIST || fingerprint() != committed || l1[0] != original[0] ||
        l1[1] != original[1])
        goto done;
    failure = 35;
    const struct hpclang_rpl_flash_io plan_io = {0, plan_read, plan_program, 0x140000u, 0x200000u};
    struct hpclang_rpl_flash_replace_plan plan;
    uint8_t intent[HPCLANG_RPL_FLASH_INTENT_BYTES];
    if (hpclang_rpl_flash_plan_replace(&plan_io, native_record, sizeof native_record, &plan) ||
        plan.intent.old_slot != create_slot + sizeof native_record ||
        plan.intent.new_bytes != sizeof native_record || plan_program_calls ||
        fingerprint() != committed ||
        hpclang_rpl_flash_intent_encode(plan_io.first, plan_io.end, plan.intent_slot, &plan.intent,
                                        intent, sizeof intent))
        goto done;
    failure = 36;
    if ((plan.intent_slot & ~4095u) != create_sector ||
        plan.intent.new_slot + plan.intent.new_bytes > create_sector + sizeof saved_sector)
        goto done;
    const struct hpclang_rpl_flash_io publish_io = {0, plan_read, intent_program, 0x140000u,
                                                    0x200000u};
    if (hpclang_rpl_flash_publish_intent(&publish_io, native_record, sizeof native_record, &plan))
        goto done;
    for (uint32_t i = 0; i < sizeof intent; ++i) {
        uint32_t address = plan.intent_slot + i;
        uint8_t actual = (uint8_t)(hpclang_flash_read16(0, address & ~1u) >> ((address & 1u) * 8u));
        if (actual != intent[i])
            goto done;
    }
    failure = 37;
    if (hpclang_stock_flash_recover_tails() ||
        ((hpclang_flash_read16(0, plan.intent_slot & ~1u) >> ((plan.intent_slot & 1u) * 8u)) &
         255u))
        goto done;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 987;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    failure = 38;
    uint32_t replacement_bytes;
    uint8_t *replacement = replacement_record(&replacement_bytes);
    if (!replacement)
        goto done;
    int prepared = hpclang_rpl_flash_plan_replace(&plan_io, replacement, replacement_bytes, &plan);
    if (prepared || (plan.intent_slot & ~4095u) != create_sector ||
        plan.intent.new_slot + plan.intent.new_bytes > create_sector + sizeof saved_sector) {
        free(replacement);
        goto done;
    }
    int published =
        hpclang_rpl_flash_publish_intent(&publish_io, replacement, replacement_bytes, &plan);
    int appended = published ? published
                             : hpclang_rpl_flash_append(&publish_io, plan.intent.new_slot,
                                                        replacement, replacement_bytes);
    free(replacement);
    if (appended)
        goto done;
    failure = 39;
    if (hpclang_stock_flash_recover_tails() ||
        ((hpclang_flash_read16(0, plan.intent.old_slot & ~1u) >>
          ((plan.intent.old_slot & 1u) * 8u)) &
         255u))
        goto done;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 986;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    /* An interrupted new body is not a native object. Recover using the
     * durable reservation, retaining the current value and later tail. */
    failure = 40;
    replacement = replacement_record(&replacement_bytes);
    if (!replacement)
        goto done;
    prepared = hpclang_rpl_flash_plan_replace(&plan_io, replacement, replacement_bytes, &plan);
    if (prepared || (plan.intent_slot & ~4095u) != create_sector ||
        plan.intent.new_slot + plan.intent.new_bytes > create_sector + sizeof saved_sector) {
        free(replacement);
        goto done;
    }
    published =
        hpclang_rpl_flash_publish_intent(&publish_io, replacement, replacement_bytes, &plan);
    free(replacement);
    if (published || !intent_program(0, (plan.intent.new_slot + 6u) & ~1u, 0))
        goto done;
    failure = 41;
    if (hpclang_stock_flash_recover_tails() ||
        ((hpclang_flash_read16(0, plan.intent.new_slot & ~1u) >>
          ((plan.intent.new_slot & 1u) * 8u)) &
         255u))
        goto done;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 986;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    /* A fully verified body with an interrupted FF-to-53 publication must
     * finish publishing before recovery deletes the original backup. */
    failure = 42;
    replacement = replacement_record(&replacement_bytes);
    if (!replacement)
        goto done;
    prepared = hpclang_rpl_flash_plan_replace(&plan_io, replacement, replacement_bytes, &plan);
    if (prepared || (plan.intent_slot & ~4095u) != create_sector ||
        plan.intent.new_slot + plan.intent.new_bytes > create_sector + sizeof saved_sector) {
        free(replacement);
        goto done;
    }
    published =
        hpclang_rpl_flash_publish_intent(&publish_io, replacement, replacement_bytes, &plan);
    replacement[0] = 0x7f;
    uint32_t aligned = plan.intent.new_slot & ~1u;
    uint32_t offset = plan.intent.new_slot & 1u;
    uint32_t physical_bytes = (offset + replacement_bytes + 1u) & ~1u;
    if (physical_bytes > sizeof block) {
        free(replacement);
        goto done;
    }
    for (uint32_t i = 0; i < physical_bytes; ++i) {
        uint32_t address = aligned + i;
        block[i] = (uint8_t)(hpclang_flash_read16(0, address & ~1u) >> ((address & 1u) * 8u));
        if (i >= offset && i - offset < replacement_bytes)
            block[i] = replacement[i - offset];
    }
    int partial =
        published ? published : hpclang_stock_flash_program(aligned, block, physical_bytes);
    free(replacement);
    if (partial)
        goto done;
    failure = 43;
    if (hpclang_stock_flash_recover_tails() ||
        ((hpclang_flash_read16(0, plan.intent.old_slot & ~1u) >>
          ((plan.intent.old_slot & 1u) * 8u)) &
         255u) ||
        ((hpclang_flash_read16(0, plan.intent.new_slot & ~1u) >>
          ((plan.intent.new_slot & 1u) * 8u)) &
         255u) != 0x53u)
        goto done;
    object = hpclang_port_backup_get(2, (const uint8_t *)"FLASHONE", 8);
    if (!object)
        goto done;
    valid = !hpclang_object_integer_i64(object, &value) && value == 986;
    hpclang_object_release(object);
    if (!valid)
        goto done;
    failure = 30;
    if (hpclang_stock_flash_erase_sector(create_sector) != HPCLANG_SST_OK ||
        hpclang_stock_flash_program(create_sector, saved_sector, sizeof saved_sector) !=
            HPCLANG_SST_OK ||
        fingerprint() != before)
        goto done;
    failure = 0;
done:
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
    return 0;
}
