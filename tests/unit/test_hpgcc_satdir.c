#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object-store.h>
#include <satdir.h>

struct fake_entry;

struct fake_node
{
    const char *name;
    const struct fake_entry *entries;
    size_t count;
};

struct fake_entry
{
    const char *name;
    int is_directory;
    double value;
    const struct fake_node *child;
};

static const struct fake_entry grand_entries[] = {
    {"A", 0, 4.5, NULL},
};
static const struct fake_node grand = {
    "GRAND",
    grand_entries,
    1u,
};

static const struct fake_entry child_entries[] = {
    {"A", 0, 2.5, NULL},
    {"B", 0, 3.5, NULL},
    {"GRAND", 1, 0.0, &grand},
};
static const struct fake_node child = {
    "CHILD",
    child_entries,
    3u,
};

static const struct fake_entry home_entries[] = {
    {"A", 0, 1.5, NULL},
    {"ROOTONLY", 0, 9.5, NULL},
    {"CHILD", 1, 0.0, &child},
};
static const struct fake_node home = {
    "HOME",
    home_entries,
    3u,
};

struct hpclang_object_store
{
    const struct fake_node *node;
};

struct hpclang_object_store_iterator
{
    const struct fake_node *node;
    size_t index;
};

struct hpclang_object
{
    double value;
};

static const struct fake_entry *find_entry(const struct fake_node *node, const uint8_t *name,
                                           uint32_t name_length)
{
    for (size_t index = 0u; index < node->count; ++index) {
        const struct fake_entry *entry = &node->entries[index];
        if (strlen(entry->name) == name_length && memcmp(entry->name, name, name_length) == 0) {
            return entry;
        }
    }
    return NULL;
}

struct hpclang_object_store *hpclang_object_store_home(void)
{
    struct hpclang_object_store *store = malloc(sizeof(*store));
    assert(store != NULL);
    store->node = &home;
    return store;
}

struct hpclang_object_store *
hpclang_object_store_open_child(const struct hpclang_object_store *parent, const uint8_t *name,
                                uint32_t name_length)
{
    const struct fake_entry *entry = find_entry(parent->node, name, name_length);
    if (entry == NULL) {
        errno = ENOENT;
        return NULL;
    }
    if (!entry->is_directory) {
        errno = ENOTDIR;
        return NULL;
    }

    struct hpclang_object_store *store = malloc(sizeof(*store));
    assert(store != NULL);
    store->node = entry->child;
    return store;
}

void hpclang_object_store_close(struct hpclang_object_store *store)
{
    free(store);
}

int hpclang_object_store_get(const struct hpclang_object_store *store, const uint8_t *name,
                             uint32_t name_length, struct hpclang_object **object)
{
    const struct fake_entry *entry = find_entry(store->node, name, name_length);
    if (entry == NULL) {
        errno = ENOENT;
        return -1;
    }
    if (entry->is_directory) {
        errno = EISDIR;
        return -1;
    }

    *object = malloc(sizeof(**object));
    assert(*object != NULL);
    (*object)->value = entry->value;
    return 0;
}

struct hpclang_object_store_iterator *
hpclang_object_store_iterator_open(const struct hpclang_object_store *store)
{
    struct hpclang_object_store_iterator *iterator = malloc(sizeof(*iterator));
    assert(iterator != NULL);
    iterator->node = store->node;
    iterator->index = 0u;
    return iterator;
}

int hpclang_object_store_iterator_read(struct hpclang_object_store_iterator *iterator,
                                       struct hpclang_object_store_entry *out)
{
    if (iterator->index >= iterator->node->count)
        return 0;

    const struct fake_entry *entry = &iterator->node->entries[iterator->index++];
    out->name_length = (uint32_t)strlen(entry->name);
    memcpy(out->name, entry->name, out->name_length);
    out->flags = entry->is_directory ? HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY : 0u;
    out->kind = entry->is_directory ? HPCLANG_OBJECT_DIRECTORY : HPCLANG_OBJECT_REAL;
    return 1;
}

void hpclang_object_store_iterator_close(struct hpclang_object_store_iterator *iterator)
{
    free(iterator);
}

enum hpclang_object_kind hpclang_object_kind(const struct hpclang_object *object)
{
    (void)object;
    return HPCLANG_OBJECT_REAL;
}

void hpclang_object_release(struct hpclang_object *object)
{
    free(object);
}

int hpclang_object_real_f64(const struct hpclang_object *object, double *value)
{
    *value = object->value;
    return 0;
}

static int path_list_contains(SAT_DIR_SLIST *list, const char *path)
{
    for (; list != NULL; list = list->next) {
        if (strcmp(list->str, path) == 0)
            return 1;
    }
    return 0;
}

int main(void)
{
    sat_dir_open();

    char *cwd = sat_dir_curdir();
    assert(cwd != NULL);
    assert(strcmp(cwd, "HOME") == 0);
    free(cwd);

    assert(sat_dir_getreal("A", -1.0) == 1.5);

    assert(sat_dir_chdir("CHILD") == 0);
    cwd = sat_dir_curdir();
    assert(cwd != NULL);
    assert(strcmp(cwd, "HOME/CHILD") == 0);
    free(cwd);

    assert(sat_dir_getreal("A", -1.0) == 2.5);
    assert(sat_dir_rclreal("ROOTONLY", -1.0) == 9.5);

    assert(sat_dir_chdir("GRAND") == 0);
    assert(sat_dir_rclreal("B", -1.0) == 3.5);
    assert(sat_dir_rclreal("A", -1.0) == 4.5);

    assert(sat_dir_chdir("../..") == 0);
    cwd = sat_dir_curdir();
    assert(cwd != NULL);
    assert(strcmp(cwd, "HOME") == 0);
    free(cwd);

    SAT_DIR_SLIST *paths = NULL;
    assert(sat_dir_searchobject("A", &paths) == 3);
    assert(path_list_contains(paths, "HOME/A"));
    assert(path_list_contains(paths, "HOME/CHILD/A"));
    assert(path_list_contains(paths, "HOME/CHILD/GRAND/A"));
    _sat_dir_destroy_slist(paths);

    SAT_OBJ_DSCR *descriptor = sat_dir_getobject("HOME/A");
    assert(descriptor != NULL);
    assert(sat_obj_is_real(descriptor));
    assert((descriptor->addr & 0xf0000000u) == 0xf0000000u);

    sat_dir_close();
    return 0;
}
