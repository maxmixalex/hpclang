#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>
#include <saturn.h>

#include "object-snapshot.h"

enum { STACK_CAPACITY = 16 };

static struct hpclang_object *stack_items[STACK_CAPACITY];
static uint32_t stack_depth_value;
static uint32_t last_drop_count;

static struct hpclang_object *clone_object(
    const struct hpclang_object *object)
{
    size_t bytes = ((size_t)object->nibble_count + 1u) / 2u;
    struct hpclang_object *copy = malloc(sizeof(*copy) + bytes);
    assert(copy != NULL);
    memcpy(copy, object, sizeof(*copy) + bytes);
    return copy;
}

int hpclang_stack_depth(uint32_t *depth)
{
    *depth = stack_depth_value;
    return 0;
}

int hpclang_stack_peek(
    uint32_t level,
    struct hpclang_object **object)
{
    if (level == 0u || level > stack_depth_value)
        return -1;
    *object = clone_object(stack_items[level - 1u]);
    return 0;
}

int hpclang_stack_drop(uint32_t count)
{
    if (count > stack_depth_value)
        return -1;
    last_drop_count = count;
    for (uint32_t i = 0u; i < count; ++i)
        hpclang_object_release(stack_items[i]);
    memmove(
        stack_items,
        stack_items + count,
        (stack_depth_value - count) * sizeof(stack_items[0]));
    stack_depth_value -= count;
    return 0;
}

int hpclang_stack_push(const struct hpclang_object *object)
{
    if (stack_depth_value == STACK_CAPACITY)
        return -1;
    memmove(
        stack_items + 1u,
        stack_items,
        stack_depth_value * sizeof(stack_items[0]));
    stack_items[0] = clone_object(object);
    ++stack_depth_value;
    return 0;
}

static void clear_stack(void)
{
    while (stack_depth_value != 0u)
        assert(hpclang_stack_drop(stack_depth_value) == 0);
    last_drop_count = 0u;
}

static void set_nibble(
    struct hpclang_object *object,
    uint32_t offset,
    uint8_t value)
{
    uint8_t *packed = &object->data[offset >> 1];
    if ((offset & 1u) == 0u)
        *packed = (uint8_t)((*packed & 0xf0u) | (value & 0x0fu));
    else
        *packed = (uint8_t)((*packed & 0x0fu) | ((value & 0x0fu) << 4));
}

static void set_nibbles(
    struct hpclang_object *object,
    uint32_t offset,
    uint32_t value,
    uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i)
        set_nibble(
            object,
            offset + i,
            (uint8_t)((value >> (i * 4u)) & 0x0fu));
}

static struct hpclang_object *make_real_1_5(void)
{
    struct hpclang_object *object =
        calloc(1u, sizeof(*object) + 11u);
    assert(object != NULL);
    object->kind = HPCLANG_OBJECT_REAL;
    object->nibble_count = 21u;
    object->native_prologue = 0x02933u;
    set_nibbles(object, 0u, 0x02933u, 5u);

    /* exponent 0, mantissa 150000000000, positive sign */
    uint64_t mantissa = UINT64_C(150000000000);
    for (uint32_t i = 0u; i < 12u; ++i) {
        set_nibble(object, 8u + i, (uint8_t)(mantissa % 10u));
        mantissa /= 10u;
    }
    set_nibble(object, 20u, 0u);
    return object;
}

static void expect_top_string(const char *expected)
{
    char buffer[128];
    assert(sat_pick_str(1, buffer) == buffer);
    assert(strcmp(buffer, expected) == 0);
}

static void test_scope_depth_and_drop(void)
{
    clear_stack();
    unsigned scope = sat_stack_init();
    assert(scope != 0u);
    assert(sat_stack_depth() == 0);

    assert(sat_stack_push_string("one") == 1);
    assert(sat_stack_push_string("two") == 1);
    assert(sat_stack_push_string("three") == 1);
    assert(sat_stack_depth() == 3);

    last_drop_count = 0u;
    assert(sat_stack_dropn(2) == 2);
    assert(last_drop_count == 2u);
    assert(sat_stack_depth() == 1);
    assert(sat_stack_drop() == 1);
    assert(sat_stack_drop() == 0);

    sat_stack_exit(scope);
}

static void test_strings(void)
{
    clear_stack();
    assert(sat_stack_push_string("bottom") == 1);
    assert(sat_stack_push_string("top") == 1);

    char buffer[32];
    assert(sat_pick_str(2, buffer) == buffer);
    assert(strcmp(buffer, "bottom") == 0);
    assert(sat_stack_depth() == 2);

    char *allocated = sat_pick_str_alloc(1);
    assert(allocated != NULL);
    assert(strcmp(allocated, "top") == 0);
    free(allocated);
    assert(sat_stack_depth() == 2);

    assert(sat_pop_str(buffer) == buffer);
    assert(strcmp(buffer, "top") == 0);
    assert(sat_stack_depth() == 1);

    allocated = sat_pop_str_alloc();
    assert(allocated != NULL);
    assert(strcmp(allocated, "bottom") == 0);
    free(allocated);
    assert(sat_stack_depth() == 0);
}

static void test_numeric_string_pushes(void)
{
    clear_stack();

    assert(sat_stack_push_int(42) == 1);
    expect_top_string("42");
    clear_stack();

    assert(sat_stack_push_int(-42) == 1);
    expect_top_string("-42");
    clear_stack();

    assert(_sat_stack_push_int(255, 16, 0) == 1);
    expect_top_string("ff");
    clear_stack();

    assert(_sat_stack_push_int(255, 16, 1) == 1);
    expect_top_string("#ffh");
    clear_stack();

    assert(_sat_stack_push_int(255, 8, 1) == 1);
    expect_top_string("#377o");
    clear_stack();

    assert(_sat_stack_push_int(255, 10, 1) == 1);
    expect_top_string("#255d");
    clear_stack();

    assert(_sat_stack_push_int(10, 2, 1) == 1);
    expect_top_string("1010");
    clear_stack();

    assert(sat_stack_push_double(1.5) == 1);
    expect_top_string("1.500000");
    clear_stack();

    assert(sat_stack_push_double(-2.25) == 1);
    expect_top_string("-2.250000");
    clear_stack();
}

static void test_real_pop_is_type_safe(void)
{
    clear_stack();
    assert(sat_stack_push_string("not-real") == 1);
    assert(sat_pop_real() == 0.0);
    assert(sat_stack_depth() == 1);
    clear_stack();

    stack_items[0] = make_real_1_5();
    stack_depth_value = 1u;
    double value = sat_pop_real();
    assert(value > 1.499999 && value < 1.500001);
    assert(sat_stack_depth() == 0);
}

static void test_char_push(void)
{
    clear_stack();
    assert(sat_push_char((char)0xa5) == TRUE);
    assert(sat_stack_depth() == 1);

    struct hpclang_object *object = NULL;
    assert(hpclang_stack_peek(1u, &object) == 0);
    uint8_t value = 0u;
    assert(hpclang_object_character_value(object, &value) == 0);
    assert(value == 0xa5u);
    hpclang_object_release(object);
    clear_stack();
}

int main(void)
{
    test_scope_depth_and_drop();
    test_strings();
    test_numeric_string_pushes();
    test_real_pop_is_type_safe();
    test_char_push();
    clear_stack();
    return 0;
}