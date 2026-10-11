#include <errno.h>
#include <stdint.h>
#include <hpclang/device.h>
#include <hpclang/display.h>

enum
{
    KOS_LCD_GET_CONTRAST = 13u,
    KOS_LCD_SET_CONTRAST = 127u
};

uint32_t hpclang_kos_sync3(uint32_t index, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static unsigned original;
static int owned;

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
    uint32_t value = hpclang_kos_sync3(KOS_LCD_GET_CONTRAST, 0, 0, 0);
    if (value > 15) {
        errno = EIO;
        return -1;
    }
    *level = value;
    return 0;
}

static int write_level(unsigned level)
{
    (void)hpclang_kos_sync3(KOS_LCD_SET_CONTRAST, level, 0, 0);
    unsigned actual;
    if (hpclang_display_contrast_read(&actual) || actual != level) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int hpclang_display_contrast_set(unsigned level)
{
    if (level > 15) {
        errno = EINVAL;
        return -1;
    }
    unsigned current;
    if (hpclang_display_contrast_read(&current))
        return -1;
    if (level == current)
        return 0;
    if (!owned) {
        original = current;
        owned = 1;
    }
    if (write_level(level)) {
        /* Retain ownership if verification failed, so explicit restore and
         * normal runtime cleanup can still restore the original setting. */
        (void)write_level(current);
        errno = EIO;
        return -1;
    }
    return 0;
}

int hpclang_display_contrast_restore(void)
{
    if (!owned)
        return 0;
    if (write_level(original))
        return -1;
    owned = 0;
    return 0;
}
