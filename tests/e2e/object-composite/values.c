#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/object.h>

static int numeric_array_matches(const struct hpclang_object *array)
{
    struct hpclang_object_array_info info;
    if (hpclang_object_array_info(array, &info) != 0 || info.rank != 2u ||
        info.dimensions[0] != 2u || info.dimensions[1] != 2u || info.count != 4u ||
        info.element_kind != HPCLANG_OBJECT_COMPLEX)
        return 0;
    for (size_t index = 0u; index < 4u; ++index) {
        struct hpclang_object *item = NULL;
        double real = 0.0, imaginary = 0.0;
        if (hpclang_object_array_get(array, index, &item) != 0)
            return 0;
        int result = hpclang_object_complex_f64(item, &real, &imaginary);
        hpclang_object_release(item);
        if (result != 0 || real != (index == 3u ? -4.0 : 1.25) ||
            imaginary != (index == 3u ? 8.0 : -2.5))
            return 0;
    }
    return 1;
}

static uint32_t numeric_array(struct hpclang_object **output)
{
    struct hpclang_object *a = NULL, *b = NULL, *array = NULL, *changed = NULL;
    uint32_t failure = 0x380u;
    *output = NULL;
    if (hpclang_object_create_complex_f64(1.25, -2.5, &a) != 0 ||
        hpclang_object_create_complex_f64(-4.0, 8.0, &b) != 0)
        goto done;
    const struct hpclang_object *elements[] = {a, a, a, a};
    const uint32_t dimensions[] = {2u, 2u};
    if (hpclang_object_create_array(dimensions, 2u, elements, 4u, &array) != 0 ||
        hpclang_object_array_replace(array, 3u, b, &changed) != 0)
        goto done;
    struct hpclang_object *invalid = NULL;
    if (hpclang_object_array_get(array, 4u, &invalid) != -1 || errno != EINVAL || invalid != NULL) {
        hpclang_object_release(invalid);
        goto done;
    }
    *output = changed;
    changed = NULL;
    failure = 0u;
done:
    hpclang_object_release(changed);
    hpclang_object_release(array);
    hpclang_object_release(a);
    hpclang_object_release(b);
    if (failure == 0u && !numeric_array_matches(*output)) {
        hpclang_object_release(*output);
        *output = NULL;
        failure = 0x381u;
    }
    return failure;
}

static const uint8_t identifier[] = {'H', 'P', 'C', 'X'};
static const uint8_t label[] = {'b', 'i', 't', 's'};
static const uint8_t binary[] = {0xefu, 0xcdu, 0xabu, 0x89u, 0x67u, 0x45u, 0x03u};

int hpclang_e2e_composite_matches(const struct hpclang_object *list)
{
    struct hpclang_object *item = NULL, *value = NULL;
    size_t count = 0u, size = 0u;
    int ok = 0;
    if (hpclang_object_list_size(list, &count) != 0 || count != 4u ||
        hpclang_object_list_get(list, 0u, &item) != 0 || !numeric_array_matches(item))
        goto done;
    hpclang_object_release(item);
    item = NULL;
    enum hpclang_identifier_scope scope;
    uint8_t bytes[sizeof(binary)] = {0};
    if (hpclang_object_list_get(list, 1u, &item) != 0 ||
        hpclang_object_identifier_info(item, &size, &scope) != 0 || size != sizeof(identifier) ||
        scope != HPCLANG_IDENTIFIER_GLOBAL ||
        hpclang_object_identifier_copy(item, bytes, sizeof(bytes)) != 0 ||
        memcmp(bytes, identifier, sizeof(identifier)) != 0)
        goto done;
    hpclang_object_release(item);
    item = NULL;
    if (hpclang_object_list_get(list, 2u, &item) != 0 ||
        hpclang_object_tag_size(item, &size) != 0 || size != sizeof(label) ||
        hpclang_object_tag_copy(item, bytes, sizeof(bytes)) != 0 ||
        memcmp(bytes, label, sizeof(label)) != 0 || hpclang_object_tag_value(item, &value) != 0 ||
        hpclang_object_binary_size(value, &size) != 0 || size != 52u ||
        hpclang_object_binary_copy(value, bytes, sizeof(bytes)) != 0 ||
        memcmp(bytes, binary, sizeof(binary)) != 0)
        goto done;
    hpclang_object_release(item);
    item = NULL;
    struct hpclang_decimal_real decimal;
    if (hpclang_object_list_get(list, 3u, &item) != 0 ||
        hpclang_object_real_decimal(item, &decimal) != 0 ||
        decimal.significand != UINT64_C(100000000000) || decimal.exponent != 389 ||
        decimal.negative != 1)
        goto done;
    ok = 1;
done:
    hpclang_object_release(value);
    hpclang_object_release(item);
    return ok;
}

uint32_t hpclang_e2e_composite_value(struct hpclang_object **output)
{
    struct hpclang_object *array = NULL, *id = NULL, *bits = NULL, *tag = NULL;
    struct hpclang_object *wide = NULL, *list = NULL, *changed = NULL;
    const struct hpclang_decimal_real decimal = {.significand = 1u, .exponent = 400, .negative = 1};
    uint32_t failure = 0x388u;
    *output = NULL;
    if (numeric_array(&array) != 0 ||
        hpclang_object_create_identifier(identifier, sizeof(identifier), HPCLANG_IDENTIFIER_GLOBAL,
                                         &id) != 0 ||
        hpclang_object_create_binary(binary, 52u, &bits) != 0 ||
        hpclang_object_create_tagged(label, sizeof(label), bits, &tag) != 0 ||
        hpclang_object_create_real_decimal(&decimal, &wide) != 0)
        goto done;
    const struct hpclang_object *elements[] = {array, id, tag};
    const struct hpclang_object *inserted[] = {wide};
    size_t count = 0u;
    if (hpclang_object_create_list(elements, 3u, &list) != 0 ||
        hpclang_object_list_splice(list, 3u, 0u, inserted, 1u, &changed) != 0 ||
        hpclang_object_list_size(list, &count) != 0 || count != 3u)
        goto done;
    *output = changed;
    changed = NULL;
    failure = 0u;
done:
    hpclang_object_release(changed);
    hpclang_object_release(list);
    hpclang_object_release(wide);
    hpclang_object_release(tag);
    hpclang_object_release(bits);
    hpclang_object_release(id);
    hpclang_object_release(array);
    if (failure == 0u && !hpclang_e2e_composite_matches(*output)) {
        hpclang_object_release(*output);
        *output = NULL;
        failure = 0x389u;
    }
    return failure;
}
