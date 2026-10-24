#include <errno.h>
#include <stdint.h>
#include <kos.h>
#include <hpclang/sound.h>
#include <hpclang/time.h>

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
unsigned hpclang_e2e_kos_sound_test(void)
{
    unsigned failure = 0;
    for (unsigned mode = 0; mode < 16; ++mode) {
        uint64_t before, after;
        if (hpclang_time_read_us(&before) || kos_beep(880, 10, mode) ||
            hpclang_time_read_us(&after)) {
            failure = 1;
            goto done;
        }
        if (mode & 2u) {
            if (after - before < 10000 || hpclang_sound_status() != 0 || !kos_CheckBeepEnd()) {
                failure = 2;
                goto done;
            }
            continue;
        }
        /* A long-lived request makes the overlap checks independent of the
         * emulator's instruction/wall-clock ratio. */
        hpclang_sound_stop();
        if (kos_beep(880, 60000, mode) || !kos_CheckBeepEnd() || hpclang_sound_status() != 1) {
            failure = 3;
            goto done;
        }
        uint32_t period = REG(0x07600024);
        if (kos_beep(0, 1, 3) != -51 || kos_beep(15001, 1, 3) != -51 ||
            kos_beep(880, 0, 3) != -51 || errno != EINVAL || REG(0x07600024) != period) {
            failure = 4;
            goto done;
        }
        if (kos_beep(440, 60000, 0) || hpclang_sound_status() != 1 ||
            ((REG(0x07600024) != period) != !!(mode & 1u))) {
            failure = 5;
            goto done;
        }
        hpclang_sound_stop();
        if (kos_beep(880, 10, mode)) {
            failure = 6;
            goto done;
        }
        errno = EDOM;
        for (volatile unsigned i = 0; i < 20000000 && (REG(0x07600008) & 0x1000u); ++i)
            __asm__ volatile("nop");
        if (errno != EDOM || hpclang_sound_status() != 0 || !kos_CheckBeepEnd()) {
            failure = 7;
            goto done;
        }
    }
    if (kos_beep(880, 60000, 1) || hpclang_sound_start(440, 60000, 1) ||
        kos_beep(880, 10, 3) != -1 || errno != EBUSY || hpclang_sound_status() != 1)
        failure = 8;
done:
    hpclang_sound_stop();
    return failure;
}
