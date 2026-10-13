#include "../../examples/notebook/store.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct hpclang_object
{
    enum hpclang_object_kind kind;
    size_t length;
    uint8_t bytes[256];
};
struct hpclang_object_store
{
    int unused;
};
static struct hpclang_object saved;
static struct hpclang_object_store home;
static int aplet, supported = 1, present, fault, create_fault, write_fault, copy_fault;
static int writes, owners, handles;
static enum hpclang_object_store_put_mode last_mode;

int hpclang_device_capability(enum hpclang_capability capability)
{
    if (!supported)
        return HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE;
    return (capability == HPCLANG_CAPABILITY_APLET_DATA) == aplet ? HPCLANG_CAPABILITY_SUPPORTED
                                                                  : HPCLANG_CAPABILITY_UNSUPPORTED;
}
struct hpclang_object_store *hpclang_object_store_home(void)
{
    ++handles;
    return &home;
}
void hpclang_object_store_close(struct hpclang_object_store *store)
{
    assert(store == &home && handles == 1);
    --handles;
    errno = EBADF; /* Cleanup must not hide the actual operation error. */
}
int hpclang_aplet_data_get(struct hpclang_object **out)
{
    if (fault) {
        errno = fault;
        return -1;
    }
    if (!present && !aplet) {
        errno = ENOENT;
        return -1;
    }
    *out = malloc(sizeof(**out));
    assert(*out);
    ++owners;
    **out = present ? saved : (struct hpclang_object){.kind = HPCLANG_OBJECT_LIST};
    return 0;
}
int hpclang_object_store_get(const struct hpclang_object_store *store, const uint8_t *name,
                             uint32_t length, struct hpclang_object **out)
{
    assert(store == &home && length == 7 && !memcmp(name, "HPCNOTE", length));
    return hpclang_aplet_data_get(out);
}
int hpclang_aplet_data_set(const struct hpclang_object *object)
{
    if (write_fault) {
        errno = write_fault;
        return -1;
    }
    saved = *object;
    present = 1;
    ++writes;
    return 0;
}
int hpclang_object_store_put(const struct hpclang_object_store *store, const uint8_t *name,
                             uint32_t length, const struct hpclang_object *object,
                             enum hpclang_object_store_put_mode mode)
{
    assert(store == &home && length == 7 && !memcmp(name, "HPCNOTE", length));
    assert(mode ==
           (present ? HPCLANG_OBJECT_STORE_REPLACE_ONLY : HPCLANG_OBJECT_STORE_CREATE_ONLY));
    last_mode = mode;
    return hpclang_aplet_data_set(object);
}
enum hpclang_object_kind hpclang_object_kind(const struct hpclang_object *object)
{
    return object->kind;
}
int hpclang_object_list_size(const struct hpclang_object *object, size_t *size)
{
    *size = object->length;
    return 0;
}
int hpclang_object_string_size(const struct hpclang_object *object, size_t *size)
{
    *size = object->length;
    return 0;
}
int hpclang_object_string_copy(const struct hpclang_object *object, uint8_t *bytes, size_t capacity)
{
    if (copy_fault) {
        errno = copy_fault;
        return -1;
    }
    assert(capacity >= object->length);
    memcpy(bytes, object->bytes, object->length);
    return 0;
}
int hpclang_object_create_string(const uint8_t *bytes, size_t length, struct hpclang_object **out)
{
    if (create_fault) {
        errno = create_fault;
        return -1;
    }
    assert(length <= sizeof(saved.bytes));
    *out = malloc(sizeof(**out));
    assert(*out);
    ++owners;
    **out = (struct hpclang_object){.kind = HPCLANG_OBJECT_STRING, .length = length};
    memcpy((*out)->bytes, bytes, length);
    return 0;
}
void hpclang_object_release(struct hpclang_object *object)
{
    if (object) {
        --owners;
        free(object);
    }
    errno = EBADF;
}

static void run(int use_aplet)
{
    aplet = use_aplet;
    present = writes = fault = 0;
    char text[NOTEBOOK_CAPACITY] = "unchanged";
    assert(notebook_load(text) == -1 && errno == ENOENT && !strcmp(text, "unchanged"));
    assert(notebook_save("hello \xc3\xa9") == 0 && writes == 1);
    if (!aplet)
        assert(last_mode == HPCLANG_OBJECT_STORE_CREATE_ONLY);
    assert(notebook_load(text) == 0 && !strcmp(text, "hello \xc3\xa9"));
    const char *valid[] = {"\xc2\xa0", "\xe4\xb8\xad", "\xf0\x9f\x98\x80", "\xf4\x8f\xbf\xbf"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        assert(notebook_save(valid[i]) == 0);
        assert(notebook_load(text) == 0 && !strcmp(text, valid[i]));
    }
    int unicode_writes = writes;
    const char *invalid[] = {"\n",
                             "\x7f",
                             "\xc2\x80",
                             "\xc0\xaf",
                             "\x80",
                             "\xe0\x80\xaf",
                             "\xed\xa0\x80",
                             "\xf4\x90\x80\x80",
                             "\xf5\x80\x80\x80",
                             "\xf0\x80\x80\xaf",
                             "\xc2",
                             "\xe4\xb8",
                             "\xf0\x9f\x98",
                             "\xe4?x"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(notebook_save(invalid[i]) == -1 && errno == EILSEQ && writes == unicode_writes);
        struct hpclang_object original = saved;
        const size_t prefix = sizeof("hpclang-notebook/1\n") - 1;
        memcpy(saved.bytes + prefix, invalid[i], strlen(invalid[i]));
        saved.length = prefix + strlen(invalid[i]);
        strcpy(text, "unchanged");
        assert(notebook_load(text) == -1 && errno == EILSEQ && !strcmp(text, "unchanged"));
        assert(notebook_save("no") == -1 && errno == EILSEQ && writes == unicode_writes);
        saved = original;
    }
    assert(notebook_save("") == 0 && writes == unicode_writes + 1);
    assert(notebook_load(text) == 0 && text[0] == 0);
    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;
    assert(notebook_save(text) == 0 && notebook_load(text) == 0);
    assert(strlen(text) == sizeof(text) - 1);
    int old_writes = writes;
    memset(text, 'x', sizeof(text));
    assert(notebook_save(text) == -1 && errno == E2BIG && writes == old_writes);
    strcpy(text, "unchanged");
    fault = EIO;
    assert(notebook_load(text) == -1 && errno == EIO && !strcmp(text, "unchanged"));
    assert(notebook_save("no") == -1 && errno == EIO && writes == old_writes);
    fault = 0;
    struct hpclang_object before = saved;
    create_fault = ENOMEM;
    assert(notebook_save("no") == -1 && errno == ENOMEM && writes == old_writes);
    create_fault = 0;
    write_fault = ENOSPC;
    assert(notebook_save("no") == -1 && errno == ENOSPC && writes == old_writes);
    write_fault = 0;
    copy_fault = EIO;
    assert(notebook_load(text) == -1 && errno == EIO && !strcmp(text, "unchanged"));
    assert(notebook_save("no") == -1 && errno == EIO && writes == old_writes);
    copy_fault = 0;
    assert(!memcmp(&saved, &before, sizeof(saved)));
    saved.bytes[saved.length - 1] = 0;
    assert(notebook_load(text) == -1 && errno == EILSEQ && !strcmp(text, "unchanged"));
    assert(notebook_save("no") == -1 && errno == EILSEQ && writes == old_writes);
    saved = before;
    saved.length = sizeof(saved.bytes);
    assert(notebook_save("no") == -1 && errno == EILSEQ && writes == old_writes);
    saved = before;
    saved.bytes[0] = '!';
    assert(notebook_load(text) == -1 && errno == EILSEQ && !strcmp(text, "unchanged"));
    assert(notebook_save("no") == -1 && errno == EILSEQ && writes == old_writes);
    saved.kind = HPCLANG_OBJECT_LIST;
    saved.length = 1;
    assert(notebook_save("no") == -1 && errno == EILSEQ && writes == old_writes);
    supported = 0;
    assert(notebook_save("no") == -1 && errno == ENOSYS && writes == old_writes);
    assert(notebook_load(text) == -1 && errno == ENOSYS && !strcmp(text, "unchanged"));
    supported = 1;
    assert(notebook_load(NULL) == -1 && errno == EINVAL);
    assert(notebook_save(NULL) == -1 && errno == EINVAL);
    assert(owners == 0 && handles == 0);
}
int main(void)
{
    run(0);
    run(1);
    return 0;
}
