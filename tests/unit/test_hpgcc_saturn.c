#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>
#include <saturn.h>

#include "object-snapshot.h"

enum
{
    STACK_CAPACITY = 16
};

static struct hpclang_object *stack_items[STACK_CAPACITY];
static uint32_t stack_depth_value;
static uint32_t last_drop_count;
static int last_boolean;

int hpclang_stack_push_boolean(int value)
{
    assert(value == 0 || value == 1);
    last_boolean = value;
    return 0;
}

int hpclang_stack_swap(void)
{
    if (stack_depth_value < 2u)
        return -1;
    struct hpclang_object *top = stack_items[0];
    stack_items[0] = stack_items[1];
    stack_items[1] = top;
    return 0;
}

int hpclang_stack_nip(void)
{
    if (stack_depth_value < 2u)
        return -1;
    hpclang_object_release(stack_items[1]);
    memmove(stack_items + 1u, stack_items + 2u, (stack_depth_value - 2u) * sizeof(stack_items[0]));
    --stack_depth_value;
    return 0;
}

static struct hpclang_object *clone_object(const struct hpclang_object *object)
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

int hpclang_stack_peek(uint32_t level, struct hpclang_object **object)
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
    memmove(stack_items, stack_items + count, (stack_depth_value - count) * sizeof(stack_items[0]));
    stack_depth_value -= count;
    return 0;
}

int hpclang_stack_push(const struct hpclang_object *object)
{
    if (stack_depth_value == STACK_CAPACITY)
        return -1;
    memmove(stack_items + 1u, stack_items, stack_depth_value * sizeof(stack_items[0]));
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

    assert(sat_stack_swap() == 0 && sat_stack_nip() == 0);
    assert(sat_stack_push_string("bottom") == 1);
    assert(sat_stack_swap() == 0 && sat_stack_nip() == 0);
    assert(sat_stack_push_string("top") == 1);
    assert(sat_stack_swap() == 1);
    expect_top_string("bottom");
    assert(sat_stack_nip() == 1 && sat_stack_depth() == 1);
    expect_top_string("bottom");
    clear_stack();
    sat_pushtruefalse(-3);
    assert(last_boolean == 1);
    sat_stack_push_bool(0);
    assert(last_boolean == 0);
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

static void test_stdargs(void)
{
    clear_stack();
    assert(sat_stack_push_string("'left'") == 1);
    assert(sat_stack_push_string("right") == 1);

    char **argv = NULL;
    assert(sat_stack_pop_stdargs(&argv) == 2);
    assert(argv != NULL);
    assert(strcmp(argv[0], "left") == 0);
    assert(strcmp(argv[1], "right") == 0);
    assert(sat_stack_depth() == 0);
    free(argv[0]);
    free(argv[1]);
    free(argv);

    assert(sat_stack_push_string("'value'") == 1);
    const char *program = "lotto";
    argv = NULL;
    assert(_sat_stack_pop_stdargs(program, &argv) == 2);
    assert(argv != NULL);
    assert(argv[0] == program);
    assert(strcmp(argv[1], "value") == 0);
    assert(sat_stack_depth() == 0);
    free(argv[1]);
    free(argv);

    argv = (char **)(uintptr_t)1u;
    assert(sat_stack_pop_stdargs(&argv) == 0);
    assert(argv == NULL);

    assert(sat_stack_push_string("good") == 1);
    assert(sat_push_char('x') == TRUE);
    argv = (char **)(uintptr_t)1u;
    assert(sat_stack_pop_stdargs(&argv) == 0);
    assert(argv == NULL);
    assert(sat_stack_depth() == 2);
    clear_stack();
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

static void test_zint_exchange(void)
{
    clear_stack();

    assert(sat_push_zint_llong(123456789LL) == TRUE);
    assert(sat_stack_depth() == 1);
    assert(sat_pop_zint_llong() == 123456789LL);
    assert(sat_stack_depth() == 0);

    assert(sat_push_zint_llong(-987654321LL) == TRUE);
    assert(sat_pop_zint_llong() == -987654321LL);
    assert(sat_stack_depth() == 0);

    assert(sat_push_zint_llong(LLONG_MIN) == TRUE);
    assert(sat_pop_zint_llong() == LLONG_MIN);
    assert(sat_stack_depth() == 0);

    assert(sat_push_zint_llong(0) == TRUE);
    assert(sat_pop_zint_llong() == 0);
    assert(sat_stack_depth() == 0);

    assert(sat_stack_push_string("not-zint") == 1);
    assert(sat_pop_zint_llong() == 0);
    assert(sat_stack_depth() == 1);
    clear_stack();
}

static void test_historical_stack_example_flow(void)
{
    clear_stack();

    /* Mirror the bundled HPGCC examples/stack/stack.c usage pattern. */
    assert(sat_push_zint_llong(21) == TRUE);
    unsigned scope = sat_stack_init();
    assert(scope != 0u);

    assert(sat_push_zint_llong(sat_pop_zint_llong() * 2) == TRUE);
    assert(sat_stack_push_string("Hello, World!") == 1);
    assert(sat_push_zint_llong((LONGLONG)sat_stack_depth()) == TRUE);
    sat_stack_exit(scope);

    assert(sat_stack_depth() == 3);
    assert(sat_pop_zint_llong() == 2);

    char buffer[32];
    assert(sat_pop_str(buffer) == buffer);
    assert(strcmp(buffer, "Hello, World!") == 0);
    assert(sat_pop_zint_llong() == 42);
    assert(sat_stack_depth() == 0);
}

static void test_real_exchange(void)
{
    clear_stack();

    assert(sat_push_real(1.5) == TRUE);
    assert(sat_stack_depth() == 1);
    struct hpclang_object *object = NULL;
    assert(hpclang_stack_peek(1u, &object) == 0);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    hpclang_object_release(object);
    double value = sat_pop_real();
    assert(value > 1.49999999999 && value < 1.50000000001);
    assert(sat_stack_depth() == 0);

    assert(sat_push_real(-2.25) == TRUE);
    value = sat_pop_real();
    assert(value > -2.25000000001 && value < -2.24999999999);
    assert(sat_stack_depth() == 0);

    assert(sat_push_real(INFINITY) == FALSE);
    assert(sat_stack_depth() == 0);

    assert(sat_stack_push_string("not-real") == 1);
    assert(sat_pop_real() == 0.0);
    assert(sat_stack_depth() == 1);
    clear_stack();
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
    test_stdargs();
    test_numeric_string_pushes();
    test_zint_exchange();
    test_historical_stack_example_flow();
    test_real_exchange();
    test_char_push();
    clear_stack();
    return 0;
}
