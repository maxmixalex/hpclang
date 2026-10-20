#ifndef HPCLANG_HPGCC_LINE_H
#define HPCLANG_HPGCC_LINE_H
#include <stddef.h>
/* Byte-oriented historical editor boundary. read follows hpclang_stdio_result;
 * echo/erase/full return 0 or -1/errno and must not reenter this editor. full is
 * optional; read/echo/erase are required. No allocation or hardware ownership. */
struct hpclang_hpgcc_line_io
{
    int (*read)(void *context);
    int (*echo)(void *context, unsigned char byte);
    int (*erase)(void *context);
    int (*full)(void *context);
    void *context;
};
/* Capacity includes NUL. Starts empty, accepts CR/LF, echoes LF, ignores other
 * C0 bytes except BS. Full input is rejected until edited/accepted. Returns
 * buffer on acceptance or nonempty EOF, NULL on empty EOF/error/cancellation.
 * Partial text remains terminated on failure; invalid arguments change nothing.
 * Success/normal EOF preserve entry errno; missing failure errno becomes EIO. */
char *hpclang_hpgcc_read_line(char *buffer, size_t capacity,
                              const struct hpclang_hpgcc_line_io *io);
#endif
