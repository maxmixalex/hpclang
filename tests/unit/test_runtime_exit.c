#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <hpclang/abi.h>

unsigned char __tls_base[8];
static char trace[32];
static size_t used;
static unsigned scenario;
static int status;

int hpclang_start(const struct hpclang_boot_info *boot);
__attribute__((noreturn)) void fixture_immediate_exit(int value);

static void event(char value)
{
    assert(used + 1 < sizeof trace);
    trace[used++] = value;
    trace[used] = 0;
}

void _set_tls(void *tls)
{
    assert(tls == __tls_base);
    event('S');
}
void hpclang_time_init(void)
{
    event('T');
}
void hpclang_sound_restore(void)
{
    event('O');
}
void hpclang_display_end(void)
{
    event('D');
}
void hpclang_power_restore(void)
{
    event('P');
}
void hpclang_storage_shutdown(void)
{
    event('N');
}

__attribute__((noreturn)) void fixture_exit(int value)
{
    /* ARM E2E tests exercise actual picolibc callbacks and finalizers. */
    event('F');
    fixture_immediate_exit(value);
}

void __libc_init_array(void)
{
    event('I');
    if (scenario == 3)
        fixture_exit(status);
    if (scenario == 4)
        fixture_immediate_exit(status);
}

static void nested(void)
{
    if (scenario == 1)
        fixture_exit(status);
    if (scenario == 2)
        fixture_immediate_exit(status);
}

int fixture_main(void)
{
    event('M');
    nested();
    assert(scenario == 0);
    return status;
}

int main(void)
{
    static const char *const expected[] = {"STIMFODPN", "STIMFODPN", "STIMODPN", "STIFODPN",
                                           "STIODPN"};
    const int statuses[] = {0, 1, -1, INT_MIN, INT_MAX};
    const struct hpclang_boot_info boot = {0};
    for (scenario = 0; scenario < sizeof expected / sizeof expected[0]; ++scenario) {
        for (size_t i = 0; i < sizeof statuses / sizeof statuses[0]; ++i) {
            status = statuses[i];
            used = 0;
            trace[0] = 0;
            assert(hpclang_start(&boot) == status);
            assert(hpclang_boot_info() == &boot);
            assert(strcmp(trace, expected[scenario]) == 0);
        }
    }
    return 0;
}
