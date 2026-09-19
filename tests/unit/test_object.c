#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "saturn-view.h"

enum {
    TEMPTOP = 0x806eeu,
    RSKTOP = 0x806f3u,
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu,
    AVMEM = 0x80e9bu,
    SATURN_NIBBLES = 0x100000u
};

static uint8_t saturn[SATURN_NIBBLES];

static struct hpclang_rpl_memory_bounds fake_bounds = {
    .magic = HPCLANG_RPL_BOUNDS_MAGIC,
    .temp_limit = 0x88000u,
    .user_limit = 0x88000u,
};

static struct hpclang_boot_info fake_boot = {
    .model = HPCLANG_MODEL_HP50G,
    .saturn_state = 1u,
};

static struct hpclang_device_info fake_device = {
    .size = sizeof(struct hpclang_device_info),
    .version = HPCLANG_DEVICE_INFO_VERSION,
    .model = HPCLANG_MODEL_HP50G,
    .family = HPCLANG_FAMILY_HP4950,
    .display_width = 131u,
    .display_height = 80u,
};

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &fake_boot;
}

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &fake_device;
}

int hpclang_saturn_read_nibble(
    struct hpclang_saturn_address address,
    uint8_t *value)
{
    if (value == NULL ||
        fake_boot.saturn_state == 0u ||
        address.nibble >= SATURN_NIBBLES)
        return 0;
    *value = saturn[address.nibble];
    return 1;
}

int hpclang_saturn_write_nibble(
    struct hpclang_saturn_address address,
    uint8_t value)
{
    if (fake_boot.saturn_state == 0u ||
        address.nibble >= SATURN_NIBBLES ||
        value > 0x0fu)
        return 0;
    saturn[address.nibble] = value;
    return 1;
}

static void enable_mutation(void)
{
    fake_boot.platform_cookie =
        (uintptr_t)&fake_bounds |
        HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
}

static void put_word5(uint32_t at, uint32_t value)
{
    assert(at <= SATURN_NIBBLES - 5u);
    for (uint32_t i = 0u; i < 5u; ++i)
        saturn[at + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t get_word5(uint32_t at)
{
    assert(at <= SATURN_NIBBLES - 5u);
    uint32_t value = 0u;
    for (uint32_t i = 0u; i < 5u; ++i)
        value |= (uint32_t)saturn[at + i] << (i * 4u);
    return value;
}

static void prepare_one_real(void)
{
    memset(saturn, 0, sizeof(saturn));
    enable_mutation();

    put_word5(TEMPTOP, 0x84000u);
    put_word5(RSKTOP, 0x85000u);
    put_word5(DSKTOP, 0x90000u);
    put_word5(EDITLINE, 0x9000au); /* level 1 + bottom marker */
    put_word5(AVMEM, (0x90000u - 0x85000u) / 5u);

    put_word5(0x90000u, 0x91000u);
    put_word5(0x91000u, 0x02933u);
}

static void test_public_read_only_stack(void)
{
    prepare_one_real();

    uint32_t depth = 0u;
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == 1u);

    struct hpclang_object *object = NULL;
    assert(hpclang_stack_peek(1u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    hpclang_object_release(object);

    errno = 0;
    assert(hpclang_stack_peek(2u, &object) == -1);
    assert(object == NULL);
    assert(errno == EINVAL);
}

static void test_unfrozen_object_is_rejected(void)
{
    prepare_one_real();
    put_word5(0x71000u, 0x02a96u);

    struct hpclang_object *object = NULL;
    errno = 0;
    assert(hpclang_stack_peek(1u, &object) == -1);
    assert(object == NULL);
    assert(errno == ENOTSUP);
}

static void test_unavailable_family_and_mapping(void)
{
    uint32_t depth = 99u;

    fake_device.model = HPCLANG_MODEL_HP39GS;
    fake_device.family = HPCLANG_FAMILY_HP39;
    errno = 0;
    assert(hpclang_stack_depth(&depth) == -1);
    assert(depth == 0u);
    assert(errno == ENOSYS);

    fake_device.model = HPCLANG_MODEL_HP50G;
    fake_device.family = HPCLANG_FAMILY_HP4950;
    fake_boot.saturn_state = 0u;
    errno = 0;
    assert(hpclang_stack_depth(&depth) == -1);
    assert(errno == ENOSYS);

    fake_boot.saturn_state = 1u;
}

static void test_public_drop(void)
{
    prepare_one_real();

    uint32_t before = get_word5(DSKTOP);
    assert(hpclang_stack_drop(1u) == 0);
    assert(get_word5(DSKTOP) == before + 5u);

    uint32_t depth = 99u;
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == 0u);

    before = get_word5(DSKTOP);
    errno = 0;
    assert(hpclang_stack_drop(1u) == -1);
    assert(errno == EINVAL);
    assert(get_word5(DSKTOP) == before);
}

static void test_public_pop(void)
{
    prepare_one_real();

    struct hpclang_object *object = NULL;
    assert(hpclang_stack_pop(&object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);

    uint32_t depth = 99u;
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == 0u);
    hpclang_object_release(object);

    prepare_one_real();
    put_word5(0x91000u, 0x02a96u); /* unsupported directory boundary */
    uint32_t before = get_word5(DSKTOP);
    errno = 0;
    assert(hpclang_stack_pop(&object) == -1);
    assert(object == NULL);
    assert(errno == ENOTSUP);
    assert(get_word5(DSKTOP) == before);
}

static void test_mutation_requires_fence(void)
{
    prepare_one_real();
    fake_boot.platform_cookie = 0u;

    uint32_t before = get_word5(DSKTOP);
    errno = 0;
    assert(hpclang_stack_drop(1u) == -1);
    assert(errno == ENOSYS);
    assert(get_word5(DSKTOP) == before);

    struct hpclang_object *object = NULL;
    errno = 0;
    assert(hpclang_stack_pop(&object) == -1);
    assert(object == NULL);
    assert(errno == ENOSYS);
    assert(get_word5(DSKTOP) == before);
}

static void test_malformed_stack_state(void)
{
    prepare_one_real();
    put_word5(EDITLINE, 0x70006u);

    uint32_t depth = 0u;
    errno = 0;
    assert(hpclang_stack_depth(&depth) == -1);
    assert(errno == EILSEQ);
}

int main(void)
{
    test_public_read_only_stack();
    test_unfrozen_object_is_rejected();
    test_unavailable_family_and_mapping();
    test_public_drop();
    test_public_pop();
    test_mutation_requires_fence();
    test_malformed_stack_state();
    return 0;
}
