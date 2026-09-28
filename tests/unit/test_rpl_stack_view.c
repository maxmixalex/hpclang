#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpl-stack-view.h"

enum
{
    IMAGE_NIBBLES = 0x81000u,
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu
};

static uint8_t image[IMAGE_NIBBLES];

static void put_word5(uint32_t at, uint32_t value)
{
    assert(at <= IMAGE_NIBBLES - 5u);
    for (uint32_t i = 0u; i < 5u; ++i)
        image[at + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    if (value == NULL || address >= IMAGE_NIBBLES)
        return 0;
    *value = image[address];
    return 1;
}

static const struct hpclang_rpl_reader reader = {
    .read_nibble = read_nibble,
    .context = NULL,
};

static void reset_stack(uint32_t top, uint32_t depth)
{
    memset(image, 0, sizeof(image));
    put_word5(DSKTOP, top);
    /*
     * EDITLINE is one five-nibble cell beyond the bottom marker:
     * top + depth object-pointer cells + one marker cell.
     */
    put_word5(EDITLINE, top + (depth + 1u) * 5u);
}

static void test_empty_stack(void)
{
    uint32_t depth = 99u;
    reset_stack(0x70000u, 0u);
    assert(hpclang_rpl_stack_depth(&reader, &depth) == HPCLANG_RPL_STACK_OK);
    assert(depth == 0u);

    uint32_t object = 99u;
    assert(hpclang_rpl_stack_object_address(&reader, 1u, &object) == HPCLANG_RPL_STACK_LEVEL);
    assert(object == 0u);
}

static void test_levels(void)
{
    reset_stack(0x70000u, 3u);
    put_word5(0x70000u, 0x12345u);
    put_word5(0x70005u, 0x23456u);
    put_word5(0x7000au, 0x34567u);

    uint32_t depth = 0u;
    assert(hpclang_rpl_stack_depth(&reader, &depth) == HPCLANG_RPL_STACK_OK);
    assert(depth == 3u);

    uint32_t object = 0u;
    assert(hpclang_rpl_stack_object_address(&reader, 1u, &object) == HPCLANG_RPL_STACK_OK);
    assert(object == 0x12345u);
    assert(hpclang_rpl_stack_object_address(&reader, 2u, &object) == HPCLANG_RPL_STACK_OK);
    assert(object == 0x23456u);
    assert(hpclang_rpl_stack_object_address(&reader, 3u, &object) == HPCLANG_RPL_STACK_OK);
    assert(object == 0x34567u);

    assert(hpclang_rpl_stack_object_address(&reader, 0u, &object) == HPCLANG_RPL_STACK_LEVEL);
    assert(hpclang_rpl_stack_object_address(&reader, 4u, &object) == HPCLANG_RPL_STACK_LEVEL);
}

static void test_malformed_bounds(void)
{
    uint32_t depth = 0u;

    memset(image, 0, sizeof(image));
    put_word5(DSKTOP, 0x70000u);
    put_word5(EDITLINE, 0x6ffffu);
    assert(hpclang_rpl_stack_depth(&reader, &depth) == HPCLANG_RPL_STACK_MALFORMED);

    put_word5(EDITLINE, 0x70006u);
    assert(hpclang_rpl_stack_depth(&reader, &depth) == HPCLANG_RPL_STACK_MALFORMED);

    put_word5(DSKTOP, 0xffffdu);
    put_word5(EDITLINE, 0xfffffu);
    assert(hpclang_rpl_stack_depth(&reader, &depth) == HPCLANG_RPL_STACK_MALFORMED);
}

int main(void)
{
    test_empty_stack();
    test_levels();
    test_malformed_bounds();
    return 0;
}
