#include "store.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <errno.h>
#include <string.h>

static const uint8_t name[] = "HPCNOTE";
static const char signature[] = "hpclang-notebook/1\n";

struct destination
{
    struct hpclang_object_store *home;
    int aplet;
};

static int open_destination(struct destination *destination, int write)
{
    *destination = (struct destination){0};
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
    int result = destination->aplet
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
            (memcmp(bytes, signature, sizeof(signature) - 1) || memchr(bytes, 0, length))) {
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

int notebook_load(char text[NOTEBOOK_CAPACITY])
{
    if (!text) {
        errno = EINVAL;
        return -1;
    }
    struct destination destination;
    if (open_destination(&destination, 0))
        return -1;
    int result = read_note(&destination, text);
    int error = errno;
    if (destination.home)
        hpclang_object_store_close(destination.home);
    errno = error;
    return result;
}

int notebook_save(const char *text)
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
    struct destination destination;
    if (open_destination(&destination, 1))
        return -1;
    int result = read_note(&destination, NULL);
    int exists = result == 0;
    struct hpclang_object *object = NULL;
    if (exists || errno == ENOENT) {
        char bytes[sizeof(signature) - 1 + NOTEBOOK_CAPACITY];
        memcpy(bytes, signature, sizeof(signature) - 1);
        memcpy(bytes + sizeof(signature) - 1, text, length);
        result = hpclang_object_create_string((const uint8_t *)bytes,
                                              sizeof(signature) - 1 + length, &object);
        if (!result)
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
