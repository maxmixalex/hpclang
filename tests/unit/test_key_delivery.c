#include <assert.h>
#include <string.h>
#include "key-delivery.h"

int main(void)
{
    struct hpclang_key_delivery s = {0};
    /* Host time advances, but the keyboard peripheral is still initializing.
     * An old sample must not cause this pending edge to be consumed. */
    assert(hpclang_key_delivery_step(&s, 0, 1, 0, 1, 0) == HPCLANG_KEY_WAIT);
    assert(hpclang_key_delivery_step(&s, 4000000, 1, 0, 1, 0) == HPCLANG_KEY_WAIT);
    assert(!s.delivered);
    assert(hpclang_key_delivery_step(&s, 5000000, 1, 1, 1, 0) == HPCLANG_KEY_EMIT);
    assert(hpclang_key_delivery_step(&s, 6000000, 1, 1, 0, 1) == HPCLANG_KEY_WAIT);
    assert(hpclang_key_delivery_step(&s, 7000000, 1, 1, 1, 0) == HPCLANG_KEY_DONE);
    /* Readiness and acknowledgement failures remain separate, bounded errors. */
    s = (struct hpclang_key_delivery){0};
    assert(hpclang_key_delivery_step(&s, 0, 1, 0, 0, 0) == HPCLANG_KEY_WAIT);
    assert(hpclang_key_delivery_step(&s, 15000000, 1, 0, 0, 0) < 0);
    assert(strcmp(s.failure, "input-not-ready") == 0);
    s = (struct hpclang_key_delivery){0};
    assert(hpclang_key_delivery_step(&s, 0, 1, 1, 0, 0) == HPCLANG_KEY_EMIT);
    assert(hpclang_key_delivery_step(&s, 15000000, 1, 1, 0, 0) < 0);
    assert(strcmp(s.failure, "input-not-sampled") == 0);
    /* Always deliver a release, even if the guest reconfigured the matrix. */
    s = (struct hpclang_key_delivery){0};
    assert(hpclang_key_delivery_step(&s, 0, 0, 0, 0, 0) == HPCLANG_KEY_EMIT);
    /* An idle ROM performs no post-release read; the open switch is enough. */
    assert(hpclang_key_delivery_step(&s, 100, 0, 0, 0, 1) == HPCLANG_KEY_DONE);
    /* A stale GPIO sample must not hide a switch that failed to release. */
    s = (struct hpclang_key_delivery){0};
    assert(hpclang_key_delivery_step(&s, 0, 0, 0, 1, 0) == HPCLANG_KEY_EMIT);
    assert(hpclang_key_delivery_step(&s, 100, 0, 0, 1, 0) == HPCLANG_KEY_WAIT);
    assert(hpclang_key_delivery_step(&s, 15000000, 0, 0, 1, 0) < 0);
    assert(strcmp(s.failure, "input-not-released") == 0);
    return 0;
}
