#ifndef HPCLANG_STDIO_ROUTING_H
#define HPCLANG_STDIO_ROUTING_H
struct hpclang_stdio_callbacks;
/* Optional compatibility backend, installed once with process-lifetime context.
 * Explicit bindings take precedence as a whole (including missing callbacks).
 * Registration does not call the backend or acquire hardware. */
int hpclang_stdio_set_default(const struct hpclang_stdio_callbacks *callbacks);
int hpclang_stdio_route_write(unsigned stream, unsigned char byte);
int hpclang_stdio_route_read(void);
int hpclang_stdio_route_flush(unsigned stream);
/* Supplied by the libc bridge, or by a host test fixture. */
void hpclang_stdio_clear_streams(void);
#endif
