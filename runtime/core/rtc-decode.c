#include "datetime-internal.h"

static uint32_t bcd(uint8_t raw)
{
    unsigned lo = raw & 15u, hi = raw >> 4;
    return lo <= 9 && hi <= 9 ? hi * 10 + lo : UINT32_MAX;
}

int hpclang_rtc_decode(const uint8_t raw[7], struct hpclang_datetime *value)
{
    if (raw[0] != raw[6])
        return 0;
    uint32_t year = bcd(raw[1]);
    if (year > 99)
        return 0;
    struct hpclang_datetime result = {2000 + year,        bcd(raw[2] & 0x1f), bcd(raw[3] & 0x3f),
                                      bcd(raw[4] & 0x3f), bcd(raw[5] & 0x7f), bcd(raw[0] & 0x7f)};
    if (!hpclang_datetime_valid(&result))
        return 0;
    *value = result;
    return 1;
}
