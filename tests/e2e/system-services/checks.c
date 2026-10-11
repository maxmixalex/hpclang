#include <errno.h>
#include <float.h>
#include <stdint.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/datetime.h>
#include <hpclang/device.h>
#include <hpclang/input.h>
#include <hpclang/display.h>
#include <hpclang/power.h>
#include <hpclang/timer.h>
#include <hpclang/sound.h>
#include <hpclang/time.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

unsigned hpclang_e2e_console_test(void);

static int numeric_line(struct hpclang_text_input *input, char *buffer, size_t capacity,
                        const char *text)
{
    if (hpclang_text_input_init(input, buffer, capacity) != 0)
        return -1;
    for (; *text; ++text)
        if (hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, (unsigned char)*text) !=
            HPCLANG_INPUT_EDITING)
            return -1;
    return 0;
}

static unsigned numeric_test(void)
{
    char buffer[32];
    struct hpclang_text_input input;
    int64_t whole = 7;
    double real = 7.0;
    if (numeric_line(&input, buffer, sizeof(buffer), "-9223372036854775808") != 0 ||
        hpclang_text_input_parse_int64(&input, &whole) != 0 || whole != INT64_MIN)
        return 1u;
    if (numeric_line(&input, buffer, sizeof(buffer), "9223372036854775808") != 0 ||
        hpclang_text_input_parse_int64(&input, &whole) != -1 || errno != ERANGE ||
        whole != INT64_MIN)
        return 2u;
    if (numeric_line(&input, buffer, sizeof(buffer), "-1.25e") != 0 ||
        hpclang_text_input_edit(&input, HPCLANG_INPUT_ACCEPT, 0) != HPCLANG_INPUT_ACCEPTED ||
        hpclang_text_input_parse_double(&input, &real) != -1 || errno != EINVAL || real != 7.0)
        return 3u;
    struct hpclang_keyboard_event key = {.type = HPCLANG_KEYBOARD_EVENT_DOWN, .key = HPCLANG_KEY_2};
    if (hpclang_text_input_key(&input, &key) != HPCLANG_INPUT_EDITING ||
        hpclang_text_input_parse_double(&input, &real) != 0 || real != -125.0 ||
        strcmp(buffer, "-1.25e2") != 0)
        return 4u;
    if (numeric_line(&input, buffer, sizeof(buffer), "4.9406564584124654e-324") != 0 ||
        hpclang_text_input_parse_double(&input, &real) != 0 || real != DBL_TRUE_MIN)
        return 5u;
    if (numeric_line(&input, buffer, sizeof(buffer), "1e-324") != 0 ||
        hpclang_text_input_parse_double(&input, &real) != -1 || errno != ERANGE ||
        real != DBL_TRUE_MIN)
        return 6u;
    if (numeric_line(&input, buffer, sizeof(buffer), "1e309") != 0 ||
        hpclang_text_input_parse_double(&input, &real) != -1 || errno != ERANGE ||
        real != DBL_TRUE_MIN)
        return 7u;
    if (numeric_line(&input, buffer, sizeof(buffer), "12x") != 0 ||
        hpclang_text_input_parse_double(&input, &real) != -1 || errno != EINVAL)
        return 8u;
    return 0u;
}

static int cancel_tone(void *context)
{
    int *observed = context;
    if ((REG32(0x07600008) & 0xf000) == 0x9000 && (REG32(0x07a00010) & 0x30) == 0x20) {
        errno = 0;
        if (hpclang_sound_play(880, 1, 0, 0) == -1 && errno == EBUSY)
            *observed = 1;
    }
    return 1;
}

static int sound_test(void)
{
    static const uintptr_t addresses[] = {0x07600000, 0x07600004, 0x07600008,
                                          0x07600024, 0x07600028, 0x0760002c,
                                          0x07a00010, 0x07a00018, 0x07100008};
    uint32_t before[sizeof(addresses) / sizeof(addresses[0])];
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i)
        before[i] = REG32(addresses[i]);
    int observed = 0;
    int cancelled = hpclang_sound_play(880, 1000, cancel_tone, &observed);
    if (cancelled != 1)
        return 0x100 + errno;
    if (!observed)
        return 1;
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i)
        if (REG32(addresses[i]) != before[i])
            return 2;
    uint64_t start, end;
    if (hpclang_time_read_us(&start) != 0)
        return 0x200 + errno;
    if (hpclang_sound_play(1000, 5, 0, 0) != 0)
        return 0x300 + errno;
    if (hpclang_time_read_us(&end) != 0)
        return 0x400 + errno;
    if (end - start < 5000)
        return 6;
    if (end - start > 200000)
        return 3;
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i)
        if (REG32(addresses[i]) != before[i])
            return 4;
    errno = 0;
    if (hpclang_sound_play(0, 1, 0, 0) != -1 || errno != EINVAL)
        return 5;
    return 0;
}

uint32_t hpclang_e2e_system_test(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    uint32_t family = hpclang_device_info()->family;
    int live = boot != NULL && boot->saturn_state != 0u ? HPCLANG_CAPABILITY_SUPPORTED
                                                        : HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE;
    int classic = family == HPCLANG_FAMILY_HP48 || family == HPCLANG_FAMILY_HP4950;
    const enum hpclang_capability classic_services[] = {
        HPCLANG_CAPABILITY_RPL_STACK, HPCLANG_CAPABILITY_RPL_OBJECT_STORE,
        HPCLANG_CAPABILITY_RPL_STACK_MUTATION, HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION};
    for (unsigned i = 0u; i < sizeof(classic_services) / sizeof(classic_services[0]); ++i)
        if (hpclang_device_capability(classic_services[i]) !=
            (classic ? live : HPCLANG_CAPABILITY_UNSUPPORTED))
            return 0x7c;
    for (int id = HPCLANG_CAPABILITY_APLET_DATA; id <= HPCLANG_CAPABILITY_APLET_CATALOG; ++id)
        if (hpclang_device_capability((enum hpclang_capability)id) !=
            (family == HPCLANG_FAMILY_HP39 ? live : HPCLANG_CAPABILITY_UNSUPPORTED))
            return 0x7d;

    unsigned console_failure = hpclang_e2e_console_test();
    if (console_failure != 0u)
        return 0xb0u + console_failure;
    unsigned numeric_failure = numeric_test();
    if (numeric_failure != 0u)
        return 0xa0u + numeric_failure;
    if (hpclang_device_capability(HPCLANG_CAPABILITY_CIVIL_TIME_READ) !=
            HPCLANG_CAPABILITY_SUPPORTED ||
        hpclang_device_capability(HPCLANG_CAPABILITY_POLL_TIMERS) != HPCLANG_CAPABILITY_SUPPORTED ||
        hpclang_device_capability(HPCLANG_CAPABILITY_SOUND) != HPCLANG_CAPABILITY_SUPPORTED)
        return 0x77;

    char input_buffer[8];
    struct hpclang_text_input input;
    struct hpclang_keyboard_event digit = {.type = HPCLANG_KEYBOARD_EVENT_DOWN,
                                           .key = HPCLANG_KEY_7};
    if (hpclang_text_input_init(&input, input_buffer, sizeof(input_buffer)) != 0 ||
        hpclang_text_input_key(&input, &digit) != HPCLANG_INPUT_EDITING ||
        hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0x4e2d) != HPCLANG_INPUT_EDITING ||
        input.length != 4 ||
        hpclang_text_input_edit(&input, HPCLANG_INPUT_BACKSPACE, 0) != HPCLANG_INPUT_EDITING ||
        strcmp(input_buffer, "7") != 0 ||
        hpclang_text_input_edit(&input, HPCLANG_INPUT_ACCEPT, 0) != HPCLANG_INPUT_ACCEPTED ||
        hpclang_text_input_edit(&input, HPCLANG_INPUT_CANCEL, 0) != HPCLANG_INPUT_CANCELLED)
        return 0x7b;

    unsigned contrast, contrast_again;
    uint32_t interrupt_mode = REG32(0x07100004);
    uint32_t interrupt_mask = REG32(0x07100008);
    uint32_t interrupt_pending = REG32(0x07100000);
    if (hpclang_display_contrast_read(&contrast) != 0 || contrast > 15 ||
        hpclang_display_contrast_read(&contrast_again) != 0 || contrast != contrast_again ||
        REG32(0x07100004) != interrupt_mode || REG32(0x07100008) != interrupt_mask ||
        (REG32(0x07100000) & interrupt_pending) != interrupt_pending)
        return 0x79;
    uint32_t gpio_config = REG32(0x07a00050);
    uint32_t gpio_pullup = REG32(0x07a00058);
    int usb = hpclang_power_usb_present();
    if (usb < 0 || usb > 1 || REG32(0x07a00050) != gpio_config || REG32(0x07a00058) != gpio_pullup)
        return 0x78;
    struct hpclang_timer periodic = {0};
    uint64_t expirations;
    if (hpclang_timer_start(&periodic, 1000, 1000) != 0)
        return 0x75;
    uint64_t time_before = hpclang_time_monotonic_us();
    hpclang_delay_us(3000u);
    uint64_t time_after = hpclang_time_monotonic_us();
    if (time_after < time_before || time_after - time_before < 3000u ||
        time_after - time_before > 200000u)
        return 0x0f;
    if (hpclang_timer_poll(&periodic, &expirations) != 0 || expirations < 3)
        return 0x75;
    hpclang_timer_cancel(&periodic);
    if (hpclang_timer_poll(&periodic, &expirations) != 0 || expirations != 0)
        return 0x75;

    struct hpclang_datetime civil, roundtrip;
    int64_t civil_seconds;
    unsigned weekday;
    if (hpclang_datetime_read(&civil) != 0 || civil.year < 2000 || civil.year > 2099 ||
        hpclang_datetime_to_seconds(&civil, &civil_seconds) != 0 ||
        hpclang_datetime_from_seconds(civil_seconds, &roundtrip) != 0 ||
        memcmp(&civil, &roundtrip, sizeof(civil)) != 0 ||
        hpclang_datetime_weekday(&civil, &weekday) != 0 || weekday > 6)
        return 0x74;

    /* Exercise both directions, then restore civil time including elapsed
     * whole seconds. No alarm or periodic-tick configuration may change. */
    static const uintptr_t rtc_settings[] = {0x07b00040, 0x07b00044, 0x07b00050,
                                             0x07b00054, 0x07b00058, 0x07b0005c,
                                             0x07b00060, 0x07b00064, 0x07b00068};
    uint8_t settings[sizeof(rtc_settings) / sizeof(rtc_settings[0])];
    for (unsigned i = 0; i < sizeof(settings); ++i)
        settings[i] = *(volatile uint8_t *)rtc_settings[i];
    struct hpclang_datetime original, changed;
    int64_t original_seconds;
    uint64_t mono_before, mono_after;
    if (hpclang_datetime_read(&original) != 0 ||
        hpclang_datetime_to_seconds(&original, &original_seconds) != 0 ||
        hpclang_time_read_us(&mono_before) != 0)
        return 0x7a01;
    struct hpclang_timer deadline = {0};
    if (hpclang_timer_start(&deadline, 20000, 0) != 0)
        return 0x7a02;
    uint32_t adjust_failure = 0;
    static const struct hpclang_datetime dates[] = {{2098, 12, 31, 23, 59, 58},
                                                    {2001, 1, 1, 0, 0, 1}};
    for (unsigned i = 0; i < 2; ++i) {
        if (hpclang_datetime_set(&dates[i]) != 0 || hpclang_datetime_read(&changed) != 0 ||
            changed.year != dates[i].year || changed.month != dates[i].month ||
            changed.day != dates[i].day) {
            adjust_failure = 0x7a10 + i;
            break;
        }
        if (hpclang_time_read_us(&mono_after) != 0 || mono_after < mono_before ||
            mono_after - mono_before > 200000) {
            adjust_failure = 0x7a20 + i;
            break;
        }
    }
    if (hpclang_time_read_us(&mono_after) != 0)
        adjust_failure = 0x7a30;
    else
        original_seconds += (int64_t)((mono_after - mono_before) / 1000000);
    if (hpclang_datetime_from_seconds(original_seconds, &original) != 0 ||
        hpclang_datetime_set(&original) != 0)
        return 0x7a31;
    if (adjust_failure)
        return adjust_failure;
    hpclang_delay_us(20000);
    if (hpclang_timer_poll(&deadline, &expirations) != 0 || expirations != 1)
        return 0x7a32;
    for (unsigned i = 0; i < sizeof(settings); ++i)
        if (settings[i] != *(volatile uint8_t *)rtc_settings[i])
            return 0x7a33;

    int sound_result = sound_test();
    return sound_result ? 0x7600u + (uint32_t)sound_result : 0;
}
