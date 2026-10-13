#ifndef HPCLANG_HPGRAPHICS_H
#define HPCLANG_HPGRAPHICS_H

/* Historical HPG spelling. Mutable drawing state belongs to this adapter;
 * framebuffer addresses and native packing are never exposed. Font/pattern
 * constructors borrow their byte buffers until the descriptor is freed. */
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct hpg_graphics hpg_t;
    typedef struct hpg_font hpg_font_t;
    typedef struct hpg_pattern hpg_pattern_t;

#define HPG_MODE_PAINT 0
#define HPG_MODE_XOR 1
#define HPG_COLOR_WHITE 0x00
#define HPG_COLOR_BLACK 0xff
#define HPG_COLOR_GRAY_1 0x11
#define HPG_COLOR_GRAY_2 0x22
#define HPG_COLOR_GRAY_3 0x33
#define HPG_COLOR_GRAY_4 0x44
#define HPG_COLOR_GRAY_5 0x55
#define HPG_COLOR_GRAY_6 0x66
#define HPG_COLOR_GRAY_7 0x77
#define HPG_COLOR_GRAY_8 0x88
#define HPG_COLOR_GRAY_9 0x99
#define HPG_COLOR_GRAY_10 0xaa
#define HPG_COLOR_GRAY_11 0xbb
#define HPG_COLOR_GRAY_12 0xcc
#define HPG_COLOR_GRAY_13 0xdd
#define HPG_COLOR_GRAY_14 0xee

    extern hpg_t *hpg_stdscreen;
#define HPG_STDSCREEN hpg_stdscreen
    void hpg_init(void);
    void hpg_cleanup(void);
    int hpg_get_width(hpg_t *g);
    int hpg_get_height(hpg_t *g);
    hpg_t *hpg_alloc_mono_image(int width, int height);
    hpg_t *hpg_alloc_gray4_image(int width, int height);
    hpg_t *hpg_alloc_gray16_image(int width, int height);
    void hpg_free_image(hpg_t *image);
    void hpg_clear_on(hpg_t *g);
    void hpg_clear(void);
    void hpg_draw_pixel_on(hpg_t *g, int x, int y);
    void hpg_draw_pixel(int x, int y);
    void hpg_draw_line_on(hpg_t *g, int x1, int y1, int x2, int y2);
    void hpg_draw_line(int x1, int y1, int x2, int y2);
    void hpg_draw_rect_on(hpg_t *g, int x1, int y1, int x2, int y2);
    void hpg_draw_rect(int x1, int y1, int x2, int y2);
    void hpg_draw_circle_on(hpg_t *g, int cx, int cy, int radius);
    void hpg_draw_circle(int cx, int cy, int radius);
    void hpg_fill_circle_on(hpg_t *g, int cx, int cy, int radius);
    void hpg_fill_circle(int cx, int cy, int radius);
    void hpg_draw_polygon_on(hpg_t *g, int vx[], int vy[], int len);
    void hpg_draw_polygon(int vx[], int vy[], int len);
    void hpg_fill_polygon_on(hpg_t *g, int vx[], int vy[], int len);
    void hpg_fill_polygon(int vx[], int vy[], int len);
    /* Copies source pixels without using its clip. Destination clip and PAINT/XOR
     * apply; color/font/pattern state is preserved. Overlap uses snapshot-equivalent
     * traversal, including XOR. Out-of-surface source/destination regions are clipped. */
    void hpg_blit(hpg_t *src, int sx, int sy, int width, int height, hpg_t *dst, int dx, int dy);
    void hpg_fill_rect_on(hpg_t *g, int x1, int y1, int x2, int y2);
    void hpg_fill_rect(int x1, int y1, int x2, int y2);
    unsigned char hpg_get_pixel(hpg_t *g, int x, int y);
    unsigned char hpg_get_color(hpg_t *g);
    void hpg_set_color(hpg_t *g, unsigned char color);
    unsigned char hpg_get_mode(hpg_t *g);
    void hpg_set_mode(hpg_t *g, unsigned char mode);
    void hpg_clip_reset(hpg_t *g);
    void hpg_clip_set(hpg_t *g, int x1, int y1, int x2, int y2);
    void hpg_clip(hpg_t *g, int x1, int y1, int x2, int y2);
    hpg_pattern_t *hpg_alloc_pattern(char *buffer, int height, int fixed);
    void hpg_free_pattern(hpg_pattern_t *pattern);
    hpg_pattern_t *hpg_get_pattern(hpg_t *g);
    void hpg_set_pattern(hpg_t *g, hpg_pattern_t *pattern);
    hpg_font_t *hpg_alloc_font(char *buffer, int count, int height, int advance);
    void hpg_free_font(hpg_font_t *font);
    hpg_font_t *hpg_get_font(hpg_t *g);
    void hpg_set_font(hpg_t *g, hpg_font_t *font);
    int hpg_font_get_height(hpg_font_t *font);
    int hpg_font_get_advance(hpg_font_t *font);

#ifdef __cplusplus
}
#endif
#endif
