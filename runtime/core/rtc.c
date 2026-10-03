#include <errno.h>
#include <hpclang/device.h>
#include "datetime-internal.h"

#define RTC_REG(offset) (*(volatile uint8_t *)(uintptr_t)(UINT32_C(0x07b00000) + (offset)))

int hpclang_rtc_read(struct hpclang_datetime *value)
{
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        uint8_t raw[7];
        raw[0] = RTC_REG(0x70);
        raw[1] = RTC_REG(0x88);
        raw[2] = RTC_REG(0x84);
        raw[3] = RTC_REG(0x7c);
        raw[4] = RTC_REG(0x78);
        raw[5] = RTC_REG(0x74);
        raw[6] = RTC_REG(0x70);
        if (hpclang_rtc_decode(raw, value))
            return 1;
    }
    return 0;
}

int hpclang_datetime_read(struct hpclang_datetime *value)
{
    if (!value) {
        errno = EINVAL;
        return -1;
    }
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    if (!hpclang_rtc_read(value)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static uint8_t encode_bcd(uint32_t value)
{
    return (uint8_t)((value / 10) * 16 + value % 10);
}

int hpclang_rtc_write(const struct hpclang_datetime *value)
{
    uint8_t control = RTC_REG(0x40);
    /* Test-clock modes or an existing clock-reset transaction are not ours. */
    if (control & 0x0e) {
        errno = EBUSY;
        return -1;
    }
    unsigned weekday;
    if (hpclang_datetime_weekday(value, &weekday) < 0)
        return -1;
    RTC_REG(0x40) = control | 9;
    RTC_REG(0x88) = encode_bcd(value->year % 100);
    RTC_REG(0x84) = encode_bcd(value->month);
    RTC_REG(0x7c) = encode_bcd(value->day);
    RTC_REG(0x80) = encode_bcd(weekday + 1);
    RTC_REG(0x78) = encode_bcd(value->hour);
    RTC_REG(0x74) = encode_bcd(value->minute);
    RTC_REG(0x70) = encode_bcd(value->second);
    RTC_REG(0x40) = control;
    return 0;
}
