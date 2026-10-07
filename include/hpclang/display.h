#ifndef HPCLANG_DISPLAY_H
#define HPCLANG_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Pixel target with an implementation-private representation.
     *
     * Applications never receive framebuffer addresses, packing, or stride from
     * this type. The physical display surface is borrowed from the runtime.
     */
    struct hpclang_surface;

    /* Borrow Stock's display or the active session's back page. */
    struct hpclang_surface *hpclang_display_surface(void);

    /*
     * Acquire a temporary 2/4/16-level mode and two white DMA pages.
     * No framebuffer memory is reserved at startup. During a session,
     * display_surface() borrows the back page. Reacquire it after present;
     * never destroy a borrowed surface.
     *
     * Return 0 or -1/errno: EINVAL, EBUSY, ENOMEM, ENOTSUP, ENOSYS,
     * or a mapping/device error. Failure preserves Stock LCD state.
     * Calls are single-threaded and must not run from interrupt handlers.
     */
    int hpclang_display_begin(uint32_t levels);

    /*
     * Publish the back page and exchange pages at an observed frame boundary.
     * timeout_us must be 1..1000000. Cache publication is automatic.
     * LCD DMA stops/restarts to discard prefetched data; this does not promise
     * flicker-free physical scanout. Finish drawing before calling.
     *
     * Failure before switching leaves both pages in place. The new back page
     * contains the previous front image. Requires begin, otherwise EPERM.
     */
    int hpclang_display_present(uint32_t timeout_us);

    /*
     * Wait for the next frame-counter rollover without changing the LCD.
     * Same timeout range; ENODEV if disabled, ETIMEDOUT on expiry,
     * EIO for an unavailable or stalled clock.
     */
    int hpclang_display_wait_frame(uint32_t timeout_us);

    /*
     * Restore original LCD configuration before freeing pages. Idempotent.
     * Also runs after main/destructors and before _exit halts.
     * Borrowed session surfaces become invalid. Arbitrary CPU faults and
     * power loss are outside this cleanup contract.
     */
    void hpclang_display_end(void);

    /*
     * Set a back-page status segment (0..5); present publishes the change.
     * Printed icons vary by model. Classic HPG order is remote, left shift,
     * right shift, alpha, battery, wait. This changes neither calculator flags
     * nor the saved Stock framebuffer. EINVAL for a bad segment, EPERM if
     * no display session is active, ENOTSUP for an incompatible scan layout.
     */
    int hpclang_display_indicator_set(unsigned segment, uint8_t intensity);

    /*
     * Allocate an owned surface initialized to white. levels must be 2, 4, or 16;
     * those correspond to 1, 2, or 4 implementation-private bits per pixel.
     * Return NULL for unsupported levels, zero geometry, or allocation failure.
     */
    struct hpclang_surface *hpclang_surface_create(uint32_t width, uint32_t height,
                                                   uint32_t levels);

    /* Convenience constructor for a two-level owned surface. */
    struct hpclang_surface *hpclang_surface_create_mono(uint32_t width, uint32_t height);

    /* Release a surface returned by an hpclang owned-surface constructor. */
    void hpclang_surface_destroy(struct hpclang_surface *surface);

    uint32_t hpclang_surface_width(const struct hpclang_surface *surface);
    uint32_t hpclang_surface_height(const struct hpclang_surface *surface);
    uint32_t hpclang_surface_levels(const struct hpclang_surface *surface);

    /*
     * Drawing coordinates use a top-left origin. Operations clip to the surface.
     *
     * intensity uses 0 for white and 255 for black. Each surface quantizes that
     * range to its advertised level count. Stock's borrowed display has two
     * levels; an
     * acquired display and owned surfaces may have 2, 4, or 16.
     */
    void hpclang_surface_clear(struct hpclang_surface *surface, uint8_t intensity);

    /*
     * Read one pixel as an hpclang intensity. Out-of-bounds coordinates read as
     * white (0). Returned values are expanded back to the 0..255 intensity
     * range: 2-level surfaces return 0/255, 4-level surfaces 0/85/170/255, and
     * 16-level surfaces multiples of 17.
     */
    uint8_t hpclang_surface_get_pixel(const struct hpclang_surface *surface, int32_t x, int32_t y);

    void hpclang_surface_set_pixel(struct hpclang_surface *surface, int32_t x, int32_t y,
                                   uint8_t intensity);

    void hpclang_surface_draw_line(struct hpclang_surface *surface, int32_t x0, int32_t y0,
                                   int32_t x1, int32_t y1, uint8_t intensity);

    /*
     * Draw an outline rectangle. width/height are extents, not inclusive end
     * coordinates. Like the other primitives, the outline is geometrically
     * clipped rather than replaced by an outline of the clipped bounding box.
     */
    void hpclang_surface_draw_rect(struct hpclang_surface *surface, int32_t x, int32_t y,
                                   int32_t width, int32_t height, uint8_t intensity);

    /*
     * Circles use an integer center/radius. A negative radius is a no-op.
     * Rasterization is clipped before iterating visible rows/columns, so very
     * large off-screen circles do not cause work proportional to their radius.
     */
    void hpclang_surface_draw_circle(struct hpclang_surface *surface, int32_t center_x,
                                     int32_t center_y, int32_t radius, uint8_t intensity);

    void hpclang_surface_fill_circle(struct hpclang_surface *surface, int32_t center_x,
                                     int32_t center_y, int32_t radius, uint8_t intensity);

    struct hpclang_point
    {
        int32_t x;
        int32_t y;
    };

    /*
     * Polygon vertices are connected in order and the last vertex is connected
     * back to the first. Fill uses the even-odd rule. A NULL/empty polygon is a
     * no-op; one- and two-point fills degrade to the corresponding outline.
     */
    void hpclang_surface_draw_polygon(struct hpclang_surface *surface,
                                      const struct hpclang_point *points, uint32_t point_count,
                                      uint8_t intensity);

    void hpclang_surface_fill_polygon(struct hpclang_surface *surface,
                                      const struct hpclang_point *points, uint32_t point_count,
                                      uint8_t intensity);

    /*
     * Copy a rectangular source region to a destination surface. Source and
     * destination coordinates are clipped together so their pixel correspondence
     * is preserved. Overlap on the same surface is supported.
     */
    void hpclang_surface_blit(struct hpclang_surface *destination, int32_t destination_x,
                              int32_t destination_y, const struct hpclang_surface *source,
                              int32_t source_x, int32_t source_y, int32_t width, int32_t height);

    void hpclang_surface_fill_rect(struct hpclang_surface *surface, int32_t x, int32_t y,
                                   int32_t width, int32_t height, uint8_t intensity);

    /* Read Stock's contrast setting (0..15) without changing it. Returns 0,
     * or -1 with EINVAL (NULL), ENOSYS (unknown model), EIO (invalid result).
     * The setting is an ordinal level, not a measured LCD luminance. */
    int hpclang_display_contrast_read(unsigned *level);

#ifdef __cplusplus
}
#endif

#endif
