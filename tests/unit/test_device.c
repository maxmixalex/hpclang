#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpclang/abi.h>
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
uint32_t hpclang_kos_swi3(uint32_t index, uint32_t a, uint32_t b, uint32_t c)
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
    }
    boot.model = UINT32_MAX;
    assert(hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN);
    for (int capability = HPCLANG_CAPABILITY_DISPLAY;
         capability <= HPCLANG_CAPABILITY_CONTRAST_READ; ++capability)
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
