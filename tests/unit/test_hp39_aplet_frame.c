#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "hp39-aplet-frame.h"

static uint8_t memory[0x100000];
static uint32_t unreadable = UINT32_MAX;
static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    if (address >= sizeof(memory) || address == unreadable)
        return 0;
    *value = memory[address];
    return 1;
}
static void put(uint32_t address, uint32_t count, uint32_t value)
{
    for (uint32_t i = 0; i < count; ++i)
        memory[address + i] = (value >> (4u * i)) & 15u;
}
static void setup(void)
{
    memset(memory, 0, sizeof(memory));
    put(0x806f8u, 5, 0x90000u);
    put(0x806fdu, 5, 0x90014u);
    put(0x806f3u, 5, 0x85000u);
    put(0x84ffbu, 5, 0x88024u);
    uint32_t protocol[] = {0x02d9du, 0x12cfdu,  0x12cfcu, 0x2892du, 0x05842u,
                           0x02e92u, 0x000113u, 0x12ca1u, 0x03244u, 0x0312bu};
    uint32_t address = 0x88000u;
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t width = i == 6u ? 6u : 5u;
        put(address, width, protocol[i]);
        address += width;
    }
    put(0x90000u, 5, 0x89000u);
    put(0x89000u, 5, 0x02a2cu);
    put(0x9000au, 5, 0x04049u);
    put(0x04049u, 5, 0x02911u);
}
int main(void)
{
    const struct hpclang_rpl_reader reader = {read_nibble, 0};
    uint32_t slot = 0;
    setup();
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == 0 && slot == 0x90005u);
    /* The bootstrap may explicitly hide the launcher pointer. The data slot
     * stays put, and
     * must not depend on whether the saved value is a string. */
    put(0x806f8u, 5, 0x90005u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 1, &slot) == 0 && slot == 0x90005u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS);
    put(0x806f8u, 5, 0x8ffb0u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 15, 1, &slot) == 0 && slot == 0x90005u);
    setup();
    /* Private roots lie below the original caller, and USEROB may move it. */
    put(0x806f8u, 5, 0x8ffb0u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 20, 0, &slot) == 0 && slot == 0x90005u);
    memmove(memory + 0x90030u, memory + 0x90000u, 20);
    put(0x806fdu, 5, 0x90044u);
    put(0x806f8u, 5, 0x8ffe0u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 20, 0, &slot) == 0 && slot == 0x90035u);
    /* A matching stack shape alone never authorizes the data channel. */
    for (uint32_t i = 0; i < 51u; i++) {
        setup();
        memory[0x88000u + i] ^= 1u;
        slot = 123u;
        assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS &&
               slot == 123u);
    }
    setup();
    put(0x0404eu, 5, 8u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS);
    setup();
    put(0x89000u, 5, 0x02a74u);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS);
    setup();
    assert(hpclang_hp39_aplet_data_slot(&reader, 21, 0, &slot) == -1 && errno == EILSEQ);
    setup();
    put(0x806fdu, 5, 0x8ffffu);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == EILSEQ);
    setup();
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, NULL) == -1 && errno == EINVAL);
    assert(hpclang_hp39_aplet_data_slot(NULL, 0, 0, &slot) == -1 && errno == EIO);
    unreadable = 0x806f8u;
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == EIO);
    unreadable = 0x88000u;
    slot = 123u;
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS &&
           slot == 123u);
    unreadable = UINT32_MAX;
    put(0x84ffbu, 5, 0xfffffu);
    assert(hpclang_hp39_aplet_data_slot(&reader, 0, 0, &slot) == -1 && errno == ENOSYS);
    return 0;
}
