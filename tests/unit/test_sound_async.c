#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <hpclang/device.h>
#include <hpclang/sound.h>
#include "sound-session.h"
#include "exit-hooks.h"
#include "sound-client.h"

struct hpclang_exit_hooks hpclang_exit_hooks;
static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};
static uint32_t cpu = 0xd0;
static uint64_t now = 100;
static unsigned reads, fail_read, starts, closes;
static int blocking, admission_error, rearm_error;
static struct hpclang_sound_session *current_session;
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_sound_blocking_active(void)
{
    return blocking;
}
uint32_t hpclang_irq_save_disable(void)
{
    uint32_t old = cpu;
    cpu |= 0xc0;
    return old;
}
void hpclang_irq_restore(uint32_t value)
{
    cpu = value;
}
void hpclang_irq_bank_exchange(struct hpclang_irq_bank *bank)
{
    (void)bank;
    assert(0);
}
void hpclang_sound_irq_entry(void)
{
}
void hpclang_sound_irq_dispatch(void);
void hpclang_sound_irq_fault(void);
int hpclang_time_read_us_masked(uint64_t *out)
{
    assert((cpu & 0xc0) == 0xc0);
    if (++reads == fail_read) {
        errno = EIO;
        return -1;
    }
    *out = now++;
    return 0;
}
void hpclang_clock_pclk(uint64_t *num, uint64_t *den)
{
    *num = 50000000;
    *den = 1;
}
int hpclang_sound_session_start(const struct hpclang_sound_session_bus *bus,
                                struct hpclang_sound_session *s, uint64_t num, uint64_t den,
                                uint32_t frequency, uint64_t time, uint64_t deadline,
                                uint32_t stack, uint32_t handler, int replace)
{
    (void)bus;
    (void)num;
    (void)den;
    (void)frequency;
    (void)time;
    assert((cpu & 0xc0) == 0xc0 && stack && !(stack & 7) && handler);
    assert(!s->irq.owned || s->state != SOUND_SESSION_RUNNING || replace);
    ++starts;
    current_session = s;
    memset(s, 0, sizeof *s);
    if (admission_error) {
        errno = admission_error;
        return -1;
    }
    s->irq.owned = s->alarm.owned = s->tone.owned = 1;
    s->state = SOUND_SESSION_RUNNING;
    s->deadline = deadline;
    return 0;
}
void hpclang_sound_session_close(const struct hpclang_sound_session_bus *bus,
                                 struct hpclang_sound_session *s)
{
    (void)bus;
    assert((cpu & 0xc0) == 0xc0);
    ++closes;
    memset(s, 0, sizeof *s);
}
int hpclang_watchdog_advance(const struct hpclang_watchdog_bus *bus,
                             struct hpclang_watchdog_alarm *alarm, uint64_t time, uint64_t deadline)
{
    (void)bus;
    assert(alarm->owned && deadline > time);
    if (rearm_error) {
        errno = rearm_error;
        return -1;
    }
    return 0;
}
void hpclang_sound_session_advance(const struct hpclang_sound_session_bus *bus,
                                   struct hpclang_sound_session *s, uint64_t time, int error)
{
    (void)bus;
    if (error || time >= s->deadline) {
        s->state = error ? SOUND_SESSION_FAILED : SOUND_SESSION_DONE;
        s->error = error;
        s->tone.owned = s->alarm.owned = 0;
    }
}
static void dispatch(void)
{
    uint32_t old = cpu;
    cpu |= 0xc0;
    hpclang_sound_irq_dispatch();
    cpu = old;
}
int main(void)
{
    void *page = mmap((void *)0x07f00000, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(page == (void *)0x07f00000);
    volatile uint32_t *mapping = (void *)0x07f001e0;
    *mapping = 0x53000c12;
    assert(hpclang_sound_status() == 0 && cpu == 0xd0);
    assert(hpclang_sound_start(0, 1, 0) == -1 && errno == EINVAL);
    assert(hpclang_sound_start(440, 0, 0) == -1 && errno == EINVAL);
    cpu = 0x50;
    assert(hpclang_sound_start(440, 1, 0) == -1 && errno == EBUSY && cpu == 0x50);
    cpu = 0xd0;
    blocking = 1;
    assert(hpclang_sound_start(440, 1, 0) == -1 && errno == EBUSY);
    blocking = 0;
    *mapping = 0;
    assert(hpclang_sound_start(440, 1, 0) == -1 && errno == ENOSYS && cpu == 0xd0);
    *mapping = 0x53000c12;
    assert(!hpclang_sound_start(440, 10, 0) && cpu == 0x50);
    assert(hpclang_exit_hooks.sound_async && hpclang_exit_hooks.sound_async_active());
    unsigned prior_reads = reads;
    assert(hpclang_sound_status() == 1 && reads == prior_reads && cpu == 0x50);
    assert(hpclang_sound_start(880, 10, 0) == -1 && errno == EBUSY && starts == 1);
    assert(!hpclang_sound_start(880, 10, 1) && cpu == 0x50);
    now = current_session->deadline;
    errno = EDOM;
    dispatch();
    assert(errno == EDOM && current_session->irq.owned);
    assert(!hpclang_sound_status() && cpu == 0xd0 && !hpclang_exit_hooks.sound_async);
    unsigned old_closes = closes;
    hpclang_sound_stop();
    assert(closes == old_closes && cpu == 0xd0);
    assert(!hpclang_sound_start(440, 10, 0));
    hpclang_exit_hooks.sound_async();
    assert(cpu == 0xd0 && !hpclang_exit_hooks.sound_async_active);
    assert(!hpclang_sound_start(440, 10, 0));
    fail_read = reads + 1;
    dispatch();
    assert(hpclang_sound_status() == -1 && errno == EIO && cpu == 0xd0);
    fail_read = reads + 2;
    assert(hpclang_sound_start(440, 10, 0) == -1 && errno == EIO && cpu == 0xd0);
    fail_read = 0;
    rearm_error = ERANGE;
    assert(hpclang_sound_start(440, 10, 0) == -1 && errno == ERANGE && cpu == 0xd0);
    rearm_error = 0;
    assert(!hpclang_sound_start(440, 10, 0));
    admission_error = EBUSY;
    assert(hpclang_sound_start(880, 10, 1) == -1 && errno == EBUSY && cpu == 0xd0);
    assert(!hpclang_exit_hooks.sound_async && !hpclang_sound_status());
    admission_error = 0;
    assert(!hpclang_sound_start(440, 10, 0));
    cpu |= 0xc0;
    errno = EDOM;
    hpclang_sound_irq_fault();
    assert(errno == EDOM);
    cpu &= ~0x80u;
    assert(hpclang_sound_status() == -1 && errno == EIO && cpu == 0xd0);
    static const int client;
    assert(!hpclang_sound_start_client(440, 10, 0, &client));
    assert(hpclang_sound_is_client(&client) && !hpclang_sound_is_client(0));
    assert(hpclang_sound_start(0, 10, 1) == -1 && hpclang_sound_is_client(&client));
    assert(!hpclang_sound_start(880, 10, 1) && !hpclang_sound_is_client(&client));
    hpclang_sound_stop();
    assert(!hpclang_sound_start_client(440, 10, 0, &client));
    hpclang_sound_stop();
    assert(!hpclang_sound_is_client(&client));
    now = UINT64_MAX;
    assert(hpclang_sound_start(440, 10, 0) == -1 && errno == EOVERFLOW && cpu == 0xd0);
    assert(!munmap(page, 4096));
    return 0;
}
