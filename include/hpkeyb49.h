#ifndef HPCLANG_HPKEYB49_H
#define HPCLANG_HPKEYB49_H

#ifdef __cplusplus
extern "C"
{
#endif
    /* Historical physical-state queries. Matrix coordinates are zero-based
     * hardware column/row, not character positions. Named queries follow the
     * current model: HP39's single Shift is LS; RS is absent. No shift/alpha latch
     * or translated text state is installed or changed by this adapter.
     * Invalid coordinates return 0/EINVAL; unknown models return 0/ENOSYS. */
    int keyb_isKeyPressed(unsigned char col, unsigned char row);
    int keyb_isAnyKeyPressed(void);
    int keyb_isLeft(void);
    int keyb_isRight(void);
    int keyb_isUp(void);
    int keyb_isDown(void);
    int keyb_isAlpha(void);
    int keyb_isLS(void);
    int keyb_isRS(void);
    int keyb_isON(void);
    /* Wait for any press followed by all keys released. An already-held key
     * qualifies. Does not consume the clean event queue or change RTC settings.
     * Unknown model returns immediately with ENOSYS. */
    void keyb_waitKeyPressed(void);
#ifdef __cplusplus
}
#endif
#endif
