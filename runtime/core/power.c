#include <errno.h>
#include <stdint.h>
#include <hpclang/device.h>
#include <hpclang/power.h>

int hpclang_power_usb_present(void)
{
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    return (*(volatile uint32_t *)(uintptr_t)0x07a00054u & 2u) != 0;
}
