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

static int mutate(uint32_t address, uint16_t value, int erase)
{
    if ((address & 1u) || address < user_flash.first || address >= user_flash.end ||
        (erase && (address & (user_flash.sector_bytes - 1u))))
        return HPCLANG_SST_INVALID;
    struct flash_owner owner;
    int result = open_flash(&owner);
    if (result != HPCLANG_SST_OK)
        return result;
    struct hpclang_sst_device device;
    result = hpclang_sst_identify(&bus, &device);
    if (result == HPCLANG_SST_OK &&
        (device.bytes != user_flash.end || device.sector_bytes != user_flash.sector_bytes))
        result = HPCLANG_SST_UNSUPPORTED;
    if (result == HPCLANG_SST_OK) {
        result = erase ? hpclang_sst_erase_sector(&bus, &user_flash, address, 100000u)
                       : hpclang_sst_program_word(&bus, &user_flash, address, value, 1000u);
        if (result == HPCLANG_SST_BUSY) {
            /* F0 cannot abort SST programming/erasure. Retain exclusive RAM
             * execution until completion, then independently verify the data. */
            while (hpclang_sst_wait_ready(&bus, address, 100000u) == HPCLANG_SST_BUSY) {
            }
            result = HPCLANG_SST_OK;
            uint32_t bytes = erase ? user_flash.sector_bytes : 2u;
            for (uint32_t offset = 0; offset < bytes; offset += 2u)
                if (hpclang_flash_read16(0, address + offset) != value)
                    result = HPCLANG_SST_VERIFY;
        }
    }
    close_flash(&owner);
    return result;
}

int hpclang_stock_flash_program_word(uint32_t address, uint16_t value)
{
    return mutate(address, value, 0);
}

int hpclang_stock_flash_erase_sector(uint32_t address)
{
    return mutate(address, UINT16_MAX, 1);
}
