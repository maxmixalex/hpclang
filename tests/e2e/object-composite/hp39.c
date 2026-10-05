#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <hpclang/aplet.h>
#include <hpclang/object.h>
#include "saturn-view.h"

extern volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_composite_value(struct hpclang_object **output);
int hpclang_e2e_composite_matches(const struct hpclang_object *object);

static uint32_t temp_top(void)
{
    uint32_t result = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!hpclang_saturn_read_nibble(hpclang_saturn_address_make(0x806eeu + i), &nibble))
            return UINT32_MAX;
        result |= (uint32_t)nibble << (i * 4u);
    }
    return result;
}

uint32_t hpclang_e2e_hp39_composite(void)
{
    struct hpclang_object *saved = NULL, *value = NULL, *readback = NULL;
    size_t count = 0u;
    uint32_t failure = 0x390u;
    if (hpclang_aplet_data_get(&saved) != 0 || hpclang_object_list_size(saved, &count) != 0 ||
        (count != 0u && !hpclang_e2e_composite_matches(saved)))
        goto done;
    int was_empty = count == 0u;
    if (hpclang_e2e_composite_value(&value) != 0 || hpclang_aplet_data_set(value) != 0)
        goto done;
    hpclang_object_release(value);
    value = NULL;
    if (hpclang_aplet_data_get(&readback) != 0 || !hpclang_e2e_composite_matches(readback))
        goto done;

    /* Hold the owned composite across native garbage collection, then use it
     * again as input. The separate mutation fixture retains its larger schema
     * and size-limit tests without doubling their Aplet code in this fixture. */
    uint8_t text[1536];
    memset(text, 'G', sizeof(text));
    uint32_t previous = temp_top();
    int collected = 0;
    failure = 0x391u;
    for (uint32_t step = 0u; step < 128u; ++step) {
        if (hpclang_object_create_string(text, sizeof(text), &value) != 0 ||
            hpclang_aplet_data_set(value) != 0)
            goto done;
        hpclang_object_release(value);
        value = NULL;
        uint32_t current = temp_top();
        if (current == UINT32_MAX)
            goto done;
        if (current < previous)
            collected = 1;
        previous = current;
        if (collected)
            break;
    }
    if (!collected || !hpclang_e2e_composite_matches(readback) ||
        hpclang_aplet_data_set(readback) != 0)
        goto done;
    hpclang_object_release(readback);
    readback = NULL;
    if (hpclang_aplet_data_get(&readback) != 0 || !hpclang_e2e_composite_matches(readback))
        goto done;
    failure = 0x392u;
    if (!was_empty && hpclang_aplet_data_clear() != 0)
        goto done;
    hpclang_e2e_result = was_empty ? 0x4f530300u : 0x4f530301u;
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    failure = 0u;
done:
    hpclang_object_release(readback);
    hpclang_object_release(value);
    hpclang_object_release(saved);
    return failure;
}
