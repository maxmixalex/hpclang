#include <assert.h>
#include <stdint.h>
#include "watchdog-clock.h"

typedef struct
{
    int64_t expires;
    int active;
} hdw_timer_t;
typedef struct
{
    uint32_t PCLK;
    void *s3c2410_watchdog;
    void *cpu;
} hdw_t;
typedef uint32_t target_phys_addr_t;
typedef struct
{
    uint32_t *datap;
} fake_reg;
typedef struct
{
    uint32_t wtcon, wtdat, wtcnt;
    fake_reg regs[3];
    hdw_t *hdw_state;
    struct hpclang_watchdog_clock clock;
    hdw_timer_t *timer;
} s3c2410_watchdog_t;
#define S3C2410_OFFSET_OK(w, off) ((off) <= 8 && !((off) & 3))
#define S3C2410_OFFSET_ENTRY(w, off) (&(w)->regs[(off) / 4])
#define S3C2410_WATCHDOG_WTDAT 4
#define INT_WDT 9
#define HDW_RESET_WATCHDOG 1
static int64_t now;
static unsigned interrupts, resets, cpu_resets;
static int s3c2410_watchdog_update(s3c2410_watchdog_t *watchdog);
static int64_t timer_get_clock(void)
{
    return now;
}
static void timer_mod(hdw_timer_t *t, int64_t expires)
{
    t->active = 1;
    t->expires = expires;
}
static void timer_del(hdw_timer_t *t)
{
    t->active = 0;
}
static void s3c2410_intc_assert(hdw_t *h, int irq, int level)
{
    (void)h;
    assert(irq == INT_WDT && !level);
    ++interrupts;
}
static void reset_modules(hdw_t *h, int reason)
{
    assert(reason == HDW_RESET_WATCHDOG);
    ++resets;
    s3c2410_watchdog_t *w = h->s3c2410_watchdog;
    w->wtcon = 0x910; /* Model the firmware disabling the reset watchdog. */
    s3c2410_watchdog_update(w);
}
static void cpu_reset(void *cpu)
{
    (void)cpu;
    ++cpu_resets;
}
#include "watchdog-model.inc"

int main(void)
{
    hdw_timer_t timer = {0};
    hdw_t hw = {.PCLK = 60000000};
    s3c2410_watchdog_t w = {
        .wtcon = 0x924, .wtdat = 1000, .wtcnt = 1000, .hdw_state = &hw, .timer = &timer};
    hw.s3c2410_watchdog = &w;
    w.regs[0].datap = &w.wtcon;
    w.regs[1].datap = &w.wtdat;
    w.regs[2].datap = &w.wtcnt;
    s3c2410_watchdog_update(&w);
    assert(timer.active && timer.expires == 2667);
    now = 100;
    assert(s3c2410_watchdog_read(&w, 8) == 963);
    s3c2410_watchdog_write(&w, 4, 2000);
    assert(w.wtcnt == 963 && timer.expires == 2667);
    now = 2667;
    s3c2410_watchdog_tick(&w);
    assert(interrupts == 1 && !resets && w.wtcnt == 2000 && timer.expires == 8000);
    now = 3000;
    s3c2410_watchdog_write(&w, 0, 0x904);
    assert(!timer.active && w.wtcnt == 1875);
    now = 10000;
    assert(s3c2410_watchdog_read(&w, 8) == 1875);
    s3c2410_watchdog_write(&w, 8, 1000);
    s3c2410_watchdog_write(&w, 0, 0x924);
    now = 10100;
    hw.PCLK = 12000000;
    s3c2410_watchdog_clock_changed(&hw);
    assert(w.wtcnt == 963 && timer.expires == 22934);
    now = timer.expires;
    s3c2410_watchdog_tick(&w);
    assert(interrupts == 2 && !resets);
    s3c2410_watchdog_write(&w, 0, 0x900);
    s3c2410_watchdog_write(&w, 8, 0x10001);
    assert(w.wtcnt == 1);
    s3c2410_watchdog_write(&w, 0, 0x921);
    now = timer.expires;
    s3c2410_watchdog_tick(&w);
    assert(resets == 1 && cpu_resets == 1 && interrupts == 2 && !timer.active);
    assert(s3c2410_watchdog_read(&w, 12) == UINT32_MAX);
    uint32_t count = w.wtcnt;
    s3c2410_watchdog_write(&w, 12, 7);
    assert(w.wtcnt == count);
    return 0;
}
