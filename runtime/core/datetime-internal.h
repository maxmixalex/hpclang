#ifndef HPCLANG_DATETIME_INTERNAL_H
#define HPCLANG_DATETIME_INTERNAL_H
#include <hpclang/datetime.h>

int hpclang_datetime_valid(const struct hpclang_datetime *value);
/* No errno changes; suitable for the monotonic clock's internal sampling. */
int hpclang_rtc_read(struct hpclang_datetime *value);
/* Caller validates the date and preserves monotonic time around this write. */
int hpclang_rtc_write(const struct hpclang_datetime *value);
/* BCD sample: seconds before, year, month, day, hour, minute, seconds after. */
int hpclang_rtc_decode(const uint8_t raw[7], struct hpclang_datetime *value);
#endif
