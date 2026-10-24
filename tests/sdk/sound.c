#include <kos.h>
#include <hpclang/sound.h>

int main(void)
{
    S32 (*start)(U32, TIME_T, U32) = kos_beep;
    BOOL (*complete)(void) = kos_CheckBeepEnd;
    if (start(880, 10, 1))
        return 1;
    (void)complete();
    hpclang_sound_stop();
    return start(440, 10, 2);
}
