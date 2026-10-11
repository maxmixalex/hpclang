#ifndef HPCLANG_DEVICE_H
#define HPCLANG_DEVICE_H

#include <stdint.h>

#include <hpclang/target.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HPCLANG_DEVICE_INFO_VERSION 1u

    /*
     * Stable physical-device facts available to every hpclang application.
     *
     * size/version allow fields to be appended in later SDK revisions without
     * changing the meaning or offset of existing fields.
     *
     * display_width/display_height describe the visible LCD area. They do not
     * describe framebuffer stride, pixel packing, grayscale mode, or ownership.
     */
    struct hpclang_device_info
    {
        uint32_t size;
        uint32_t version;
        uint32_t model;
        uint32_t family;
        uint32_t display_width;
        uint32_t display_height;
    };

    enum hpclang_capability
    {
        HPCLANG_CAPABILITY_DISPLAY = 1,
        HPCLANG_CAPABILITY_KEYBOARD,
        HPCLANG_CAPABILITY_MONOTONIC_TIME,
        HPCLANG_CAPABILITY_CIVIL_TIME_READ,
        HPCLANG_CAPABILITY_POLL_TIMERS,
        HPCLANG_CAPABILITY_SOUND,
        HPCLANG_CAPABILITY_STREAM_STORAGE,
        HPCLANG_CAPABILITY_RPL_STACK,
        HPCLANG_CAPABILITY_RPL_OBJECT_STORE,
        HPCLANG_CAPABILITY_USB_POWER_DETECT,
        HPCLANG_CAPABILITY_CIVIL_TIME_SET,
        HPCLANG_CAPABILITY_CONTRAST_READ,
        HPCLANG_CAPABILITY_APLET_DATA,
        HPCLANG_CAPABILITY_APLET_VARIABLES,
        HPCLANG_CAPABILITY_TEXT_CATALOG,
        HPCLANG_CAPABILITY_APLET_CATALOG,
        HPCLANG_CAPABILITY_DISPLAY_MODES,
        HPCLANG_CAPABILITY_DISPLAY_INDICATORS,
        HPCLANG_CAPABILITY_RPL_STACK_MUTATION,
        HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION
    };

    enum hpclang_capability_state
    {
        HPCLANG_CAPABILITY_UNSUPPORTED = 0,
        HPCLANG_CAPABILITY_SUPPORTED = 1,
        HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE = 2
    };

    /* Discover implemented services for this model and launch context without
     * accessing hardware or acquiring resources. SUPPORTED does not promise
     * media presence, free memory, valid RTC contents or an idle sound timer.
     * Service calls still report their own runtime errors. Unknown capability
     * IDs return -1/EINVAL; otherwise return a hpclang_capability_state.
     * This describes services, not compatibility-header/function availability.
     * RPL_STACK and RPL_OBJECT_STORE describe read access. Their MUTATION
     * counterparts additionally require launcher-provided safe memory bounds.
     * HP39 services require a valid launcher portal. Queries inspect private
     * launcher RAM, never invoke that portal, and preserve errno on success.
     */
    int hpclang_device_capability(enum hpclang_capability capability);

    /*
     * Return an immutable process-lifetime descriptor for the active calculator.
     * Unknown model IDs return a valid descriptor with family/geometry set to 0.
     * No allocation or platform service call is performed.
     */
    const struct hpclang_device_info *hpclang_device_info(void);

#ifdef __cplusplus
}
#endif

#endif
