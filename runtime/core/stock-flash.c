#include <hpclang/abi.h>
#include <hpclang/target.h>
#include <hpclang/time.h>
#include "irq.h"
#include "stock-flash.h"

/* Exchanges L1[0:2] with the two descriptors, preserving dirty RAM and
 * invalidating Flash aliases. Caller must mask IRQ/FIQ and execute in RAM. */
void hpclang_flash_mapping_swap(uint32_t descriptors[2]);

/* Assembly MMIO also handles physical address zero without a C null dereference. */
uint16_t hpclang_flash_read16(void *context, uint32_t address);
void hpclang_flash_write16(void *context, uint32_t address, uint16_t value);

static uint32_t flash_time_us(void *context)
{
    (void)context;
    return (uint32_t)hpclang_time_monotonic_us();
}

struct flash_owner
{
    uint32_t descriptors[2], irq;
};

static const struct hpclang_sst_bus bus = {0, hpclang_flash_read16, hpclang_flash_write16,
                                           flash_time_us};
static const struct hpclang_sst_region user_flash = {0x140000u, 0x200000u, 4096u};

static int open_flash(struct flash_owner *owner)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot || !boot->saturn_state ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G))
        return HPCLANG_SST_UNSUPPORTED;
    uint64_t now;
    if (hpclang_time_read_us(&now))
        return HPCLANG_SST_INVALID;

    owner->irq = hpclang_irq_save_disable();
    const volatile uint32_t *l1 = (const void *)(uintptr_t)0x07f00000u;
    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t old = l1[i];
        if ((old & 0xfff00803u) != ((i << 20u) | 0x802u)) {
            hpclang_irq_restore(owner->irq);
            return HPCLANG_SST_UNSUPPORTED;
        }
        /* Keep the physical section/domain, grant user writes and clear C/B. */
        owner->descriptors[i] = (old & ~0xcu) | 0xc00u;
    }
    hpclang_flash_mapping_swap(owner->descriptors);
    return HPCLANG_SST_OK;
}

static void close_flash(struct flash_owner *owner)
{
    hpclang_flash_mapping_swap(owner->descriptors);
    hpclang_irq_restore(owner->irq);
}

int hpclang_stock_flash_identify(struct hpclang_sst_device *device)
{
    if (!device)
        return HPCLANG_SST_INVALID;
    *device = (struct hpclang_sst_device){0};
    struct flash_owner owner;
    int result = open_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result;
    result = hpclang_sst_identify(&bus, device);
    /* identify always exits query mode, including unsupported chips/CFI. */
    close_flash(&owner);
    return result;
}

static int open_writable_flash(struct flash_owner *owner)
{
    int result = open_flash(owner);
    if (result != HPCLANG_SST_OK)
        return result;
    struct hpclang_sst_device device;
    result = hpclang_sst_identify(&bus, &device);
    if (result == HPCLANG_SST_OK &&
        (device.bytes != user_flash.end || device.sector_bytes != user_flash.sector_bytes))
        result = HPCLANG_SST_UNSUPPORTED;
    if (result != HPCLANG_SST_OK)
        close_flash(owner);
    return result;
}

static int finish_command(int result, uint32_t address, uint16_t value, uint32_t bytes)
{
    if (result == HPCLANG_SST_BUSY) {
        /* F0 cannot abort SST programming/erasure. Retain exclusive RAM
         * execution until completion, then independently verify the data. */
        while (hpclang_sst_wait_ready(&bus, address, 100000u) == HPCLANG_SST_BUSY) {
        }
        result = HPCLANG_SST_OK;
        for (uint32_t offset = 0; offset < bytes; offset += 2u)
            if (hpclang_flash_read16(0, address + offset) != value)
                result = HPCLANG_SST_VERIFY;
    }
    return result;
}

static uint16_t packed_word(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | (uint16_t)data[1] << 8u);
}

int hpclang_stock_flash_program(uint32_t address, const uint8_t *data, uint32_t bytes)
{
    if (!data || !bytes || ((address | bytes) & 1u) || address < user_flash.first ||
        address >= user_flash.end || bytes > user_flash.end - address)
        return HPCLANG_SST_INVALID;
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result;
    for (uint32_t offset = 0; offset < bytes; offset += 2u) {
        uint16_t value = packed_word(data + offset);
        if ((hpclang_flash_read16(0, address + offset) & value) != value) {
            result = HPCLANG_SST_NEEDS_ERASE;
            goto done;
        }
    }
    for (uint32_t offset = 0; offset < bytes; offset += 2u) {
        uint16_t value = packed_word(data + offset);
        result = hpclang_sst_program_word(&bus, &user_flash, address + offset, value, 1000u);
        result = finish_command(result, address + offset, value, 2u);
        if (result != HPCLANG_SST_OK)
            break;
    }
done:
    close_flash(&owner);
    return result;
}

int hpclang_stock_flash_program_word(uint32_t address, uint16_t value)
{
    const uint8_t data[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return hpclang_stock_flash_program(address, data, sizeof(data));
}

int hpclang_stock_flash_erase_sector(uint32_t address)
{
    if (address < user_flash.first || address >= user_flash.end ||
        (address & (user_flash.sector_bytes - 1u)))
        return HPCLANG_SST_INVALID;
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result;
    result = hpclang_sst_erase_sector(&bus, &user_flash, address, 100000u);
    result = finish_command(result, address, UINT16_MAX, user_flash.sector_bytes);
    close_flash(&owner);
    return result;
}

static int append_read(void *context, uint32_t address, uint16_t *value)
{
    *value = hpclang_flash_read16(context, address);
    return 1;
}

static int append_program(void *context, uint32_t address, uint16_t value)
{
    (void)context;
    int result = hpclang_sst_program_word(&bus, &user_flash, address, value, 1000u);
    return finish_command(result, address, value, 2u) == HPCLANG_SST_OK;
}

int hpclang_stock_flash_append(uint32_t address, const uint8_t *record, uint32_t bytes)
{
    if (address < user_flash.first || address >= user_flash.end || (address & 0x1ffffu) < 0x105u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result == HPCLANG_SST_UNSUPPORTED ? HPCLANG_RPL_FLASH_UNSUPPORTED
                                                 : HPCLANG_RPL_FLASH_FAILED;
    uint32_t bank = address & ~0x1ffffu;
    const struct hpclang_rpl_flash_io io = {0, append_read, append_program, bank + 0x104u,
                                            bank + 0x20000u};
    result = hpclang_rpl_flash_append(&io, address, record, bytes);
    close_flash(&owner);
    return result;
}

int hpclang_stock_flash_recover_tail(uint32_t bank_index)
{
    if (bank_index >= 6u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result == HPCLANG_SST_UNSUPPORTED ? HPCLANG_RPL_FLASH_UNSUPPORTED
                                                 : HPCLANG_RPL_FLASH_DIRTY;
    uint32_t bank = user_flash.first + bank_index * 0x20000u;
    const struct hpclang_rpl_flash_io io = {0, append_read, append_program, bank + 0x104u,
                                            bank + 0x20000u};
    if (hpclang_flash_read16(0, bank + 0x100u) != (0xf0u | ((bank_index + 8u) << 8u)))
        result = HPCLANG_RPL_FLASH_DIRTY;
    else
        result = hpclang_rpl_flash_recover_tail(&io, bank + 0x105u, bank + 0x20000u);
    close_flash(&owner);
    return result;
}

int hpclang_stock_flash_finish_commit(uint32_t address, const uint8_t *record, uint32_t bytes)
{
    if (address < user_flash.first || address >= user_flash.end || (address & 0x1ffffu) < 0x105u)
        return HPCLANG_RPL_FLASH_INVALID;
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result == HPCLANG_SST_UNSUPPORTED ? HPCLANG_RPL_FLASH_UNSUPPORTED
                                                 : HPCLANG_RPL_FLASH_DIRTY;
    uint32_t bank = address & ~0x1ffffu;
    const struct hpclang_rpl_flash_io io = {0, append_read, append_program, bank + 0x104u,
                                            bank + 0x20000u};
    result = hpclang_rpl_flash_finish_commit(&io, address, record, bytes);
    close_flash(&owner);
    return result;
}

int hpclang_stock_flash_create_backup(const uint8_t *record, uint32_t bytes)
{
    struct flash_owner owner;
    int result = open_writable_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result == HPCLANG_SST_UNSUPPORTED ? HPCLANG_RPL_FLASH_UNSUPPORTED
                                                 : HPCLANG_RPL_FLASH_FAILED;
    const struct hpclang_rpl_flash_io io = {0, append_read, append_program, user_flash.first,
                                            user_flash.end};
    result = hpclang_rpl_flash_create_backup(&io, record, bytes);
    close_flash(&owner);
    return result;
}
