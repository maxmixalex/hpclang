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
