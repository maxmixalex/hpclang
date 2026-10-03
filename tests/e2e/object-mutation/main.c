#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/object.h>
#include <hpclang/object-store.h>
#include <hpclang/target.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_nested_mutation(void);
uint32_t hpclang_e2e_directory_mutation(void);
uint32_t hpclang_e2e_hp39_mutation(void);

#define HPCLANG_OBJECT_PHASE UINT32_C(0x4f530000)

static void phase(uint32_t step)
{
    hpclang_e2e_result = HPCLANG_OBJECT_PHASE | (step & UINT32_C(0xffff));
}

static void hold_success(void)
{
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
}

static void hold_failure(void)
{
    for (;;)
        __asm__ volatile("nop");
}

static int finish(uint32_t failure)
{
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, hpclang_font_mini(), 28, 28,
                              failure == 0u ? "OBJECT PASS" : "OBJECT FAIL", 255u);

    if (failure == 0u) {
        hpclang_e2e_result = UINT32_C(0x50415353);
        hold_success();
        return 0;
    }

    hpclang_e2e_result = UINT32_C(0x46410000) | failure;
    hold_failure();
    return (int)failure;
}

static int supported_model(uint32_t model)
{
    return model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
           model == HPCLANG_MODEL_HP50G;
}

static int same_entries(struct hpclang_object_store_iterator *before,
                        struct hpclang_object_store_iterator *after)
{
    struct hpclang_object_store_entry a = {.size = sizeof(a),
                                           .version = HPCLANG_OBJECT_STORE_ENTRY_VERSION};
    struct hpclang_object_store_entry b = {.size = sizeof(b),
                                           .version = HPCLANG_OBJECT_STORE_ENTRY_VERSION};
    for (;;) {
        int left = hpclang_object_store_iterator_read(before, &a);
        int right = hpclang_object_store_iterator_read(after, &b);
        if (left < 0 || right < 0 || left != right)
            return 0;
        if (left == 0)
            return 1;
        if (a.name_length != b.name_length || a.kind != b.kind || a.flags != b.flags ||
            memcmp(a.name, b.name, a.name_length) != 0)
            return 0;
    }
}

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot != NULL &&
        (boot->model == HPCLANG_MODEL_HP39GPLUS || boot->model == HPCLANG_MODEL_HP39GS ||
         boot->model == HPCLANG_MODEL_HP40GS))
        return finish(hpclang_e2e_hp39_mutation());
    if (boot == NULL || !supported_model(boot->model))
        return finish(0xe0u);
    if (boot->saturn_state == 0u)
        return finish(0xe1u);

    /* The shared USB harness transfers the same owned witness on all models. */
    static const uint8_t name[] = {'Z', 'Z', 'E', '2', 'E'};
    static const uint8_t initial_value[] = "HPCLANG-E2E";
    static const uint8_t replacement_bytes[] = "HPCLANG-E2E!";
    static const uint8_t create_name[] = {'Z', 'Z', 'M', '7', 'F'};
    static const uint8_t create_value[] = "M7F";
    const uint32_t name_length = (uint32_t)sizeof(name);
    const size_t expected_size = sizeof(initial_value) - 1u;
    const size_t replacement_size = sizeof(replacement_bytes) - 1u;

    uint32_t failure = 0u;
    struct hpclang_object_store *home = NULL;
    struct hpclang_object_store *current = NULL;
    struct hpclang_object_store_iterator *entries_before = NULL;
    struct hpclang_object_store_iterator *entries_after = NULL;
    struct hpclang_object *before = NULL;
    struct hpclang_object *replacement = NULL;
    struct hpclang_object *after = NULL;
    struct hpclang_object *restored = NULL;
    struct hpclang_object *created_input = NULL;
    struct hpclang_object *created_readback = NULL;

    phase(1u);
    home = hpclang_object_store_home();
    if (home == NULL) {
        failure = 0xe2u;
        goto done;
    }

    current = hpclang_object_store_current();
    if (current == NULL || (entries_before = hpclang_object_store_iterator_open(current)) == NULL) {
        failure = 0xfau;
        goto done;
    }
    hpclang_object_store_close(current);
    current = NULL;

    phase(2u);
    if (hpclang_object_store_get(home, name, name_length, &before) != 0 || before == NULL ||
        hpclang_object_kind(before) != HPCLANG_OBJECT_STRING) {
        failure = 0xe3u;
        goto done;
    }

    size_t before_size = 0u;
    if (hpclang_object_string_size(before, &before_size) != 0 || before_size != expected_size) {
        failure = 0xe4u;
        goto done;
    }

    uint8_t expected[sizeof(initial_value) - 1u];
    if (before_size != 0u) {
        if (hpclang_object_string_copy(before, expected, before_size) != 0 ||
            memcmp(expected, initial_value, before_size) != 0) {
            failure = 0xe5u;
            goto done;
        }
    }

    if (replacement_size != expected_size + 1u ||
        hpclang_object_create_string(replacement_bytes, replacement_size, &replacement) != 0 ||
        replacement == NULL) {
        failure = 0xe6u;
        goto done;
    }

    /*
     * First live USEROB-resize witness. DOCSTR grows by one calculator byte,
     * i.e. exactly two nibbles. The logical HOME handle must be resolved again
     * after the shift; no raw Saturn address is retained across either write.
     */
    phase(15u);
    if (hpclang_object_store_put(home, name, name_length, replacement,
                                 HPCLANG_OBJECT_STORE_UPSERT) != 0) {
        failure = 0xe7u;
        goto done;
    }

    phase(16u);
    if (hpclang_object_store_get(home, name, name_length, &after) != 0 || after == NULL ||
        hpclang_object_kind(after) != HPCLANG_OBJECT_STRING) {
        failure = 0xe8u;
        goto done;
    }

    size_t after_size = 0u;
    if (hpclang_object_string_size(after, &after_size) != 0 || after_size != replacement_size) {
        failure = 0xe9u;
        goto done;
    }

    uint8_t replacement_actual[sizeof(replacement_bytes) - 1u];
    if (replacement_size > sizeof(replacement_actual) ||
        hpclang_object_string_copy(after, replacement_actual, replacement_size) != 0 ||
        memcmp(replacement_actual, replacement_bytes, replacement_size) != 0) {
        failure = 0xeau;
        goto done;
    }
    hpclang_object_release(after);
    after = NULL;

    phase(17u);
    if (hpclang_object_store_put(home, name, name_length, before,
                                 HPCLANG_OBJECT_STORE_REPLACE_ONLY) != 0) {
        failure = 0xebu;
        goto done;
    }

    phase(18u);
    if (hpclang_object_store_get(home, name, name_length, &restored) != 0 || restored == NULL ||
        hpclang_object_kind(restored) != HPCLANG_OBJECT_STRING) {
        failure = 0xecu;
        goto done;
    }

    size_t restored_size = 0u;
    if (hpclang_object_string_size(restored, &restored_size) != 0 || restored_size != before_size) {
        failure = 0xedu;
        goto done;
    }

    if (restored_size != 0u) {
        uint8_t restored_actual[sizeof(initial_value) - 1u];
        if (restored_size > sizeof(restored_actual) ||
            hpclang_object_string_copy(restored, restored_actual, restored_size) != 0 ||
            memcmp(restored_actual, expected, restored_size) != 0) {
            failure = 0xeeu;
            goto done;
        }
    }
    phase(19u);

    if (hpclang_object_create_string(create_value, sizeof(create_value) - 1u, &created_input) !=
            0 ||
        created_input == NULL) {
        failure = 0xefu;
        goto done;
    }

    phase(20u);
    if (hpclang_object_store_put(home, create_name, (uint32_t)sizeof(create_name), created_input,
                                 HPCLANG_OBJECT_STORE_UPSERT) != 0) {
        failure = 0xf0u;
        goto done;
    }

    phase(21u);
    errno = 0;
    if (hpclang_object_store_put(home, create_name, sizeof(create_name), replacement,
                                 HPCLANG_OBJECT_STORE_CREATE_ONLY) != -1 ||
        errno != EEXIST) {
        failure = 0xf4u;
        goto done;
    }
    hpclang_object_release(created_input);
    created_input = NULL;

    phase(22u);
    if (hpclang_object_store_get(home, create_name, (uint32_t)sizeof(create_name),
                                 &created_readback) != 0 ||
        created_readback == NULL ||
        hpclang_object_kind(created_readback) != HPCLANG_OBJECT_STRING) {
        failure = 0xf1u;
        goto done;
    }

    size_t created_size = 0u;
    if (hpclang_object_string_size(created_readback, &created_size) != 0 ||
        created_size != sizeof(create_value) - 1u) {
        failure = 0xf2u;
        goto done;
    }

    uint8_t created_actual[sizeof(create_value) - 1u];
    if (hpclang_object_string_copy(created_readback, created_actual, created_size) != 0 ||
        memcmp(created_actual, create_value, created_size) != 0) {
        failure = 0xf3u;
        goto done;
    }

    phase(23u);
    if (hpclang_object_store_remove(home, create_name, sizeof(create_name)) != 0) {
        failure = 0xf5u;
        goto done;
    }
    errno = 0;
    if (hpclang_object_store_get(home, create_name, sizeof(create_name), &after) != -1 ||
        errno != ENOENT || after != NULL) {
        failure = 0xf6u;
        goto done;
    }
    errno = 0;
    if (hpclang_object_store_remove(home, create_name, sizeof(create_name)) != -1 ||
        errno != ENOENT) {
        failure = 0xf7u;
        goto done;
    }
    /* Deleting Stock storage must not invalidate an owned readback snapshot. */
    if (hpclang_object_string_copy(created_readback, created_actual, created_size) != 0 ||
        memcmp(created_actual, create_value, created_size) != 0) {
        failure = 0xf8u;
        goto done;
    }
    phase(24u);
    if (hpclang_object_store_get(home, name, name_length, &after) != 0 || after == NULL ||
        hpclang_object_string_size(after, &after_size) != 0 || after_size != expected_size ||
        hpclang_object_string_copy(after, expected, expected_size) != 0 ||
        memcmp(expected, initial_value, expected_size) != 0) {
        failure = 0xf9u;
        goto done;
    }
    phase(26u);
    failure = hpclang_e2e_directory_mutation();
    if (failure != 0u)
        goto done;
    failure = hpclang_e2e_nested_mutation();
    if (failure != 0u)
        goto done;
    phase(25u);
    current = hpclang_object_store_current();
    if (current == NULL || (entries_after = hpclang_object_store_iterator_open(current)) == NULL ||
        !same_entries(entries_before, entries_after)) {
        failure = 0xfbu;
        goto done;
    }

done:
    hpclang_object_store_iterator_close(entries_after);
    hpclang_object_store_iterator_close(entries_before);
    hpclang_object_store_close(current);
    hpclang_object_release(created_readback);
    hpclang_object_release(created_input);
    hpclang_object_release(restored);
    hpclang_object_release(after);
    hpclang_object_release(replacement);
    hpclang_object_release(before);
    hpclang_object_store_close(home);
    return finish(failure);
}
