#include <assert.h>
#include "key-sample.h"

int main(void)
{
    /* Matrix: column 2 driven low, row 3 sampled as input. */
    uint32_t config = 1u << 20;
    uint32_t value = 0xffffu & ~(1u << 10) & ~(1u << 3);
    assert(hpclang_key_sampled(1, config, value, 2, 3, -1, 1));
    assert(!hpclang_key_sampled(1, config, value, 2, 3, -1, 0));
    assert(!hpclang_key_sampled(1, config, value, 1, 3, -1, 1));
    assert(!hpclang_key_sampled(1, config, value | (1u << 10), 2, 3, -1, 1));
    assert(!hpclang_key_sampled(1, config | (1u << 6), value, 2, 3, -1, 1));
    assert(hpclang_key_sampled(1, config, value | (1u << 3), 2, 3, -1, 0));
    /* Opposite scan direction, as used by the HP39 keyboard. */
    config = 1u << 6;
    assert(hpclang_key_sampled(1, config, value, 2, 3, -1, 1));
    assert(hpclang_key_sampled(1, config, value | (1u << 10), 2, 3, -1, 0));
    assert(!hpclang_key_sampled(1, config, value | (1u << 3), 2, 3, -1, 1));
    /* ON/modifiers: dedicated active-high interrupt inputs. */
    assert(hpclang_key_sampled(0, 2u << 10, 1u << 5, 0, 0, 5, 1));
    assert(hpclang_key_sampled(0, 0, 0, 0, 0, 5, 0));
    assert(!hpclang_key_sampled(0, 1u << 10, 1u << 5, 0, 0, 5, 1));
    assert(!hpclang_key_sampled(1, 0, 1u << 5, 0, 0, 5, 1));
    assert(!hpclang_key_sampled(0, 0, value, 2, 3, -1, 1));
    /* A cold boot with all outputs must not consume a pending key-down. */
    assert(!hpclang_key_ready(1, 0x55555555u, 0, 5, 6, -1));
    assert(!hpclang_key_ready(1, 0, 0, 5, 6, -1));
    assert(hpclang_key_ready(1, 1u << 26, 0, 5, 6, -1));
    assert(hpclang_key_ready(1, 1u << 12, 0, 5, 6, -1));
    assert(!hpclang_key_ready(1, 1u << 26, 1u << 13, 5, 6, -1));
    assert(hpclang_key_ready(0, 2u << 10, 0, 0, 0, 5));
    assert(!hpclang_key_ready(0, 1u << 10, 0, 0, 0, 5));
    return 0;
}
