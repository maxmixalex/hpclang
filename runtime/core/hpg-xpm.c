#include "hpg-internal.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct entry
{
    const char *key;
    unsigned char color;
};

static int space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static int number(const char **cursor, unsigned *value)
{
    const char *p = *cursor;
    while (space(*p))
        ++p;
    unsigned n = 0;
    if (*p < '0' || *p > '9')
        return 0;
    do {
        unsigned digit = (unsigned)(*p++ - '0');
        if (n > ((unsigned)INT_MAX - digit) / 10)
            return 0;
        n = n * 10 + digit;
    } while (*p >= '0' && *p <= '9');
    if (*p && !space(*p))
        return 0;
    *cursor = p;
    *value = n;
    return n != 0;
}
static int hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static unsigned char color(const char *p)
{
    unsigned char result = 0;
    unsigned best = 0, priority = 0;
    while (*p) {
        while (space(*p))
            ++p;
        const char *start = p;
        while (*p && !space(*p))
            ++p;
        size_t length = (size_t)(p - start);
        if (!length)
            break;
        if (length == 1 && (*start == 'c' || *start == 'g')) {
            priority = *start == 'g' ? 2 : 1;
            continue;
        }
        if (length == 2 && start[0] == 'g' && start[1] == '4') {
            priority = 3;
            continue;
        }
        if (priority > best && length == 7 && start[0] == '#') {
            unsigned rgb[3] = {0};
            int valid = 1;
            for (unsigned i = 0; i < 6; ++i) {
                int digit = hex(start[i + 1]);
                if (digit < 0) {
                    valid = 0;
                    break;
                }
                rgb[i / 2] = rgb[i / 2] * 16 + (unsigned)digit;
            }
            if (valid) {
                /* Historical HPG luminance, inverted to calculator ink. */
                result = (unsigned char)(255 - ((227 * rgb[0] + 724 * rgb[1] + 73 * rgb[2]) >> 10));
                best = priority;
            }
        }
        priority = 0;
    }
    return result;
}

static hpg_t *load(char *xpm[], unsigned levels)
{
    unsigned width, height, count, cpp;
    if (!xpm || !xpm[0]) {
        errno = EINVAL;
        return NULL;
    }
    const char *header = xpm[0];
    if (!number(&header, &width) || !number(&header, &height) || !number(&header, &count) ||
        !number(&header, &cpp) || width > SIZE_MAX / cpp || (size_t)count > SIZE_MAX - height - 1) {
        errno = EINVAL;
        return NULL;
    }
    size_t palette_bytes = (size_t)count * sizeof(struct entry);
    if (palette_bytes / count != sizeof(struct entry)) {
        errno = EINVAL;
        return NULL;
    }
    struct entry *palette = malloc(palette_bytes);
    if (!palette) {
        errno = ENOMEM;
        return NULL;
    }
    hpg_t *image = NULL;
    for (unsigned i = 0; i < count; ++i) {
        const char *line = xpm[(size_t)i + 1];
        if (!line || strlen(line) < cpp)
            goto invalid;
        for (unsigned j = 0; j < i; ++j)
            if (!memcmp(line, palette[j].key, cpp))
                goto invalid;
        palette[i] = (struct entry){line, color(line + cpp)};
    }
    image = levels == 2   ? hpg_alloc_mono_image((int)width, (int)height)
            : levels == 4 ? hpg_alloc_gray4_image((int)width, (int)height)
                          : hpg_alloc_gray16_image((int)width, (int)height);
    if (!image) {
        free(palette);
        return NULL;
    }
    for (unsigned y = 0; y < height; ++y) {
        const char *line = xpm[(size_t)count + 1 + y];
        if (!line || strlen(line) < (size_t)width * cpp)
            goto invalid;
        for (unsigned x = 0; x < width; ++x)
            for (unsigned i = 0; i < count; ++i)
                if (!memcmp(line + (size_t)x * cpp, palette[i].key, cpp)) {
                    hpg_set_color(image, palette[i].color);
                    hpg_draw_pixel_on(image, (int)x, (int)y);
                    break;
                }
    }
    free(palette);
    return image;
invalid:
    hpg_free_image(image);
    free(palette);
    errno = EINVAL;
    return NULL;
}
hpg_t *hpg_load_xpm_mono(char *xpm[])
{
    return load(xpm, 2);
}
hpg_t *hpg_load_xpm_gray4(char *xpm[])
{
    return load(xpm, 4);
}
hpg_t *hpg_load_xpm_gray16(char *xpm[])
{
    return load(xpm, 16);
}
