#include <stddef.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include "rpl-stock-memory.h"
#include "saturn-view.h"

/* Public object tests model Saturn as unpacked nibbles. The RAM mapping
   capture and packed-byte backend have their own independent unit tests. */
int test_saturn_write_nibble(struct hpclang_saturn_address address, uint8_t value);
static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(address), value);
}
static int write_nibble(void *context, uint32_t address, uint8_t value)
{
    (void)context;
    return test_saturn_write_nibble(hpclang_saturn_address_make(address), value);
}
int hpclang_rpl_stock_mutation_io(struct hpclang_rpl_mutation_io *io,
                                  struct hpclang_rpl_stock_memory *memory)
{
    (void)memory;
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot->saturn_state == 0u ||
        (boot->platform_cookie & HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG) == 0u)
        return 0;
    const struct hpclang_rpl_memory_bounds *bounds =
        (const void *)(boot->platform_cookie & ~HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG);
    *io = (struct hpclang_rpl_mutation_io){
        .read_nibble = read_nibble,
        .write_nibble = write_nibble,
        .temp_limit = bounds->temp_limit,
        .user_limit = bounds->user_limit,
        .stack_reserve_nibbles = bounds->stack_reserve_nibbles,
        .native_roots = bounds->native_roots,
        .native_root_count = bounds->native_root_count,
    };
    return 1;
}
