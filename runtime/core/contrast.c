#include <errno.h>
#include <stdint.h>
#include <hpclang/device.h>
#include <hpclang/display.h>

enum
{
    KOS_LCD_GET_CONTRAST = 13u
};

uint32_t hpclang_kos_query3(uint32_t index, uint32_t arg1, uint32_t arg2, uint32_t arg3);

int hpclang_display_contrast_read(unsigned *level)
{
    if (!level) {
        errno = EINVAL;
        return -1;
    }
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    uint32_t value = hpclang_kos_query3(KOS_LCD_GET_CONTRAST, 0, 0, 0);
    if (value > 15) {
        errno = EIO;
        return -1;
    }
    *level = value;
    return 0;
}
