#include "../../examples/notebook/store.h"
#include <hpclang/aplet.h>
#include <hpclang/device.h>
#include <hpclang/object-store.h>
#include <hpclang/port.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int open_error, output_error, close_error, interrupted, zero_write;
static int opens, closes;
static size_t exported_length;
static char exported[NOTEBOOK_CAPACITY];

int open(const char *path, int flags, ...)
{
    assert(!strcmp(path, "/HPCNOTE.TXT"));
    assert(flags == (O_WRONLY | O_CREAT | O_EXCL));
    ++opens;
    if (open_error) {
        errno = open_error;
        return -1;
    }
    return 42;
}
ssize_t write(int fd, const void *bytes, size_t length)
{
    assert(fd == 42);
    if (interrupted) {
        interrupted = 0;
        errno = EINTR;
        return -1;
    }
    if (output_error) {
        errno = output_error;
        return -1;
    }
    if (zero_write)
        return 0;
    size_t count = length > 2 ? 2 : length;
    assert(exported_length + count <= sizeof(exported));
    memcpy(exported + exported_length, bytes, count);
    exported_length += count;
    return (ssize_t)count;
}
int close(int fd)
{
    assert(fd == 42);
    ++closes;
    errno = close_error;
    return close_error ? -1 : 0;
}

static void files(void)
{
    assert(notebook_export_file(NULL, "a") == -1 && errno == EINVAL && opens == 0);
    assert(notebook_export_file("", "a") == -1 && errno == EINVAL && opens == 0);
    assert(notebook_export_file("/HPCNOTE.TXT", "\n") == -1 && errno == EILSEQ && opens == 0);
    open_error = EEXIST;
    assert(notebook_export_file("/HPCNOTE.TXT", "a") == -1 && errno == EEXIST);
    assert(opens == 1 && closes == 0 && exported_length == 0);
    open_error = 0;
    interrupted = 1;
    assert(notebook_export_file("/HPCNOTE.TXT", "hello \xc3\xa9") == 0);
    assert(exported_length == 9 && !memcmp(exported, "hello \xc3\xa9\n", 9));
    assert(opens == 2 && closes == 1);
    output_error = ENOSPC;
    close_error = EIO;
    assert(notebook_export_file("/HPCNOTE.TXT", "a") == -1 && errno == ENOSPC);
    assert(closes == 2);
    output_error = close_error = 0;
    zero_write = 1;
    assert(notebook_export_file("/HPCNOTE.TXT", "a") == -1 && errno == EIO);
    assert(closes == 3);
    zero_write = 0;
    exported_length = 0;
    close_error = EIO;
    assert(notebook_export_file("/HPCNOTE.TXT", "") == -1 && errno == EIO);
    assert(closes == 4 && exported_length == 1 && exported[0] == '\n');
    close_error = 0;
    exported_length = 0;
    char maximum[NOTEBOOK_CAPACITY];
    memset(maximum, 'x', sizeof(maximum));
    maximum[sizeof(maximum) - 1] = 0;
    assert(notebook_export_file("/HPCNOTE.TXT", maximum) == 0);
    assert(exported_length == sizeof(exported) && exported[sizeof(exported) - 1] == '\n');
    assert(!memcmp(exported, maximum, sizeof(maximum) - 1));
}

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
static uint32_t port_operations;
static unsigned selected_port;

int hpclang_port_query(uint32_t port, struct hpclang_port_info *info)
{
    assert(port == selected_port && info->size == sizeof(*info) &&
           info->version == HPCLANG_PORT_INFO_VERSION);
    info->available = port_operations;
    return 0;
}

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

struct hpclang_object *hpclang_port_backup_get(uint32_t port, const uint8_t *name, uint32_t length)
{
    assert(port == selected_port && length == 7 && !memcmp(name, "HPCNOTE", length));
    struct hpclang_object *object = NULL;
    return hpclang_aplet_data_get(&object) ? NULL : object;
}
int hpclang_port_backup_create(uint32_t port, const uint8_t *name, uint32_t length,
                               const struct hpclang_object *object)
{
    assert(port == selected_port && length == 7 && !memcmp(name, "HPCNOTE", length));
    assert(!present && (port_operations & HPCLANG_PORT_CREATE_BACKUP));
    return hpclang_aplet_data_set(object);
}
int hpclang_port_backup_replace(uint32_t port, const uint8_t *name, uint32_t length,
                                const struct hpclang_object *object)
{
    assert(port == selected_port && length == 7 && !memcmp(name, "HPCNOTE", length));
    assert(present && (port_operations & HPCLANG_PORT_REPLACE_BACKUP));
    return hpclang_aplet_data_set(object);
}

static void ports(void)
{
    aplet = 0;
    for (selected_port = 0; selected_port < 3; ++selected_port) {
        present = writes = 0;
        port_operations = HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_CREATE_BACKUP;
        char text[NOTEBOOK_CAPACITY] = "retained";
        assert(notebook_backup_load(selected_port, text) == -1 && errno == ENOENT);
        assert(!strcmp(text, "retained"));
        assert(notebook_backup_save(selected_port, "backup") == 0 && writes == 1);
        assert(notebook_backup_load(selected_port, text) == 0 && !strcmp(text, "backup"));
        assert(notebook_backup_save(selected_port, "new") == -1 && errno == ENOSYS && writes == 1);
        port_operations |= HPCLANG_PORT_REPLACE_BACKUP;
        assert(notebook_backup_save(selected_port, "new") == 0 && writes == 2);
        assert(notebook_backup_load(selected_port, text) == 0 && !strcmp(text, "new"));
        saved.bytes[0] = '!';
        assert(notebook_backup_save(selected_port, "no") == -1 && errno == EILSEQ && writes == 2);
        assert(notebook_backup_load(selected_port, text) == -1 && errno == EILSEQ &&
               !strcmp(text, "new"));
        port_operations = HPCLANG_PORT_CREATE_BACKUP;
        assert(notebook_backup_save(selected_port, "no") == -1 && errno == ENOSYS && writes == 2);
        port_operations = HPCLANG_PORT_READ_BACKUP;
        present = 0;
        assert(notebook_backup_save(selected_port, "no") == -1 && errno == ENOSYS && writes == 2);
        assert(owners == 0 && handles == 0);
    }
    assert(notebook_backup_save(3, "no") == -1 && errno == EINVAL);
    char text[NOTEBOOK_CAPACITY];
    assert(notebook_backup_load(~0u, text) == -1 && errno == EINVAL);
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
    ports();
    files();
    return 0;
}
