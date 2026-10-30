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

enum
{
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
    .stack_reserve_nibbles = 0u,
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

int hpclang_saturn_read_nibble(struct hpclang_saturn_address address, uint8_t *value)
{
    if (value == NULL || fake_boot.saturn_state == 0u || address.nibble >= SATURN_NIBBLES)
        return 0;
    *value = saturn[address.nibble];
    return 1;
}

int test_saturn_write_nibble(struct hpclang_saturn_address address, uint8_t value)
{
    if (fake_boot.saturn_state == 0u || address.nibble >= SATURN_NIBBLES || value > 0x0fu)
        return 0;
    saturn[address.nibble] = value;
    return 1;
}

static void enable_mutation(void)
{
    fake_boot.platform_cookie = (uintptr_t)&fake_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
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
    put_word5(0x91000u, 0x026d5u);

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
    put_word5(0x91000u, 0x026d5u); /* unsupported Aplet boundary */
    uint32_t before = get_word5(DSKTOP);
    errno = 0;
    assert(hpclang_stack_pop(&object) == -1);
    assert(object == NULL);
    assert(errno == ENOTSUP);
    assert(get_word5(DSKTOP) == before);
}

static void test_public_push_replace_roundtrip(void)
{
    prepare_one_real();

    uint32_t before = 0u;
    assert(hpclang_stack_depth(&before) == 0);

    struct hpclang_object *a = NULL;
    struct hpclang_object *b = NULL;
    assert(hpclang_object_create_character('A', &a) == 0);
    assert(hpclang_object_create_character('B', &b) == 0);

    assert(hpclang_stack_push(a) == 0);
    uint32_t depth = 0u;
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == before + 1u);

    assert(hpclang_stack_replace(1u, b) == 0);
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == before + 1u);

    struct hpclang_object *peek = NULL;
    assert(hpclang_stack_peek(1u, &peek) == 0);
    uint8_t value = 0u;
    assert(hpclang_object_character_value(peek, &value) == 0);
    assert(value == 'B');
    hpclang_object_release(peek);

    assert(hpclang_stack_push(a) == 0);
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == before + 2u);

    struct hpclang_object *popped = NULL;
    assert(hpclang_stack_pop(&popped) == 0);
    assert(hpclang_object_character_value(popped, &value) == 0);
    assert(value == 'A');
    hpclang_object_release(popped);

    assert(hpclang_stack_drop(1u) == 0);
    assert(hpclang_stack_depth(&depth) == 0);
    assert(depth == before);

    hpclang_object_release(a);
    hpclang_object_release(b);
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
    put_word5(EDITLINE, 0x90006u);

    uint32_t depth = 0u;
    errno = 0;
    assert(hpclang_stack_depth(&depth) == -1);
    assert(errno == EILSEQ);
}

static void test_public_stack_reorder(void)
{
    prepare_one_real();
    uint32_t top = get_word5(DSKTOP);
    errno = 0;
    assert(hpclang_stack_swap() == -1 && errno == EINVAL);
    assert(get_word5(DSKTOP) == top);
    assert(hpclang_stack_nip() == -1 && errno == EINVAL);
    assert(get_word5(DSKTOP) == top);

    put_word5(EDITLINE, top + 15u);
    put_word5(top + 5u, 0x92000u);
    assert(hpclang_stack_swap() == 0);
    assert(get_word5(top) == 0x92000u && get_word5(top + 5u) == 0x91000u);
    assert(hpclang_stack_nip() == 0);
    assert(get_word5(DSKTOP) == top + 5u && get_word5(top + 5u) == 0x92000u);

    fake_boot.platform_cookie = 0u;
    assert(hpclang_stack_swap() == -1 && errno == ENOSYS);
    assert(hpclang_stack_nip() == -1 && errno == ENOSYS);
}

static void test_boolean_exchange(void)
{
    prepare_one_real();
    put_word5(0x03a81u, 0x03a86u);
    put_word5(0x03ac0u, 0x03ac5u);
    uint32_t top = get_word5(DSKTOP), temp = get_word5(TEMPTOP), free_cells = get_word5(AVMEM);
    int value = 99;
    assert(hpclang_stack_pop_boolean(&value) == -1 && errno == EINVAL && value == 99);
    assert(get_word5(DSKTOP) == top);
    assert(hpclang_stack_push_boolean(2) == -1 && errno == EINVAL);
    assert(hpclang_stack_push_boolean(1) == 0);
    assert(hpclang_stack_push_boolean(0) == 0);
    assert(get_word5(TEMPTOP) == temp && get_word5(AVMEM) == free_cells - 2u);
    assert(get_word5(get_word5(DSKTOP)) == 0x03ac0u);
    assert(hpclang_stack_peek_boolean(2u, &value) == 0 && value == 1);
    assert(hpclang_stack_pop_boolean(&value) == 0 && value == 0);
    assert(hpclang_stack_pop_boolean(&value) == 0 && value == 1);
    assert(get_word5(DSKTOP) == top && get_word5(AVMEM) == free_cells);
    put_word5(0x03a81u, 0u);
    assert(hpclang_stack_push_boolean(1) == -1 && errno == EILSEQ);
    assert(get_word5(DSKTOP) == top && get_word5(AVMEM) == free_cells);
    fake_boot.platform_cookie = 0u;
    assert(hpclang_stack_push_boolean(0) == -1 && errno == ENOSYS);
    assert(hpclang_stack_pop_boolean(&value) == -1 && errno == ENOSYS);
}

static void test_versioned_native_root_masks(void)
{
    struct hpclang_rpl_memory_bounds saved = fake_bounds;
    const uint32_t versions[] = {HPCLANG_RPL_BOUNDS_MAGIC_V2, HPCLANG_RPL_BOUNDS_MAGIC,
                                 HPCLANG_RPL_BOUNDS_MAGIC_V4};
    for (unsigned v = 0; v < 3; ++v) {
        prepare_one_real();
        uint32_t root = 0xabc84010, *roots[] = {&root};
        const uint32_t masks[] = {0xfffff};
        fake_bounds.magic = versions[v];
        fake_bounds.native_roots = roots;
        fake_bounds.native_root_count = 1;
        fake_bounds.native_root_masks = masks;
        struct hpclang_object *object = NULL;
        assert(!hpclang_object_create_character('M', &object));
        assert(!hpclang_stack_push(object));
        /* Seven object nibbles and six allocation-footer nibbles shift the
         * return-stack interval. Old descriptors retain full-word semantics. */
        assert(get_word5(TEMPTOP) == 0x8400d);
        assert(root == (v == 2 ? 0xabc8401du : 0xabc84010u));
        hpclang_object_release(object);
        fake_bounds = saved;
    }
}

int main(void)
{
    test_versioned_native_root_masks();
    test_public_stack_reorder();
    test_boolean_exchange();
    test_public_read_only_stack();
    test_unfrozen_object_is_rejected();
    test_unavailable_family_and_mapping();
    test_public_drop();
    test_public_pop();
    test_public_push_replace_roundtrip();
    test_mutation_requires_fence();
    test_malformed_stack_state();
    return 0;
}
