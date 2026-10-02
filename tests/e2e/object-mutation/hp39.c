#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/object.h>
#include <hpclang/variable.h>
#include "saturn-view.h"

extern volatile uint32_t hpclang_e2e_result;
static const uint8_t category[] = "Note";
static const uint8_t name[] = "NoteText";

static int get(struct hpclang_object **value)
{
    return hpclang_variable_get(category, sizeof(category) - 1u, name, sizeof(name) - 1u, value);
}

static int set(const struct hpclang_object *value)
{
    return hpclang_variable_set(category, sizeof(category) - 1u, name, sizeof(name) - 1u, value);
}

static uint32_t stock_word(uint32_t address)
{
    uint32_t value = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_saturn_read_nibble(hpclang_saturn_address_make(address + i), &nibble))
            return UINT32_MAX;
        value |= (uint32_t)nibble << (4u * i);
    }
    return value;
}

static uint32_t page_number_roundtrip(void)
{
    const uint8_t sketch[] = "Sketch", page[] = "PageNum";
    struct hpclang_object *original = NULL, *replacement = NULL, *readback = NULL;
    uint32_t failure = 0x240u;
    double number = 0.0, original_number = 0.0;
    if (hpclang_variable_get(sketch, 6u, page, 7u, &original) != 0 ||
        hpclang_object_real_f64(original, &original_number) != 0)
        goto done;
    ++failure;
    /* PageNum is constrained by the existing sketch pages. Reconstruct the
     * current valid number instead of assuming another page exists. */
    if (hpclang_object_create_real_f64(original_number, &replacement) != 0 ||
        hpclang_variable_set(sketch, 6u, page, 7u, replacement) != 0)
        goto done;
    hpclang_object_release(replacement);
    replacement = NULL;
    ++failure;
    if (hpclang_variable_get(sketch, 6u, page, 7u, &readback) != 0 ||
        hpclang_object_real_f64(readback, &number) != 0 || number != original_number)
        goto done;
    hpclang_object_release(readback);
    readback = NULL;
    ++failure;
    if (hpclang_object_create_string(NULL, 0u, &replacement) != 0 ||
        hpclang_variable_set(sketch, 6u, page, 7u, replacement) != -1 || errno != EINVAL)
        goto done;
    ++failure;
    if (hpclang_variable_get(sketch, 6u, page, 7u, &readback) != 0 ||
        hpclang_object_real_f64(readback, &number) != 0 || number != original_number)
        goto done;
    failure = 0u;
done:
    hpclang_object_release(readback);
    hpclang_object_release(replacement);
    hpclang_object_release(original);
    return failure;
}

uint32_t hpclang_e2e_hp39_mutation(void)
{
    struct hpclang_object *original = NULL, *value = NULL, *readback = NULL;
    uint8_t *text = malloc(3500u), *actual = malloc(3500u);
    uint32_t failure = 0x120u;
    /* In this fixture all mutated values are restored to their original extent.
     * The four original context bindings must therefore still identify the same
     * directories at the end; native calls must not leave a switched context. */
    const uint32_t bindings[] = {0x8071bu, 0x80734u, 0x80739u, 0x8073eu};
    uint32_t context_before[4];
    for (size_t i = 0u; i < 4u; ++i)
        context_before[i] = stock_word(bindings[i]);
    if (text == NULL || actual == NULL || get(&original) != 0)
        goto done;
    for (size_t i = 0; i < 3500u; ++i)
        text[i] = (uint8_t)('A' + i % 26u);
    const size_t lengths[] = {31u, 3450u, 1u, 0u};
    uint32_t last_temp = stock_word(0x806eeu);
    int saw_collection = 0;
    /* Repeated schema materialization exhausts the native TEMP workspace and
     * forces Stock GC while the ARM arena and old snapshots remain live. */
    for (size_t step = 0; step < 64u; ++step) {
        size_t length = lengths[step % 4u];
        failure = 0x121u + (uint32_t)step * 4u;
        hpclang_e2e_result = 0x4f530000u | failure;
        if (hpclang_object_create_string(text, length, &value) != 0)
            goto done;
        ++failure;
        if (set(value) != 0)
            goto done;
        hpclang_object_release(value);
        value = NULL;
        ++failure;
        if (get(&readback) != 0)
            goto done;
        size_t size = 0;
        ++failure;
        if (hpclang_object_string_size(readback, &size) != 0 || size != length ||
            hpclang_object_string_copy(readback, actual, size) != 0 ||
            memcmp(actual, text, size) != 0)
            goto done;
        hpclang_object_release(readback);
        readback = NULL;
        uint32_t temp = stock_word(0x806eeu);
        if (temp < last_temp)
            saw_collection = 1;
        last_temp = temp;
        if (saw_collection && step >= 7u && step % 4u == 3u)
            break;
    }
    failure = 0x138u;
    if (!saw_collection)
        goto done;
    failure = 0x132u;
    hpclang_e2e_result = 0x4f530000u | failure;
    if (hpclang_object_create_character('X', &value) != 0 || set(value) != -1 || errno != EINVAL)
        goto done;
    hpclang_object_release(value);
    value = NULL;
    ++failure;
    if (get(&readback) != 0)
        goto done;
    size_t size = 1;
    if (hpclang_object_string_size(readback, &size) != 0 || size != 0u)
        goto done;
    hpclang_object_release(readback);
    readback = NULL;
    ++failure;
    if (hpclang_object_create_string(text, 3500u, &value) != 0 || set(value) != -1 ||
        errno != E2BIG)
        goto done;
    ++failure;
    if (hpclang_variable_set(category, sizeof(category) - 1u, (const uint8_t *)"MissingM7", 9u,
                             original) != -1 ||
        errno != ENOENT)
        goto done;
    ++failure;
    hpclang_e2e_result = 0x4f530000u | failure;
    if (set(original) != 0 || get(&readback) != 0)
        goto done;
    size_t original_size = 0;
    if (hpclang_object_string_size(original, &original_size) != 0 || original_size > 3500u ||
        hpclang_object_string_size(readback, &size) != 0 || size != original_size ||
        hpclang_object_string_copy(original, text, size) != 0 ||
        hpclang_object_string_copy(readback, actual, size) != 0 || memcmp(actual, text, size) != 0)
        goto done;
    failure = page_number_roundtrip();
    if (failure != 0u)
        goto done;
    failure = 0x139u;
    for (size_t i = 0u; i < 4u; ++i)
        if (context_before[i] == UINT32_MAX || stock_word(bindings[i]) != context_before[i])
            goto done;
    failure = 0;
done:
    free(actual);
    free(text);
    hpclang_object_release(readback);
    hpclang_object_release(value);
    hpclang_object_release(original);
    return failure;
}
