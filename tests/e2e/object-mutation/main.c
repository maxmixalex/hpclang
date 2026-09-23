#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/object.h>
#include <hpclang/object-store.h>
#include <hpclang/target.h>

int hpclang_object_store_replace_existing_same_size(
    const struct hpclang_object_store *store,
    const uint8_t *name,
    uint32_t name_length,
    const struct hpclang_object *object);

volatile uint32_t hpclang_e2e_result;

#define HPCLANG_OBJECT_PHASE UINT32_C(0x4f530000)

static void phase(uint32_t step)
{
    hpclang_e2e_result =
        HPCLANG_OBJECT_PHASE | (step & UINT32_C(0xffff));
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
    hpclang_surface_draw_text(
        surface,
        hpclang_font_mini(),
        28,
        28,
        failure == 0u ? "OBJECT PASS" : "OBJECT FAIL",
        255u);

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
    return model == HPCLANG_MODEL_HP48GII ||
           model == HPCLANG_MODEL_HP49GPLUS ||
           model == HPCLANG_MODEL_HP50G;
}

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL || !supported_model(boot->model))
        return finish(0xe0u);
    if (boot->saturn_state == 0u)
        return finish(0xe1u);

    static const uint8_t hp48_name[] = {'Z', 'Z', 'E', '2', 'E'};
    static const uint8_t hp48_value[] = "HPCLANG-E2E";
    static const uint8_t hp4950_name[] = {
        'X', '5', '0', 'N', 'G', '-', 'V', 'V', '.', 'F', 'A', 'T'
    };

    const uint8_t *name =
        boot->model == HPCLANG_MODEL_HP48GII ? hp48_name : hp4950_name;
    uint32_t name_length =
        boot->model == HPCLANG_MODEL_HP48GII
            ? (uint32_t)sizeof(hp48_name)
            : (uint32_t)sizeof(hp4950_name);
    size_t expected_size =
        boot->model == HPCLANG_MODEL_HP48GII
            ? sizeof(hp48_value) - 1u
            : 0u;

    uint32_t failure = 0u;
    struct hpclang_object_store *home = NULL;
    struct hpclang_object *before = NULL;
    struct hpclang_object *after = NULL;

    phase(1u);
    home = hpclang_object_store_home();
    if (home == NULL) {
        failure = 0xe2u;
        goto done;
    }

    phase(2u);
    if (hpclang_object_store_get(
            home, name, name_length, &before) != 0 ||
        before == NULL ||
        hpclang_object_kind(before) != HPCLANG_OBJECT_STRING) {
        failure = 0xe3u;
        goto done;
    }

    size_t before_size = 0u;
    if (hpclang_object_string_size(before, &before_size) != 0 ||
        before_size != expected_size) {
        failure = 0xe4u;
        goto done;
    }

    uint8_t expected[sizeof(hp48_value) - 1u];
    if (before_size != 0u) {
        if (hpclang_object_string_copy(
                before, expected, before_size) != 0 ||
            (boot->model == HPCLANG_MODEL_HP48GII &&
             memcmp(expected, hp48_value, before_size) != 0)) {
            failure = 0xe5u;
            goto done;
        }
    }

    phase(15u);
    if (hpclang_object_store_replace_existing_same_size(
            home, name, name_length, before) != 0) {
        failure = 0xe6u;
        goto done;
    }

    phase(16u);
    if (hpclang_object_store_get(
            home, name, name_length, &after) != 0 ||
        after == NULL ||
        hpclang_object_kind(after) != HPCLANG_OBJECT_STRING) {
        failure = 0xe7u;
        goto done;
    }

    size_t after_size = 0u;
    if (hpclang_object_string_size(after, &after_size) != 0 ||
        after_size != before_size) {
        failure = 0xe8u;
        goto done;
    }

    if (after_size != 0u) {
        uint8_t actual[sizeof(hp48_value) - 1u];
        if (hpclang_object_string_copy(after, actual, after_size) != 0 ||
            memcmp(expected, actual, after_size) != 0) {
            failure = 0xe9u;
            goto done;
        }
    }

done:
    hpclang_object_release(after);
    hpclang_object_release(before);
    hpclang_object_store_close(home);
    return finish(failure);
}
