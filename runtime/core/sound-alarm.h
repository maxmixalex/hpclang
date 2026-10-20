#ifndef HPCLANG_SOUND_ALARM_H
#define HPCLANG_SOUND_ALARM_H
#include <stdint.h>

enum hpclang_sound_alarm_reg
{
    ALARM_CONFIG,
    ALARM_MUX,
    ALARM_CONTROL,
    ALARM_COUNT,
    ALARM_CURRENT,
    ALARM_MASK,
    ALARM_SOURCE,
    ALARM_PENDING
};
struct hpclang_sound_alarm_bus
{
    void *context;
    uint32_t (*read)(void *, enum hpclang_sound_alarm_reg);
    void (*write)(void *, enum hpclang_sound_alarm_reg, uint32_t);
};
struct hpclang_sound_alarm
{
    uint32_t mux, control, count, current, mask;
    uint64_t clock_num, clock_den;
    int owned;
};

/* Internal PWM4 one-shot deadline lease, separate from PWM2 audio output.
 * Caller serializes access with IRQ/FIQ masked and keeps PCLK/prescaler stable.
 * PWM4 has no output pin. Reject active/update/DMA/pending owners before writing. Acquire
 * starts a one-shot with its IRQ masked: install an owned handler/stack and
 * routing before unmasking IRQ14. No Stock handler may run in the app mapping.
 * Delays exceeding one 16-bit period use a maximum-length chunk. The handler
 * must check the absolute deadline and rearm until due, then stop the sound.
 * This engine does not install/enable an IRQ handler or supply that deadline.
 * Arithmetic errors and invalid arguments leave registers/ownership unchanged.
 * Rearm preserves the current source mask and acknowledges only IRQ14. Release
 * restores the stopped counter, reload, mux, control and original source mask;
 * it clears only newly generated IRQ14 pending state. Other channels, the
 * shared prescaler and GPIO configuration are untouched. Release is idempotent.
 */
int hpclang_sound_alarm_acquire(const struct hpclang_sound_alarm_bus *bus,
                                struct hpclang_sound_alarm *alarm, uint64_t clock_num,
                                uint64_t clock_den, uint32_t delay_us);
int hpclang_sound_alarm_rearm(const struct hpclang_sound_alarm_bus *bus,
                              struct hpclang_sound_alarm *alarm, uint32_t delay_us);
void hpclang_sound_alarm_release(const struct hpclang_sound_alarm_bus *bus,
                                 struct hpclang_sound_alarm *alarm);
#endif
