#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "saturn-view.h"

enum {
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu,
    SATURN_NIBBLES = 0x100000u
};

static uint8_t saturn[SATURN_NIBBLES];

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

static void put_word5(uint32_t at, uint32_t value)
{
    assert(at <= SATURN_NIBBLES - 5u);
    for (uint32_t i = 0u; i < 5u; ++i)
        saturn[at + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static void prepare_one_real(void)
{
    memset(saturn, 0, sizeof(saturn));
    put_word5(DSKTOP, 0x70000u);
    put_word5(EDITLINE, 0x7000au); /* level 1 + bottom marker */
    put_word5(0x70000u, 0x71000u);
    put_word5(0x71000u, 0x02933u);
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
    test_malformed_stack_state();
    return 0;
}
