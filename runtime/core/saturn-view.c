#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>

#include "saturn-view.h"

static uintptr_t saturn_page_base(struct hpclang_saturn_address address)
{
    if (address.nibble > UINT32_C(0x000fffff))
        return 0u;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL || boot->saturn_state == 0u)
        return 0u;

    /*
     * HPGCC SAT_CPU begins with Read[257]. HPC1 preserves the live Stock
     * SAT_CPU pointer in boot_info; Direct synthetic launchers deliberately do
     * not advertise a fake map. Each Read[] entry is the ARM byte base for one
     * 0x1000-nibble Saturn page.
     */
    const uint32_t *read_map =
        (const uint32_t *)(uintptr_t)boot->saturn_state;
    return (uintptr_t)read_map[address.nibble >> 12];
}

int hpclang_saturn_read_nibble(
    struct hpclang_saturn_address address,
    uint8_t *value)
{
    if (value == NULL)
        return 0;

    uintptr_t base = saturn_page_base(address);
    if (base == 0u)
        return 0;

    const volatile uint8_t *p =
        (const volatile uint8_t *)(
            base + ((address.nibble & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    *value = (address.nibble & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
    return 1;
}

int hpclang_saturn_read_byte(
    struct hpclang_saturn_address address,
    uint8_t *value)
{
    if (value == NULL)
        return 0;

    uint8_t low = 0u;
    uint8_t high = 0u;
    if (!hpclang_saturn_read_nibble(address, &low) ||
        !hpclang_saturn_read_nibble(
            hpclang_saturn_address_add(address, 1u), &high))
        return 0;

    *value = (uint8_t)(low | (uint8_t)(high << 4));
    return 1;
}

int hpclang_saturn_write_nibble(
    struct hpclang_saturn_address address,
    uint8_t value)
{
    if (value > 0x0fu)
        return 0;

    uintptr_t base = saturn_page_base(address);
    if (base == 0u)
        return 0;

    volatile uint8_t *p =
        (volatile uint8_t *)(
            base + ((address.nibble & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    if ((address.nibble & 1u) != 0u)
        packed = (uint8_t)((packed & 0x0fu) | (uint8_t)(value << 4));
    else
        packed = (uint8_t)((packed & 0xf0u) | value);
    *p = packed;
    return 1;
}
