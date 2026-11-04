
#include <hpclang/display.h>
#include <hpclang/time.h>
#include <stdint.h>
volatile uint32_t hpclang_e2e_result;
static unsigned presented, injected;
int notebook_application_main(void);
int __real_hpclang_display_present(uint32_t timeout);
static int mailbox(uint32_t request, uint32_t ack)
{
    hpclang_e2e_result = request;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != ack)
        if (hpclang_time_monotonic_us() - start > 2000000)
            return -1;
    return 0;
}
int __wrap_hpclang_display_present(uint32_t timeout)
{
    int result = __real_hpclang_display_present(timeout);
    if (!result && !presented++) {
        /* Retain the real focused editor frame before electrical ON input. */
        if (mailbox(0x46520001u, 0x464b0001u) || mailbox(0x4b500138u, 0x4b510138u))
            return -1;
        injected = 1;
    }
    return result;
}
int main(void)
{
    int status = notebook_application_main();
    hpclang_delay_us(300000); /* Let the scheduled GPIO release finish. */
    unsigned failure = status || !injected || !presented;
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0);
    hpclang_surface_fill_rect(surface, 20, 28, 90, 8, 255);
    hpclang_e2e_result = failure ? 0x46417f01u : 0x50415353u;
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
    return 0;
}
