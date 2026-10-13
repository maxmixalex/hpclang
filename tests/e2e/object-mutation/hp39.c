#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/object.h>
#include <hpclang/aplet.h>
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

static uint32_t aplet_data_roundtrip(void)
{
    struct hpclang_object *original = NULL, *value = NULL, *readback = NULL;
    uint32_t failure = 0x280u;
    union
    {
        struct
        {
            uint8_t text[1536], actual[1536];
        } roundtrip;
        uint8_t oversize[3496];
    } scratch;
    uint8_t *text = scratch.roundtrip.text;
    uint8_t *actual = scratch.roundtrip.actual;
    hpclang_e2e_result = 0x4f530280u;
    if (hpclang_aplet_data_get(NULL) != -1 || errno != EINVAL ||
        hpclang_aplet_data_set(NULL) != -1 || errno != EINVAL)
        goto done;
    if (hpclang_aplet_data_get(&original) != 0)
        goto done;
    uint32_t previous = stock_word(0x806eeu);
    int collected = 0;
    for (uint32_t step = 0; step < 128u; step++) {
        size_t size = step % 3u == 0u ? 1536u : (step % 3u == 1u ? 7u : 0u);
        memset(text, (int)('A' + step % 26u), size);
        failure = 0x281u;
        hpclang_e2e_result = 0x4f532000u | step;
        if (hpclang_object_create_string(text, size, &value) != 0 ||
            hpclang_aplet_data_set(value) != 0)
            goto done;
        hpclang_object_release(value);
        value = NULL;
        failure = 0x282u;
        hpclang_e2e_result = 0x4f533000u | step;
        if (hpclang_aplet_data_get(&readback) != 0)
            goto done;
        size_t count = 0;
        if (hpclang_object_string_size(readback, &count) != 0 || count != size ||
            hpclang_object_string_copy(readback, actual, size) != 0 ||
            memcmp(text, actual, size) != 0)
            goto done;
        hpclang_object_release(readback);
        readback = NULL;
        uint32_t current = stock_word(0x806eeu);
        if (current < previous)
            collected = 1;
        previous = current;
        if (collected && step > 5u)
            break;
    }
    failure = 0x283u;
    if (!collected || hpclang_aplet_data_clear() != 0 || hpclang_aplet_data_get(&readback) != 0 ||
        hpclang_object_kind(readback) != HPCLANG_OBJECT_LIST)
        goto done;
    /* The rejected write must leave the current value and older snapshot intact. */
    failure = 0x285u;
    /* Roundtrip scratch is no longer live. Reuse it so preparing the rejected
     * value does not require two simultaneous 3496-byte heap allocations. */
    memset(scratch.oversize, 0, sizeof(scratch.oversize));
    failure = 0x286u;
    if (hpclang_object_create_string(scratch.oversize, sizeof(scratch.oversize), &value) != 0)
        goto done;
    failure = 0x285u;
    if (hpclang_aplet_data_set(value) != -1 || errno != E2BIG ||
        hpclang_object_kind(readback) != HPCLANG_OBJECT_LIST)
        goto done;
    hpclang_object_release(readback);
    readback = NULL;
    if (hpclang_aplet_data_get(&readback) != 0 ||
        hpclang_object_kind(readback) != HPCLANG_OBJECT_LIST)
        goto done;
    failure = 0x284u;
    if (hpclang_aplet_data_set(original) != 0)
        goto done;
    failure = 0u;
done:
    hpclang_object_release(original);
    hpclang_object_release(value);
    hpclang_object_release(readback);
    return failure;
}

/* The lifecycle gate expects EMPTY -> SAVED -> EMPTY across distinct ARM
 * invocations, each separated by a settled APLET-library return. */
static uint32_t aplet_data_lifecycle(void)
{
    static const uint8_t saved[] = "HPCLANG-APLET-PERSIST";
    struct hpclang_object *value = NULL, *next = NULL;
    uint32_t failure = 0x290u, phase = 0u;
    if (hpclang_aplet_data_get(&value) != 0)
        goto done;
    if (hpclang_object_kind(value) == HPCLANG_OBJECT_LIST) {
        if (hpclang_object_create_string(saved, sizeof(saved) - 1u, &next) != 0 ||
            hpclang_aplet_data_set(next) != 0)
            goto done;
        phase = 0x300u;
    } else {
        uint8_t bytes[sizeof(saved)];
        size_t size = 0u;
        if (hpclang_object_string_size(value, &size) != 0 || size != sizeof(saved) - 1u ||
            hpclang_object_string_copy(value, bytes, size) != 0 || memcmp(bytes, saved, size) != 0)
            goto done;
        if (hpclang_aplet_data_clear() != 0)
            goto done;
        phase = 0x301u;
    }
    hpclang_e2e_result = 0x4f530000u | phase;
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    failure = 0u;
done:
    hpclang_object_release(value);
    hpclang_object_release(next);
    return failure;
}

uint32_t hpclang_e2e_hp39_mutation(void)
{
    uint32_t aplet_failure = aplet_data_roundtrip();
    if (aplet_failure != 0u)
        return aplet_failure;
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
    failure = aplet_data_lifecycle();
done:
    free(actual);
    free(text);
    hpclang_object_release(readback);
    hpclang_object_release(value);
    hpclang_object_release(original);
    return failure;
}
