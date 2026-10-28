#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>
#include "rpl-stock-memory.h"

static struct hpclang_boot_info boot;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &boot;
}
int main(void)
{
    uint32_t *l1 = mmap((void *)0x07f00000u, 0x10000, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    uint8_t *eram = mmap((void *)0x08020000u, 0x20000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(l1 == (void *)0x07f00000u && eram == (void *)0x08020000u);
    l1[0x80] = 0x08000c02u;
    struct hpclang_rpl_memory_bounds bounds = {.temp_limit = 0xa0000, .user_limit = 0xb0000};
    boot.saturn_state = 1;
    boot.platform_cookie = (uintptr_t)&bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    const uint32_t versions[] = {HPCLANG_RPL_BOUNDS_MAGIC, HPCLANG_RPL_BOUNDS_MAGIC_V4};
    const uint32_t limits[] = {0, 0x2000, 0x40000, 0x40001};
    for (unsigned model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model)
        for (unsigned version = 0; version < 2; ++version)
            for (unsigned fence = 0; fence < 4; ++fence) {
                boot.model = model;
                bounds.magic = versions[version];
                bounds.port1_limit = limits[fence];
                struct hpclang_rpl_mutation_io io, unchanged;
                memset(&io, 0xa5, sizeof io);
                unchanged = io;
                uint32_t limit = 99;
                int expected =
                    (model == HPCLANG_MODEL_HP49GPLUS || model == HPCLANG_MODEL_HP50G) && fence < 3;
                assert(hpclang_rpl_stock_port1_mutation_io(&io, &limit) == expected);
                if (!expected) {
                    assert(limit == 99 && !memcmp(&io, &unchanged, sizeof io));
                    continue;
                }
                assert(limit == limits[fence] && io.context == eram);
                eram[7] = 0xa3;
                uint8_t nibble = 0;
                assert(io.read_nibble(io.context, 14, &nibble) && nibble == 3);
                assert(io.write_nibble(io.context, 15, 5) && eram[7] == 0x53);
                assert(!io.write_nibble(io.context, 15, 16) && eram[7] == 0x53);
                assert(!io.write_nibble(io.context, 0x40000, 1));
            }
    assert(!munmap(l1, 0x10000) && !munmap(eram, 0x20000));
    return 0;
}
