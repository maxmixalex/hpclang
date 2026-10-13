#include <errno.h>
#include <stdint.h>
#include <hpsys.h>
#include <hpclang/datetime.h>
#include <hpclang/time.h>

void sys_sleep(unsigned int millis)
{
    hpclang_delay_us((uint64_t)millis * UINT64_C(1000));
}

void sys_waitTicks(unsigned int ticks)
{
    hpclang_delay_us(((uint64_t)ticks * 4u + 2u) / 3u);
}

int sys_RTC_seconds(void)
{
    struct hpclang_datetime now;
    if (hpclang_datetime_read(&now) != 0)
        return -1;
    return (int)(now.hour * 3600u + now.minute * 60u + now.second);
}

int sys_delta_seconds(int start, int end)
{
    if (start < 0 || start >= 86400 || end < 0 || end >= 86400) {
        errno = EINVAL;
        return -1;
    }
    return (end + 86400 - start) % 86400;
}
