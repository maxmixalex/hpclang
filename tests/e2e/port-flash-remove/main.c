#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <errno.h>
#include <stdlib.h>

volatile uint32_t hpclang_e2e_result;
static const uint8_t name[] = "HPCFONE";

static void hold(void)
{
    for (volatile uint32_t n = 12000000u; n; --n)
        __asm__ volatile("nop");
}

static int read_integer(struct hpclang_object_store *home, const uint8_t *key, int64_t *value)
{
    struct hpclang_object *object = NULL;
    if (hpclang_object_store_get(home, key, 7, &object))
        return -1;
    int result = hpclang_object_integer_i64(object, value);
    hpclang_object_release(object);
    return result;
}

int main(void)
{
    unsigned failure = 1;
    int64_t phase = -1, number;
    struct hpclang_object_store *home = hpclang_object_store_home();
    struct hpclang_object *object = NULL, *snapshot = NULL;
    if (!home || read_integer(home, (const uint8_t *)"HPCSTEP", &phase) || phase < 0 || phase > 2)
        goto done;
    failure = 2;
    struct hpclang_port_info info = {.size = sizeof info, .version = HPCLANG_PORT_INFO_VERSION};
    if (hpclang_port_query(2, &info) || !(info.available & HPCLANG_PORT_REMOVE_BACKUP))
        goto done;
    if (phase) {
        /* Only native HPCA writes this witness: missing after deletion,
         * present after recreation. A stale witness cannot satisfy a launch. */
        failure = 3;
        if (read_integer(home, (const uint8_t *)"HPCFWIT", &number) || number != (phase == 1))
            goto done;
        if (hpclang_object_store_remove(home, (const uint8_t *)"HPCFWIT", 7))
            goto done;
    }
    failure = 4;
    if (phase < 2) {
        object = hpclang_port_backup_get(2, name, sizeof name - 1);
        if (object || errno != ENOENT)
            goto done;
        if (hpclang_object_create_integer_i64(phase ? 456 : 987, &object) ||
            hpclang_port_backup_create(2, name, sizeof name - 1, object))
            goto done;
        hpclang_object_release(object);
        object = NULL;
    }
    failure = 5;
    snapshot = hpclang_port_backup_get(2, name, sizeof name - 1);
    if (!snapshot || hpclang_object_integer_i64(snapshot, &number) || number != (phase ? 456 : 987))
        goto done;
    if (phase != 1) {
        failure = 6;
        if (hpclang_port_backup_remove(2, name, sizeof name - 1))
            goto done;
        object = hpclang_port_backup_get(2, name, sizeof name - 1);
        if (object || errno != ENOENT)
            goto done;
        if (hpclang_port_backup_remove(2, name, sizeof name - 1) != -1 || errno != ENOENT)
            goto done;
        if (hpclang_object_integer_i64(snapshot, &number) || number != (phase ? 456 : 987))
            goto done;
    }
    failure = 7;
    if (hpclang_object_create_integer_i64(phase + 1, &object) ||
        hpclang_object_store_put(home, (const uint8_t *)"HPCSTEP", 7, object,
                                 HPCLANG_OBJECT_STORE_REPLACE_ONLY))
        goto done;
    hpclang_e2e_result = UINT32_C(0x4f53c000) | (uint32_t)phase;
    hold();
    failure = 0;
done:
    hpclang_object_release(snapshot);
    hpclang_object_release(object);
    hpclang_object_store_close(home);
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hold();
    if (phase == 1)
        exit(0);
    if (phase == 2)
        _Exit(0);
    return 0;
}
