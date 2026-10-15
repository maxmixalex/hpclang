#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <errno.h>
#include <string.h>
#include "object-snapshot.h"

volatile uint32_t hpclang_e2e_result;

/* Minimal CRC-valid library 123, title X, no executable tables. The private
 * reader only constructs an owned test input; storage uses the public API. */
static const char encoding[] = "04b2062000108510b7000000000000000000000d5c2";
static int read_nibble(void *context, uint32_t at, uint8_t *out)
{
    (void)context;
    if (at >= sizeof encoding - 1)
        return 0;
    *out = encoding[at] <= '9' ? encoding[at] - '0' : encoding[at] - 'a' + 10;
    return 1;
}

static int equal(const struct hpclang_object *a, const struct hpclang_object *b)
{
    return a && b && a->kind == HPCLANG_OBJECT_LIBRARY && a->nibble_count == b->nibble_count &&
           !memcmp(a->data, b->data, (a->nibble_count + 1) / 2);
}

static void hold(void)
{
    for (volatile unsigned n = 12000000; n; --n)
        __asm__ volatile("nop");
}

int main(void)
{
    struct hpclang_rpl_reader reader = {read_nibble, NULL};
    struct hpclang_object *source = NULL, *copy = NULL, *witness = NULL;
    struct hpclang_object_store *home = NULL;
    unsigned failure = 1, phase = 0;
    if (hpclang_object_snapshot_copy(&reader, 0, sizeof encoding - 1, &source))
        goto done;
    for (uint32_t port = 0; port < 2; ++port) {
        failure = 1 + 16 * port;
        struct hpclang_port_info info = {.size = sizeof info, .version = HPCLANG_PORT_INFO_VERSION};
        if (hpclang_port_query(port, &info))
            goto done;
        if (port && info.medium == HPCLANG_PORT_ABSENT)
            continue;
        if (!(info.available & HPCLANG_PORT_CREATE_LIBRARY))
            goto done;
        failure = 2 + 16 * port;
        copy = hpclang_port_library_get(port, 123);
        if (!port)
            phase = copy != NULL;
        if (!phase) {
            if (copy || errno != ENOENT || hpclang_port_library_create(port, source))
                goto done;
            copy = hpclang_port_library_get(port, 123);
        } else {
            failure = 3 + 16 * port;
            if (!home)
                home = hpclang_object_store_home();
            const uint8_t *key = (const uint8_t *)(port ? "HPCWIT1" : "HPCWIT");
            if (!home || hpclang_object_store_get(home, key, port ? 7 : 6, &witness) ||
                !equal(witness, source))
                goto done;
        }
        failure = 4 + 16 * port;
        if (!equal(copy, source))
            goto done;
        failure = 5 + 16 * port;
        if (hpclang_port_library_create(port, source) != -1 || errno != EEXIST)
            goto done;
        hpclang_object_release(copy);
        copy = NULL;
        hpclang_object_release(witness);
        witness = NULL;
    }
    hpclang_e2e_result = 0x4f53d000u | phase;
    hold();
    failure = 0;
done:
    hpclang_object_release(source);
    hpclang_object_release(copy);
    hpclang_object_release(witness);
    hpclang_object_store_close(home);
    hpclang_e2e_result = failure ? 0x46410000u | failure : 0x50415353;
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hold();
    return 0;
}
