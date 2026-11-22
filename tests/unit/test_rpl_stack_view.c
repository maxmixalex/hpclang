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

static void test_reference_overlap(void)
{
    reset_stack(0x70000u, 2);
    put_word5(0x70000u, 0x78000u);
    put_word5(0x70005u, 0x41234u);
    put_word5(0x7000au, 0x41500u); /* marker, not a level */
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41234u, 0x41235u) == 1);
    assert(!hpclang_rpl_stack_reference_overlap(&reader, 0x41000u, 0x41234u));
    assert(!hpclang_rpl_stack_reference_overlap(&reader, 0x41235u, 0x41600u));
    put_word5(0x78005u, 0x41111u);
    put_word5(0x7800au, 0x41122u);
    assert(!hpclang_rpl_stack_reference_overlap(&reader, 0x41111u, 0x41123u));
    put_word5(0x78000u, 0x02baau);
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41111u, 0x41112u) == 1);
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41122u, 0x41123u) == 1);
    assert(!hpclang_rpl_stack_reference_overlap(&reader, 0x41112u, 0x41122u));
    put_word5(0x70000u, IMAGE_NIBBLES - 5u);
    put_word5(IMAGE_NIBBLES - 5u, 0x02baau);
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41000u, 0x42000u) ==
           HPCLANG_RPL_STACK_UNAVAILABLE);
    put_word5(0x70000u, 0x78000u);
    put_word5(0x78000u, 0);
    image[0x70005u] = 16;
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41000u, 0x42000u) ==
           HPCLANG_RPL_STACK_MALFORMED);
    reset_stack(IMAGE_NIBBLES - 5, 2);
    assert(hpclang_rpl_stack_reference_overlap(&reader, 0x41000u, 0x42000u) ==
           HPCLANG_RPL_STACK_UNAVAILABLE);
}

int main(void)
{
    test_empty_stack();
    test_levels();
    test_reference_overlap();
    test_malformed_bounds();
    return 0;
}
