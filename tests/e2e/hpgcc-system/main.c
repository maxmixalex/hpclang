#include <stdint.h>
#include <hpclang/time.h>

volatile uint32_t hpclang_e2e_result;
unsigned hpclang_historical_system(int (*checkpoint)(uint32_t, uint32_t));

static int checkpoint(uint32_t request, uint32_t acknowledgement)
{
    hpclang_e2e_result = request;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != acknowledgement)
        if (hpclang_time_monotonic_us() - start > 2000000u)
            return -1;
    return 0;
}

int main(void)
{
    unsigned failure = hpclang_historical_system(checkpoint);
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    return 0;
}
