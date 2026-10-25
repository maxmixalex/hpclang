/* External observation of the unchanged documentation application's ON wait. */
#include <hpgraphics.h>
#include <hpkeyb49.h>
#include <hpclang/keyboard.h>
#include <hpclang/time.h>
#include <stdint.h>

int historical_main(void);
int __real_keyb_isON(void);
volatile uint32_t hpclang_e2e_result;
static unsigned failure, waits;

static int handshake(uint32_t request, uint32_t ack)
{
    uint64_t start = hpclang_time_monotonic_us();
    hpclang_e2e_result = request;
    while (hpclang_e2e_result != ack)
        if (hpclang_time_monotonic_us() - start > 2000000) {
            failure = 1;
            return -1;
        }
    return 0;
}
static int frame(unsigned id)
{
    return handshake(UINT32_C(0x46520000) | id, UINT32_C(0x464b0000) | id);
}
static int on(unsigned down)
{
    unsigned argument = HPCLANG_KEYBOARD_ON | (down << 8);
    return handshake(UINT32_C(0x4b490000) | argument, UINT32_C(0x4b4a0000) | argument);
}
int __wrap_keyb_isON(void)
{
    ++waits;
    if (waits != 1 || __real_keyb_isON()) {
        failure = 2;
        return 1;
    }
    if (frame(2200) || on(1))
        return 1;
    int pressed = __real_keyb_isON();
    if (!pressed)
        failure = 3;
    return 1; /* A failed physical-key check still lets the app reach the failure report. */
}
int main(void)
{
    if (!frame(50)) {
        uint64_t start = hpclang_time_monotonic_us();
        while (keyb_isAnyKeyPressed())
            if (hpclang_time_monotonic_us() - start > 2000000) {
                failure = 4;
                break;
            }
        if (!failure)
            (void)historical_main();
    }
    if (!failure && (waits != 1 || on(0) || __real_keyb_isON()))
        failure = 5;
    hpclang_e2e_result = failure ? UINT32_C(0x46417e00) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
    /* As with process exit, release the original application's HPG ownership. */
    hpg_cleanup();
    return 0;
}
