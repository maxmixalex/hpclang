#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/port.h>
#include <hpclang/target.h>

uint32_t hpclang_e2e_port_lifecycle(uint32_t *phase)
{
    static const uint8_t name[] = "PORTKEEP";
    static const uint8_t text[] = "Survives native return and relaunch";
    struct hpclang_object *previous = hpclang_port_backup_get(0, name, sizeof(name) - 1);
    struct hpclang_object *next = NULL;
    int result;
    if (!previous) {
        if (errno != ENOENT || hpclang_object_create_integer_i64(321, &next))
            return 0xb0;
        *phase = 0;
        result = hpclang_port_backup_create(0, name, sizeof(name) - 1, next);
    } else if (hpclang_object_kind(previous) == HPCLANG_OBJECT_STRING) {
        uint8_t bytes[sizeof(text)] = {0};
        size_t length;
        int valid = hpclang_object_string_size(previous, &length) == 0 &&
                    length == sizeof(text) - 1 &&
                    hpclang_object_string_copy(previous, bytes, sizeof(bytes)) == 0 &&
                    !memcmp(bytes, text, length);
        hpclang_object_release(previous);
        if (!valid)
            return 0xb1;
        *phase = 2;
        result = hpclang_port_backup_remove(0, name, sizeof(name) - 1);
    } else {
        int64_t integer;
        int valid = hpclang_object_integer_i64(previous, &integer) == 0 && integer == 321;
        hpclang_object_release(previous);
        if (!valid || hpclang_object_create_string(text, sizeof(text) - 1, &next))
            return 0xb2;
        *phase = 1;
        result = hpclang_port_backup_replace(0, name, sizeof(name) - 1, next);
    }
    hpclang_object_release(next);
    return result ? 0xb3 : 0;
}

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
    if (!correct)
        return 0xd3;
    struct hpclang_object *created = NULL;
    if (hpclang_object_create_integer_i64(987, &created))
        return 0xd5;
    int written = hpclang_port_backup_create(0, (const uint8_t *)"PORTNEW", 7, created);
    hpclang_object_release(created);
    if (written)
        return 0xd6;
    value = hpclang_port_backup_get(0, (const uint8_t *)"PORTNEW", 7);
    if (!value)
        return 0xd7;
    correct = hpclang_object_integer_i64(value, &integer) == 0 && integer == 987;
    int duplicate = hpclang_port_backup_create(0, (const uint8_t *)"PORTNEW", 7, value);
    int rejected = duplicate == -1 && errno == EEXIST;
    hpclang_object_release(value);
    if (!correct || !rejected)
        return 0xd8;
    static const uint8_t text[] = "Port backup grows before shrinking";
    if (hpclang_object_create_string(text, sizeof(text) - 1, &created))
        return 0xd9;
    written = hpclang_port_backup_replace(0, (const uint8_t *)"PORTNEW", 7, created);
    hpclang_object_release(created);
    if (written)
        return 0xda;
    value = hpclang_port_backup_get(0, (const uint8_t *)"PORTNEW", 7);
    uint8_t copied[sizeof(text)] = {0};
    size_t length = 0;
    correct = value && hpclang_object_string_size(value, &length) == 0 &&
              hpclang_object_string_copy(value, copied, sizeof(copied)) == 0 &&
              length == sizeof(text) - 1 && !memcmp(text, copied, length);
    hpclang_object_release(value);
    if (!correct)
        return 0xdb;
    if (hpclang_object_create_integer_i64(654, &created))
        return 0xdc;
    written = hpclang_port_backup_replace(0, (const uint8_t *)"PORTNEW", 7, created);
    hpclang_object_release(created);
    if (written)
        return 0xdd;
    value = hpclang_port_backup_get(0, (const uint8_t *)"PORTNEW", 7);
    correct = value && hpclang_object_integer_i64(value, &integer) == 0 && integer == 654;
    hpclang_object_release(value);
    if (!correct || hpclang_port_backup_remove(0, (const uint8_t *)"PORTNEW", 7))
        return 0xde;
    value = hpclang_port_backup_get(0, (const uint8_t *)"PORTNEW", 7);
    correct = !value && errno == ENOENT;
    hpclang_object_release(value);
    return correct ? 0 : 0xdf;
}
