#include <stddef.h>
#include <stdint.h>

/* Freestanding witness support; all overlapping copies stay in owned RAM. */
void *memmove(void *destination, const void *source, size_t count)
{
    volatile uint8_t *out = destination;
    const volatile uint8_t *in = source;
    if ((uintptr_t)out < (uintptr_t)in) {
        for (size_t i = 0; i < count; ++i)
            out[i] = in[i];
    } else {
        while (count) {
            --count;
            out[count] = in[count];
        }
    }
    return destination;
}
