/* Read-only transaction-state predicate before the reservation is released.
 * Not an installation-root resolver or permission to replay/erase Flash. */
#include "binding.h"
#include "rpl-flash-replay.h"
#include "rpl-stock-memory.h"
#include <stddef.h>

const volatile struct hpclang_recovery_binding hpclang_recovery_binding =
    HPCLANG_RECOVERY_BINDING_INITIALIZER;
static uint8_t journal[HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES];
static uint8_t authority[HPCLANG_RPL_FLASH_AUTHORITY_BYTES];

void *memset(void *destination, int value, size_t count)
{
    uint8_t *out = destination;
    while (count--)
        *out++ = (uint8_t)value;
    return destination;
}
void *memcpy(void *destination, const void *source, size_t count)
{
    uint8_t *out = destination;
    const uint8_t *in = source;
    while (count--)
        *out++ = *in++;
    return destination;
}
int memcmp(const void *left, const void *right, size_t count)
{
    const uint8_t *a = left, *b = right;
    while (count--) {
        if (*a != *b)
            return *a < *b ? -1 : 1;
        ++a;
        ++b;
    }
    return 0;
}
void __aeabi_memcpy(void *destination, const void *source, size_t count)
{
    memcpy(destination, source, count);
}
void __aeabi_memcpy4(void *destination, const void *source, size_t count)
{
    memcpy(destination, source, count);
}
void __aeabi_memclr4(void *destination, size_t count)
{
    memset(destination, 0, count);
}

void hpclang_flash_mapping_swap(unsigned descriptors[2]);
static int readable(uint32_t address, uint32_t bytes)
{
    uint32_t offset = address & 0x1ffffu;
    return address >= 0x140000u && address < 0x200000u && offset >= 0x105u && offset % 4u == 1u &&
           offset + bytes < 0x20000u;
}
static int read_flash(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    *value = *(volatile uint16_t *)address;
    return 1;
}
static int pending(void)
{
    uint32_t j = hpclang_recovery_binding.journal_slot;
    uint32_t a = hpclang_recovery_binding.authority_slot;
    if (hpclang_recovery_binding.magic[0] != 0x42435048u ||
        hpclang_recovery_binding.magic[1] != 0x31444e49u || !readable(j, sizeof journal) ||
        !readable(a, sizeof authority))
        return 0;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if ((l1[0] & 0xfff00803u) != 0x802u || (l1[1] & 0xfff00803u) != 0x100802u)
        return 0;
    unsigned descriptors[2] = {(l1[0] & ~0xcu) | 0xc00u, (l1[1] & ~0xcu) | 0xc00u};
    hpclang_flash_mapping_swap(descriptors);
    for (unsigned i = 0; i < sizeof journal; ++i)
        journal[i] = *(volatile uint8_t *)(j + i);
    for (unsigned i = 0; i < sizeof authority; ++i)
        authority[i] = *(volatile uint8_t *)(a + i);
    enum hpclang_rpl_flash_authority_phase phase;
    int result =
        hpclang_rpl_flash_authority_inspect(0x140000, 0x200000, a, authority, sizeof authority, j,
                                            journal, sizeof journal, &phase) == 0;
    result = result && (phase == HPCLANG_RPL_FLASH_AUTHORITY_ARMED ||
                        (phase == HPCLANG_RPL_FLASH_AUTHORITY_CLOSING && journal[0] == 0));
    if (result) {
        const struct hpclang_rpl_flash_io io = {descriptors, read_flash, NULL, 0x140000, 0x200000};
        result = hpclang_rpl_flash_bound_membership(&io, a, authority, sizeof authority, j, journal,
                                                    sizeof journal) == 0;
    }
    hpclang_flash_mapping_swap(descriptors);
    return result;
}

/* Local wrappers keep callback addresses position-relative in this owned ELF. */
static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    return hpclang_rpl_stock_memory_read(context, address, value);
}
static int write_nibble(void *context, uint32_t address, uint8_t value)
{
    return hpclang_rpl_stock_memory_write(context, address, value);
}
static int real_constant(const uint32_t *map, uint32_t address, int value)
{
    for (unsigned i = 0; i < 21; ++i) {
        uint32_t at = address + i;
        uint32_t page = map[at >> 12];
        if (page > 0x140000u - 2048u)
            return 0;
        uint32_t physical = page + ((at & 4095u) >> 1);
        unsigned nibble = (*(volatile uint8_t *)physical >> (4u * (at & 1u))) & 15u;
        unsigned expected = i < 5 ? (0x2933u >> (4u * i)) & 15u : (i == 19 ? value : 0);
        if (nibble != expected)
            return 0;
    }
    return 1;
}
int admission(const uint32_t *map)
{
    uintptr_t pointer = (uintptr_t)map;
    if (pointer < 0x08000000u || pointer > 0x08080000u - 257u * 4u || pointer % 4u ||
        !real_constant(map, 0x2f937, 0) || !real_constant(map, 0x2f94c, 1))
        return 0;
    struct hpclang_rpl_stock_memory memory;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if (!hpclang_rpl_stock_memory_capture(&memory, map, l1[0x80]))
        return 0;
    /* This path only pushes one immutable ROM reference. It never allocates or
     * relocates TEMP/USEROB; the stack check also keeps that cell above RSKTOP. */
    struct hpclang_rpl_mutation_io io = {.read_nibble = read_nibble,
                                         .write_nibble = write_nibble,
                                         .context = &memory,
                                         .temp_limit = 0x100000,
                                         .user_limit = 0x80000};
    return hpclang_rpl_mutation_push_real_boolean(&io, pending()) == HPCLANG_RPL_MUTATION_OK;
}
