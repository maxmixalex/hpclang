#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "port-memory.h"
#include "launch-services.h"

int main(void)
{
    uint32_t pages[] = {0x08040012u, 0x08023012u, 0x08021012u, 0x08001012u};
    uint32_t limit = 99;
    assert(hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 4, &limit));
    assert(limit == 0x2000u); /* Includes all stage/heap pages, irrespective of order. */
    assert(hpclang_stage_port1_limit(HPCLANG_MODEL_HP49GPLUS, pages, 1, &limit));
    assert(limit == 0x40000u); /* No ARM pages in ERAM. */
    pages[2] = 0x08020012u;
    assert(hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 4, &limit));
    assert(limit == 0); /* Entire prefix belongs to the application. */
    limit = 99;
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP48GII, pages, 4, &limit));
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP39GS, pages, 4, &limit));
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 0, &limit));
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 257, &limit));
    pages[3] = 0x08001011u; /* Invalid small-page descriptor. */
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 4, &limit));
    pages[3] = 0x08080012u; /* Outside the model's physical RAM. */
    assert(!hpclang_stage_port1_limit(HPCLANG_MODEL_HP50G, pages, 4, &limit));
    assert(limit == 99);

    struct hpclang_rpl_memory_bounds bounds = {.magic = HPCLANG_RPL_BOUNDS_MAGIC,
                                               .temp_limit = 0xa0000u,
                                               .user_limit = 0xb0000u,
                                               .port1_limit = 0x2000u};
    struct hpclang_boot_info boot = {.model = HPCLANG_MODEL_HP50G,
                                     .saturn_state = 1,
                                     .platform_cookie = (uintptr_t)&bounds |
                                                        HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG};
    assert(hpclang_launch_port1_limit(&boot) == 0x2000u);
    bounds.port1_limit = 0x40001u;
    assert(hpclang_launch_port1_limit(&boot) == 0);
    bounds.port1_limit = 0x2000u;
    boot.model = HPCLANG_MODEL_HP48GII;
    assert(hpclang_launch_port1_limit(&boot) == 0);
    boot.model = HPCLANG_MODEL_HP50G;
    /* An old launch descriptor has no ERAM field. Main-RAM services remain
     * available, but accessing the absent extension would be an ASan error. */
    size_t prefix = offsetof(struct hpclang_rpl_memory_bounds, port1_limit);
    void *old = malloc(prefix);
    assert(old);
    bounds.magic = HPCLANG_RPL_BOUNDS_MAGIC_V2;
    memcpy(old, &bounds, prefix);
    boot.platform_cookie = (uintptr_t)old | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    assert(hpclang_launch_rpl_bounds(&boot));
    assert(hpclang_launch_port1_limit(&boot) == 0);
    free(old);
    return 0;
}
