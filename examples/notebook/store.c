#include "store.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <errno.h>
#include <string.h>

static const uint8_t name[] = "HPCNOTE";
static const char signature[] = "hpclang-notebook/1\n";

/* The persisted record is a single printable UTF-8 line, matching the editor.
 * Validate before exposing native bytes to the view or replacing a record. */
static int valid_line(const char *bytes, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        uint32_t scalar = (uint8_t)bytes[offset++];
        unsigned count = 0;
        uint32_t minimum = 0;
        if (scalar >= 0xc2 && scalar <= 0xdf) {
            scalar &= 31;
            count = 1;
            minimum = 0x80;
        } else if (scalar >= 0xe0 && scalar <= 0xef) {
            scalar &= 15;
            count = 2;
            minimum = 0x800;
        } else if (scalar >= 0xf0 && scalar <= 0xf4) {
            scalar &= 7;
            count = 3;
            minimum = 0x10000;
        } else if (scalar >= 0x80) {
            return 0;
        }
        if (count > length - offset)
            return 0;
        while (count--) {
            uint8_t next = (uint8_t)bytes[offset++];
            if ((next & 0xc0) != 0x80)
                return 0;
            scalar = (scalar << 6) | (next & 63);
        }
        if (scalar < minimum || scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f) ||
            (scalar >= 0xd800 && scalar <= 0xdfff) || scalar > 0x10ffff)
            return 0;
    }
    return 1;
}

struct destination
{
    struct hpclang_object_store *home;
    int aplet;
    int port;
    uint32_t available;
};

static int open_destination(struct destination *destination, int write, int port)
{
    *destination = (struct destination){.port = port};
    if (port >= 0) {
        struct hpclang_port_info info = {.size = sizeof(info),
                                         .version = HPCLANG_PORT_INFO_VERSION};
        if (hpclang_port_query((unsigned)port, &info))
            return -1;
        if (!(info.available & HPCLANG_PORT_READ_BACKUP)) {
            errno = ENOSYS;
            return -1;
        }
        destination->available = info.available;
        return 0;
    }
    if (hpclang_device_capability(HPCLANG_CAPABILITY_APLET_DATA) == HPCLANG_CAPABILITY_SUPPORTED) {
        destination->aplet = 1;
        return 0;
    }
    enum hpclang_capability capability =
        write ? HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION : HPCLANG_CAPABILITY_RPL_OBJECT_STORE;
    if (hpclang_device_capability(capability) != HPCLANG_CAPABILITY_SUPPORTED) {
        errno = ENOSYS;
        return -1;
    }
    destination->home = hpclang_object_store_home();
    return destination->home ? 0 : -1;
}

static int read_note(const struct destination *destination, char *text)
{
    struct hpclang_object *object = NULL;
    int result;
    if (destination->port >= 0) {
        object = hpclang_port_backup_get((unsigned)destination->port, name, sizeof(name) - 1);
        result = object ? 0 : -1;
    } else
        result = destination->aplet
                     ? hpclang_aplet_data_get(&object)
                     : hpclang_object_store_get(destination->home, name, sizeof(name) - 1, &object);
    if (result)
        return -1;
    size_t length = 0;
    if (destination->aplet && hpclang_object_kind(object) == HPCLANG_OBJECT_LIST) {
        result = hpclang_object_list_size(object, &length);
        if (!result) {
            errno = length == 0 ? ENOENT : EILSEQ;
            result = -1;
        }
    } else if (hpclang_object_kind(object) != HPCLANG_OBJECT_STRING) {
        errno = EILSEQ;
        result = -1;
    } else {
        char bytes[sizeof(signature) - 1 + NOTEBOOK_CAPACITY];
        result = hpclang_object_string_size(object, &length);
        if (!result && (length < sizeof(signature) - 1 || length >= sizeof(bytes))) {
            errno = EILSEQ;
            result = -1;
        }
        if (!result)
            result = hpclang_object_string_copy(object, (uint8_t *)bytes, sizeof(bytes));
        if (!result &&
            (memcmp(bytes, signature, sizeof(signature) - 1) ||
             !valid_line(bytes + sizeof(signature) - 1, length - (sizeof(signature) - 1)))) {
            errno = EILSEQ;
            result = -1;
        }
        if (!result && text) {
            size_t payload = length - (sizeof(signature) - 1);
            memcpy(text, bytes + sizeof(signature) - 1, payload);
            text[payload] = 0;
        }
    }
    int error = errno;
    hpclang_object_release(object);
    errno = error;
    return result;
}

static int load_note(int port, char text[NOTEBOOK_CAPACITY])
{
    if (!text) {
        errno = EINVAL;
        return -1;
    }
    struct destination destination;
    if (open_destination(&destination, 0, port))
        return -1;
    int result = read_note(&destination, text);
    int error = errno;
    if (destination.home)
        hpclang_object_store_close(destination.home);
    errno = error;
    return result;
}

static int save_note(int port, const char *text)
{
    if (!text) {
        errno = EINVAL;
        return -1;
    }
    size_t length = 0;
    while (length < NOTEBOOK_CAPACITY && text[length])
        ++length;
    if (length == NOTEBOOK_CAPACITY) {
        errno = E2BIG;
        return -1;
    }
    if (!valid_line(text, length)) {
        errno = EILSEQ;
        return -1;
    }
    struct destination destination;
    if (open_destination(&destination, 1, port))
        return -1;
    int result = read_note(&destination, NULL);
    int exists = result == 0;
    struct hpclang_object *object = NULL;
    if (exists || errno == ENOENT) {
        uint32_t operation = exists ? HPCLANG_PORT_REPLACE_BACKUP : HPCLANG_PORT_CREATE_BACKUP;
        if (port >= 0 && !(destination.available & operation)) {
            errno = ENOSYS;
            return -1;
        }
        char bytes[sizeof(signature) - 1 + NOTEBOOK_CAPACITY];
        memcpy(bytes, signature, sizeof(signature) - 1);
        memcpy(bytes + sizeof(signature) - 1, text, length);
        result = hpclang_object_create_string((const uint8_t *)bytes,
                                              sizeof(signature) - 1 + length, &object);
        if (!result && port >= 0)
            result =
                exists ? hpclang_port_backup_replace((unsigned)port, name, sizeof(name) - 1, object)
                       : hpclang_port_backup_create((unsigned)port, name, sizeof(name) - 1, object);
        else if (!result)
            result =
                destination.aplet
                    ? hpclang_aplet_data_set(object)
                    : hpclang_object_store_put(destination.home, name, sizeof(name) - 1, object,
                                               exists ? HPCLANG_OBJECT_STORE_REPLACE_ONLY
                                                      : HPCLANG_OBJECT_STORE_CREATE_ONLY);
    }
    int error = errno;
    hpclang_object_release(object);
    if (destination.home)
        hpclang_object_store_close(destination.home);
    errno = error;
    return result;
}

int notebook_load(char text[NOTEBOOK_CAPACITY])
{
    return load_note(-1, text);
}

int notebook_save(const char *text)
{
    return save_note(-1, text);
}

int notebook_backup_load(unsigned port, char text[NOTEBOOK_CAPACITY])
{
    if (port > 2) {
        errno = EINVAL;
        return -1;
    }
    return load_note((int)port, text);
}

int notebook_backup_save(unsigned port, const char *text)
{
    if (port > 2) {
        errno = EINVAL;
        return -1;
    }
    return save_note((int)port, text);
}
