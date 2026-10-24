/* Included in the pinned emulator's watchdog translation unit. */
static int hpclang_watchdog_sync(s3c2410_watchdog_t *watchdog)
{
    int expired = hpclang_watchdog_advance(&watchdog->clock, timer_get_clock());
    watchdog->wtcnt = watchdog->clock.count;
    if (!expired)
        return 0;
    if (watchdog->wtcon & 4u)
        s3c2410_intc_assert(watchdog->hdw_state, INT_WDT, 0);
    if (watchdog->wtcon & 1u) {
        reset_modules(watchdog->hdw_state, HDW_RESET_WATCHDOG);
        cpu_reset(watchdog->hdw_state->cpu);
        return 1;
    }
    return 0;
}

static void hpclang_watchdog_schedule(s3c2410_watchdog_t *watchdog)
{
    uint64_t delay = hpclang_watchdog_delay(&watchdog->clock);
    int64_t now = timer_get_clock();
    if (delay == UINT64_MAX) {
        timer_del(watchdog->timer);
        return;
    }
    timer_mod(watchdog->timer,
              delay > (uint64_t)(INT64_MAX - now) ? INT64_MAX : now + (int64_t)delay);
}

static void s3c2410_watchdog_tick(void *data)
{
    s3c2410_watchdog_t *watchdog = data;
    if (!hpclang_watchdog_sync(watchdog))
        hpclang_watchdog_schedule(watchdog);
}

static int s3c2410_watchdog_update(s3c2410_watchdog_t *watchdog)
{
    watchdog->clock = (struct hpclang_watchdog_clock){
        .last_us = timer_get_clock(),
        .count = watchdog->wtcnt & 65535u,
        .reload = watchdog->wtdat & 65535u,
        .divider = (((watchdog->wtcon >> 8) & 255u) + 1u) * (16u << ((watchdog->wtcon >> 3) & 3u)),
        .hz = watchdog->hdw_state->PCLK,
        .enabled = !!(watchdog->wtcon & 32u)};
    hpclang_watchdog_schedule(watchdog);
    return 0;
}

void s3c2410_watchdog_clock_changed(hdw_t *hdw_state)
{
    s3c2410_watchdog_t *watchdog = hdw_state->s3c2410_watchdog;
    if (!watchdog || !watchdog->timer || watchdog->clock.hz == hdw_state->PCLK)
        return;
    if (hpclang_watchdog_sync(watchdog))
        return;
    watchdog->clock.hz = hdw_state->PCLK;
    hpclang_watchdog_schedule(watchdog);
}

static uint32_t s3c2410_watchdog_read(void *opaque, target_phys_addr_t offset)
{
    s3c2410_watchdog_t *watchdog = opaque;
    if (!S3C2410_OFFSET_OK(watchdog, offset))
        return UINT32_MAX;
    hpclang_watchdog_sync(watchdog);
    return *(S3C2410_OFFSET_ENTRY(watchdog, offset))->datap;
}

static void s3c2410_watchdog_write(void *opaque, target_phys_addr_t offset, uint32_t data)
{
    s3c2410_watchdog_t *watchdog = opaque;
    if (!S3C2410_OFFSET_OK(watchdog, offset) || hpclang_watchdog_sync(watchdog))
        return;
    *(S3C2410_OFFSET_ENTRY(watchdog, offset))->datap = data & 65535u;
    if (offset == S3C2410_WATCHDOG_WTDAT)
        watchdog->clock.reload = data & 65535u;
    else
        s3c2410_watchdog_update(watchdog);
}
