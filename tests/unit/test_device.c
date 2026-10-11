#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/power.h>

static struct hpclang_boot_info boot;
static int present;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return present ? &boot : NULL;
}

static uint32_t contrast;
static struct hpclang_hp39_platform_context portal = {.magic = HPCLANG_HP39_PLATFORM_MAGIC,
                                                      .portal_entry = 1u};
static struct hpclang_rpl_memory_bounds bounds = {.magic = HPCLANG_RPL_BOUNDS_MAGIC,
                                                  .temp_limit = 0xe0000u,
                                                  .user_limit = 0xf0000u,
                                                  .stack_reserve_nibbles = 10u};

static void mutation_state(int expected)
{
    errno = EDOM;
    assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_STACK_MUTATION) == expected);
    assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION) == expected);
    assert(errno == EDOM);
}

static void mutation_context_test(void)
{
    boot.platform_cookie = 0u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    boot.platform_cookie = HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    boot.platform_cookie = (uintptr_t)&bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    mutation_state(HPCLANG_CAPABILITY_SUPPORTED);
    struct hpclang_rpl_memory_bounds saved = bounds;
    bounds.magic = 0u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds = saved;
    bounds.temp_limit = 0x80000u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds.temp_limit = 0x100001u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds = saved;
    bounds.user_limit = 0x7ffffu;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds.user_limit = 0x100000u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds = saved;
    bounds.stack_reserve_nibbles = 1u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds.stack_reserve_nibbles = UINT32_MAX;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    bounds = saved;
    boot.saturn_state = 0u;
    mutation_state(HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE);
    boot.saturn_state = 1u;
    mutation_state(HPCLANG_CAPABILITY_SUPPORTED);
}
uint32_t hpclang_kos_query3(uint32_t index, uint32_t a, uint32_t b, uint32_t c)
{
    assert(index == 13 && a == 0 && b == 0 && c == 0);
    return contrast;
}

int main(void)
{
    assert(hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN);
    assert(hpclang_device_capability(HPCLANG_CAPABILITY_SOUND) == HPCLANG_CAPABILITY_UNSUPPORTED);
    present = 1;
    for (unsigned model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        boot.model = model;
        assert(hpclang_device_info()->model == model);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_SOUND) == HPCLANG_CAPABILITY_SUPPORTED);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_DISPLAY_MODES) ==
               HPCLANG_CAPABILITY_SUPPORTED);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_DISPLAY_INDICATORS) ==
               HPCLANG_CAPABILITY_SUPPORTED);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_CIVIL_TIME_READ) ==
               HPCLANG_CAPABILITY_SUPPORTED);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_STREAM_STORAGE) ==
               (model >= HPCLANG_MODEL_HP49GPLUS ? HPCLANG_CAPABILITY_SUPPORTED
                                                 : HPCLANG_CAPABILITY_UNSUPPORTED));
        boot.saturn_state = 0;
        int absent = model >= HPCLANG_MODEL_HP48GII ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE
                                                    : HPCLANG_CAPABILITY_UNSUPPORTED;
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_STACK) == absent);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE) == absent);
        boot.saturn_state = 1;
        int available = model >= HPCLANG_MODEL_HP48GII ? HPCLANG_CAPABILITY_SUPPORTED
                                                       : HPCLANG_CAPABILITY_UNSUPPORTED;
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_STACK) == available);
        assert(hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE) == available);
        if (model >= HPCLANG_MODEL_HP48GII)
            mutation_context_test();
        else
            mutation_state(HPCLANG_CAPABILITY_UNSUPPORTED);
        for (int capability = HPCLANG_CAPABILITY_APLET_DATA;
             capability <= HPCLANG_CAPABILITY_APLET_CATALOG; ++capability) {
            int hp39 = model <= HPCLANG_MODEL_HP40GS;
            boot.platform_cookie = 0u;
            assert(
                hpclang_device_capability((enum hpclang_capability)capability) ==
                (hp39 ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE : HPCLANG_CAPABILITY_UNSUPPORTED));
            boot.platform_cookie = HPCLANG_PLATFORM_COOKIE_HP39_TAG;
            assert(
                hpclang_device_capability((enum hpclang_capability)capability) ==
                (hp39 ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE : HPCLANG_CAPABILITY_UNSUPPORTED));
            boot.platform_cookie = (uintptr_t)&portal | HPCLANG_PLATFORM_COOKIE_HP39_TAG;
            portal.magic = 0u;
            assert(
                hpclang_device_capability((enum hpclang_capability)capability) ==
                (hp39 ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE : HPCLANG_CAPABILITY_UNSUPPORTED));
            portal.magic = HPCLANG_HP39_PLATFORM_MAGIC;
            portal.portal_entry = 0u;
            assert(
                hpclang_device_capability((enum hpclang_capability)capability) ==
                (hp39 ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE : HPCLANG_CAPABILITY_UNSUPPORTED));
            portal.portal_entry = 1u;
            assert(hpclang_device_capability((enum hpclang_capability)capability) ==
                   (hp39 ? HPCLANG_CAPABILITY_SUPPORTED : HPCLANG_CAPABILITY_UNSUPPORTED));
            boot.saturn_state = 0u;
            assert(
                hpclang_device_capability((enum hpclang_capability)capability) ==
                (hp39 ? HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE : HPCLANG_CAPABILITY_UNSUPPORTED));
            boot.saturn_state = 1u;
        }
    }
    boot.model = UINT32_MAX;
    assert(hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN);
    for (int capability = HPCLANG_CAPABILITY_DISPLAY;
         capability <= HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION; ++capability)
        assert(hpclang_device_capability((enum hpclang_capability)capability) ==
               HPCLANG_CAPABILITY_UNSUPPORTED);
    assert(hpclang_device_capability((enum hpclang_capability)0) == -1 && errno == EINVAL);
    assert(hpclang_device_capability((enum hpclang_capability)999) == -1 && errno == EINVAL);
    unsigned level = 99;
    assert(hpclang_display_contrast_read(&level) == -1 && errno == ENOSYS && level == 99);
    assert(hpclang_power_usb_present() == -1 && errno == ENOSYS);
    boot.model = HPCLANG_MODEL_HP50G;
    contrast = 8;
    assert(hpclang_display_contrast_read(&level) == 0 && level == 8);
    contrast = 16;
    assert(hpclang_display_contrast_read(&level) == -1 && errno == EIO && level == 8);
    assert(hpclang_display_contrast_read(NULL) == -1 && errno == EINVAL);
    return 0;
}
