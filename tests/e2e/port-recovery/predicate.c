#include "rpl-stock-memory.h"
#include <stddef.h>

int hpclang_recovery_predicate(const uint32_t *map);

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
    return hpclang_rpl_mutation_push_real_boolean(&io, hpclang_recovery_predicate(map)) ==
           HPCLANG_RPL_MUTATION_OK;
}
