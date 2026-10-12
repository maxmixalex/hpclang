#include <stdint.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/port.h>
#include <hpclang/target.h>

uint32_t hpclang_e2e_port_reads(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    struct hpclang_port_iterator *iterator = hpclang_port_iterator_open(0);
    if (!iterator)
        return 0xd0;
    struct hpclang_port_entry entry = {.size = sizeof(entry),
                                       .version = HPCLANG_PORT_ENTRY_VERSION};
    uint32_t failure = 0;
    int result, found = 0, libraries = 0;
    while ((result = hpclang_port_iterator_read(iterator, &entry)) > 0) {
        if (entry.kind == HPCLANG_PORT_BACKUP && entry.name_length == 8 &&
            !memcmp(entry.name, "PORTZERO", 8))
            ++found;
        if (entry.kind == HPCLANG_PORT_LIBRARY) {
            struct hpclang_object *library = hpclang_port_library_get(0, entry.library_number);
            if (!library || hpclang_object_kind(library) != HPCLANG_OBJECT_LIBRARY)
                failure = 0xd4;
            hpclang_object_release(library);
            ++libraries;
        }
    }
    hpclang_port_iterator_close(iterator);
    if (failure)
        return failure;
    if (result || found != 1 || (boot->model == HPCLANG_MODEL_HP48GII && libraries == 0))
        return 0xd1;
    struct hpclang_object *value = hpclang_port_backup_get(0, (const uint8_t *)"PORTZERO", 8);
    if (!value)
        return 0xd2;
    int64_t integer;
    int correct = hpclang_object_integer_i64(value, &integer) == 0 && integer == 123;
    hpclang_object_release(value);
    return correct ? 0 : 0xd3;
}
