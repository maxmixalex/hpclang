#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/object.h>
#include <saturn.h>

/* HPGCC 1.0.3 examples/libtests/stack.c inspects the incoming top entry.
 * Its raw mapping is replaced by the clean read-only snapshot service. */
static int inspect_argument(void)
{
    struct hpclang_object *input = NULL, *copy = NULL;
    uint32_t before = 0u, after = 0u;
    int64_t number = 0;
    int ok = hpclang_stack_depth(&before) == 0 &&
             hpclang_object_create_integer_i64(21, &input) == 0 && hpclang_stack_push(input) == 0 &&
             hpclang_stack_peek(1u, &copy) == 0 && hpclang_object_integer_i64(copy, &number) == 0 &&
             number == 21 && hpclang_stack_depth(&after) == 0 && after == before + 1u &&
             hpclang_stack_drop(1u) == 0;
    hpclang_object_release(copy);
    hpclang_object_release(input);
    return ok;
}

/* HPGCC 1.1 examples/stack/rplstk.c: receive two textual arguments and
 * return their four arithmetic results as a calculator string. Preserve
 * unrelated caller entries instead of consuming the whole user stack. */
static int textual_arguments(void)
{
    if (!sat_stack_push_string("21") || !sat_stack_push_string("7"))
        return 0;
    char *right = sat_pop_str_alloc(), *left = sat_pop_str_alloc();
    if (right == NULL || left == NULL) {
        free(right);
        free(left);
        return 0;
    }
    int x = atoi(left), y = atoi(right);
    free(left);
    free(right);
    char text[64], result[64];
    int length = snprintf(text, sizeof(text), "{%d %d %d %d}", x + y, x - y, x * y, y ? x / y : 0);
    return length > 0 && (size_t)length < sizeof(text) && sat_stack_push_string(text) == 1 &&
           sat_pop_str(result) == result && strcmp(result, "{28 14 147 3}") == 0;
}

/* HPGCC 2.0 examples/stack.c: double a ZINT, push a greeting, then push
 * the resulting depth. This exercises the historical adapter as an application. */
static int integer_application(void)
{
    unsigned scope = sat_stack_init();
    int before = sat_stack_depth();
    if (!sat_push_zint_llong(-987))
        return 0;
    LONGLONG input = sat_pop_zint_llong();
    if (input != -987 || !sat_push_zint_llong(input * 2) ||
        !sat_stack_push_string("Hello, World!") || !sat_push_zint_llong(sat_stack_depth()))
        return 0;
    char greeting[32];
    int ok = sat_pop_zint_llong() == before + 2 && sat_pop_str(greeting) == greeting &&
             strcmp(greeting, "Hello, World!") == 0 && sat_pop_zint_llong() == -1974;
    sat_stack_exit(scope);
    return ok && sat_stack_depth() == before;
}

/* HPGCC3 sat3.h's existing-object list-building example appends 1, 2, 3,
 * releases the temporary numbers and continues using the resulting list. */
static int list_application(void)
{
    struct hpclang_object *list = NULL, *number = NULL, *next = NULL;
    int ok = 0;
    if (hpclang_object_create_list(NULL, 0u, &list) != 0)
        goto done;
    for (size_t i = 0u; i < 3u; ++i) {
        if (hpclang_object_create_real_f64((double)(i + 1u), &number) != 0)
            goto done;
        const struct hpclang_object *inserted[] = {number};
        if (hpclang_object_list_splice(list, i, 0u, inserted, 1u, &next) != 0)
            goto done;
        hpclang_object_release(number);
        number = NULL;
        hpclang_object_release(list);
        list = next;
        next = NULL;
    }
    if (hpclang_stack_push(list) != 0)
        goto done;
    hpclang_object_release(list);
    list = NULL;
    if (hpclang_stack_pop(&list) != 0)
        goto done;
    for (size_t i = 0u; i < 3u; ++i) {
        double value = 0;
        if (hpclang_object_list_get(list, i, &number) != 0 ||
            hpclang_object_real_f64(number, &value) != 0 || value != (double)(i + 1u))
            goto done;
        hpclang_object_release(number);
        number = NULL;
    }
    ok = 1;
done:
    hpclang_object_release(next);
    hpclang_object_release(number);
    hpclang_object_release(list);
    return ok;
}

uint32_t hpclang_e2e_historical_objects(void)
{
    if (!inspect_argument())
        return 0x395u;
    if (!textual_arguments())
        return 0x396u;
    if (!integer_application())
        return 0x397u;
    if (!list_application())
        return 0x398u;
    return 0u;
}
