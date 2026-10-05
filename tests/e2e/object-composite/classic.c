#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <hpclang/object.h>
#include <saturn.h>

uint32_t hpclang_e2e_composite_value(struct hpclang_object **output);
int hpclang_e2e_composite_matches(const struct hpclang_object *array);
uint32_t hpclang_e2e_historical_objects(void);

static uint32_t matrix_roundtrip(void)
{
    static const char decimal[] = "-123456789012345678901234567890123456789012345678901234567890";
    struct hpclang_object *integer = NULL, *id = NULL, *matrix = NULL, *changed = NULL;
    struct hpclang_object *readback = NULL, *cell = NULL;
    uint32_t failure = 0x394u;
    if (hpclang_object_create_integer_decimal(decimal, sizeof(decimal) - 1u, &integer) != 0 ||
        hpclang_object_create_identifier((const uint8_t *)"X", 1u, HPCLANG_IDENTIFIER_GLOBAL,
                                         &id) != 0)
        goto done;
    const struct hpclang_object *cells[] = {id, integer, integer, id};
    if (hpclang_object_create_matrix(2u, 2u, cells, 4u, &matrix) != 0 ||
        hpclang_object_matrix_replace(matrix, 1u, 1u, integer, &changed) != 0 ||
        hpclang_stack_push(changed) != 0)
        goto done;
    hpclang_object_release(changed);
    changed = NULL;
    hpclang_object_release(integer);
    integer = NULL;
    hpclang_object_release(id);
    id = NULL;
    if (hpclang_stack_pop(&readback) != 0)
        goto done;
    uint32_t rows = 0u, columns = 0u;
    if (hpclang_object_matrix_size(readback, &rows, &columns) != 0 || rows != 2u || columns != 2u)
        goto done;
    for (uint32_t row = 0u; row < rows; ++row) {
        for (uint32_t column = 0u; column < columns; ++column) {
            if (hpclang_object_matrix_get(readback, row, column, &cell) != 0)
                goto done;
            if (row == 0u && column == 0u) {
                if (hpclang_object_kind(cell) != HPCLANG_OBJECT_IDENTIFIER)
                    goto done;
            } else {
                char copied[sizeof(decimal)] = {0};
                size_t length = 0u;
                if (hpclang_object_integer_decimal_size(cell, &length) != 0 ||
                    length != sizeof(decimal) - 1u ||
                    hpclang_object_integer_decimal_copy(cell, copied, sizeof(copied)) != 0 ||
                    strcmp(copied, decimal) != 0)
                    goto done;
            }
            hpclang_object_release(cell);
            cell = NULL;
        }
    }
    /* Replacing a cell must leave the caller's original matrix independent. */
    if (hpclang_object_matrix_get(matrix, 1u, 1u, &cell) != 0 ||
        hpclang_object_kind(cell) != HPCLANG_OBJECT_IDENTIFIER)
        goto done;
    failure = 0u;
done:
    hpclang_object_release(cell);
    hpclang_object_release(readback);
    hpclang_object_release(changed);
    hpclang_object_release(matrix);
    hpclang_object_release(integer);
    hpclang_object_release(id);
    return failure;
}

uint32_t hpclang_e2e_classic_composite(void)
{
    struct hpclang_object *array = NULL, *marker = NULL, *top = NULL;
    uint32_t before = 0u, after = 0u, failure = 0x382u;
    if (hpclang_stack_depth(&before) != 0 || hpclang_e2e_composite_value(&array) != 0 ||
        hpclang_object_create_character(0x5au, &marker) != 0)
        goto done;
    if (hpclang_stack_push(marker) != 0 || hpclang_stack_push(array) != 0 ||
        hpclang_stack_swap() != 0 || hpclang_stack_peek(1u, &top) != 0)
        goto done;
    uint8_t value = 0u;
    if (hpclang_object_character_value(top, &value) != 0 || value != 0x5au)
        goto done;
    hpclang_object_release(top);
    top = NULL;
    if (hpclang_stack_swap() != 0 || hpclang_stack_nip() != 0)
        goto done;
    hpclang_object_release(array);
    array = NULL;
    hpclang_object_release(marker);
    marker = NULL;
    if (hpclang_stack_pop(&top) != 0 || !hpclang_e2e_composite_matches(top) ||
        hpclang_stack_depth(&after) != 0 || after != before)
        goto done;
    int boolean = -1;
    if (hpclang_stack_push_boolean(1) != 0 || hpclang_stack_push_boolean(0) != 0 ||
        hpclang_stack_peek_boolean(2u, &boolean) != 0 || boolean != 1 ||
        hpclang_stack_pop_boolean(&boolean) != 0 || boolean != 0 ||
        hpclang_stack_pop_boolean(&boolean) != 0 || boolean != 1 ||
        hpclang_stack_depth(&after) != 0 || after != before)
        goto done;
    failure = matrix_roundtrip();
    if (failure != 0u)
        goto done;
    failure = 0x399u;
    if (sat_stack_push_string("bottom") != 1 || sat_stack_push_string("top") != 1 ||
        sat_stack_swap() != 1 || sat_stack_nip() != 1)
        goto done;
    char text[16];
    if (sat_pop_str(text) != text || strcmp(text, "bottom") != 0)
        goto done;
    sat_pushtruefalse(-7);
    if (hpclang_stack_pop_boolean(&boolean) != 0 || boolean != 1)
        goto done;
    failure = hpclang_e2e_historical_objects();
    if (failure == 0u && (hpclang_stack_depth(&after) != 0 || after != before))
        failure = 0x39au;
done:
    hpclang_object_release(top);
    hpclang_object_release(array);
    hpclang_object_release(marker);
    return failure;
}
