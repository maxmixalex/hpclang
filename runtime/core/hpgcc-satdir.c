#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object-store.h>
#include <hpclang/object.h>
#include <satdir.h>

enum
{
    SATDIR_PATH_CAPACITY = 256,
    SATDIR_MAX_DEPTH = 64
};

struct sat_compat_object
{
    SAT_OBJ_DSCR descriptor;
    struct hpclang_object *snapshot;
    char *path;
    struct sat_compat_object *next;
};

static char satdir_cwd[SATDIR_PATH_CAPACITY] = "HOME";
static struct sat_compat_object *satdir_objects;
static unsigned satdir_next_token = 1u;

static char *satdir_strdup(const char *text)
{
    size_t length = strlen(text) + 1u;
    char *copy = malloc(length);
    if (copy != NULL)
        memcpy(copy, text, length);
    return copy;
}

static int satdir_set_home(char *out)
{
    memcpy(out, "HOME", 5u);
    return 0;
}

static int satdir_pop_component(char *path)
{
    char *slash = strrchr(path, '/');
    if (slash == NULL) {
        errno = ENOENT;
        return -1;
    }
    *slash = '\0';
    return 0;
}

static int satdir_append_component(char *path, const char *component, size_t length)
{
    size_t used = strlen(path);
    if (length == 0u)
        return 0;
    if (used + 1u + length >= SATDIR_PATH_CAPACITY) {
        errno = ENAMETOOLONG;
        return -1;
    }
    path[used++] = '/';
    memcpy(path + used, component, length);
    path[used + length] = '\0';
    return 0;
}

static int satdir_normalize_directory(const char *path, char *out)
{
    if (path == NULL || out == NULL) {
        errno = EFAULT;
        return -1;
    }

    const char *cursor = path;
    if (strcmp(cursor, "HOME") == 0) {
        return satdir_set_home(out);
    } else if (strncmp(cursor, "HOME/", 5u) == 0) {
        satdir_set_home(out);
        cursor += 5u;
    } else if (*cursor == '/') {
        satdir_set_home(out);
        while (*cursor == '/')
            ++cursor;
    } else {
        size_t length = strlen(satdir_cwd);
        if (length >= SATDIR_PATH_CAPACITY) {
            errno = EILSEQ;
            return -1;
        }
        memcpy(out, satdir_cwd, length + 1u);
    }

    while (*cursor != '\0') {
        while (*cursor == '/')
            ++cursor;
        if (*cursor == '\0')
            break;

        const char *start = cursor;
        while (*cursor != '\0' && *cursor != '/')
            ++cursor;
        size_t length = (size_t)(cursor - start);

        if (length == 1u && start[0] == '.')
            continue;
        if (length == 2u && start[0] == '.' && start[1] == '.') {
            if (strcmp(out, "HOME") == 0) {
                errno = ENOENT;
                return -1;
            }
            if (satdir_pop_component(out) != 0)
                return -1;
            continue;
        }
        if (satdir_append_component(out, start, length) != 0)
            return -1;
    }
    return 0;
}

static struct hpclang_object_store *satdir_resolve_directory(const char *normalized)
{
    if (normalized == NULL ||
        (strcmp(normalized, "HOME") != 0 && strncmp(normalized, "HOME/", 5u) != 0)) {
        errno = EINVAL;
        return NULL;
    }

    struct hpclang_object_store *store = hpclang_object_store_home();
    if (store == NULL)
        return NULL;

    const char *cursor = normalized + 4u;
    if (*cursor == '/')
        ++cursor;

    while (*cursor != '\0') {
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != '/')
            ++cursor;
        size_t length = (size_t)(cursor - start);
        if (length >= HPCLANG_OBJECT_STORE_NAME_CAPACITY) {
            hpclang_object_store_close(store);
            errno = ENAMETOOLONG;
            return NULL;
        }

        struct hpclang_object_store *next =
            hpclang_object_store_open_child(store, (const uint8_t *)start, (uint32_t)length);
        hpclang_object_store_close(store);
        if (next == NULL)
            return NULL;
        store = next;
        if (*cursor == '/')
            ++cursor;
    }
    return store;
}

static int satdir_split_object_path(const char *path, char *directory, const char **base,
                                    size_t *base_length)
{
    if (path == NULL || directory == NULL || base == NULL || base_length == NULL) {
        errno = EFAULT;
        return -1;
    }

    const char *slash = strrchr(path, '/');
    if (slash == NULL) {
        if (strcmp(path, "HOME") == 0) {
            errno = EISDIR;
            return -1;
        }
        if (satdir_normalize_directory(".", directory) != 0)
            return -1;
        *base = path;
        *base_length = strlen(path);
        if (*base_length == 0u) {
            errno = EINVAL;
            return -1;
        }
        return 0;
    }

    size_t dir_length = (size_t)(slash - path);
    char raw_dir[SATDIR_PATH_CAPACITY];
    if (dir_length == 0u) {
        memcpy(raw_dir, "/", 2u);
    } else {
        if (dir_length >= sizeof(raw_dir)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(raw_dir, path, dir_length);
        raw_dir[dir_length] = '\0';
    }
    if (satdir_normalize_directory(raw_dir, directory) != 0)
        return -1;

    *base = slash + 1u;
    *base_length = strlen(*base);
    if (*base_length == 0u) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static struct sat_compat_object *satdir_find_cached(const char *path)
{
    for (struct sat_compat_object *item = satdir_objects; item != NULL; item = item->next) {
        if (strcmp(item->path, path) == 0)
            return item;
    }
    return NULL;
}

static SAT_OBJ_DSCR *satdir_cache_object(const char *directory, const char *name,
                                         size_t name_length, struct hpclang_object *snapshot)
{
    char full_path[SATDIR_PATH_CAPACITY];
    size_t dir_length = strlen(directory);
    if (dir_length + 1u + name_length >= sizeof(full_path)) {
        hpclang_object_release(snapshot);
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(full_path, directory, dir_length);
    full_path[dir_length] = '/';
    memcpy(full_path + dir_length + 1u, name, name_length);
    full_path[dir_length + 1u + name_length] = '\0';

    struct sat_compat_object *cached = satdir_find_cached(full_path);
    if (cached != NULL) {
        hpclang_object_release(snapshot);
        return &cached->descriptor;
    }

    struct sat_compat_object *item = calloc(1u, sizeof(*item));
    if (item == NULL) {
        hpclang_object_release(snapshot);
        errno = ENOMEM;
        return NULL;
    }
    item->descriptor.name = malloc(name_length + 1u);
    item->path = satdir_strdup(full_path);
    if (item->descriptor.name == NULL || item->path == NULL) {
        free(item->descriptor.name);
        free(item->path);
        free(item);
        hpclang_object_release(snapshot);
        errno = ENOMEM;
        return NULL;
    }
    memcpy(item->descriptor.name, name, name_length);
    item->descriptor.name[name_length] = '\0';

    /*
     * Historical SAT_OBJ_DSCR.addr was a Saturn address. hpclang never makes
     * that borrowed address public. Use a conspicuously non-Saturn token so
     * accidental raw-address mixing fails instead of appearing to work.
     */
    item->descriptor.addr = 0xf0000000u | (satdir_next_token++ & 0x0fffffffu);
    if ((item->descriptor.addr & 0x0fffffffu) == 0u)
        item->descriptor.addr = 0xf0000001u;

    item->snapshot = snapshot;
    item->next = satdir_objects;
    satdir_objects = item;
    return &item->descriptor;
}

static SAT_OBJ_DSCR *satdir_get_from_directory(const char *directory, const char *name,
                                               size_t name_length)
{
    if (name_length >= HPCLANG_OBJECT_STORE_NAME_CAPACITY) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char full_path[SATDIR_PATH_CAPACITY];
    size_t dir_length = strlen(directory);
    if (dir_length + 1u + name_length >= sizeof(full_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(full_path, directory, dir_length);
    full_path[dir_length] = '/';
    memcpy(full_path + dir_length + 1u, name, name_length);
    full_path[dir_length + 1u + name_length] = '\0';

    struct sat_compat_object *cached = satdir_find_cached(full_path);
    if (cached != NULL)
        return &cached->descriptor;

    struct hpclang_object_store *store = satdir_resolve_directory(directory);
    if (store == NULL)
        return NULL;

    struct hpclang_object *object = NULL;
    int result =
        hpclang_object_store_get(store, (const uint8_t *)name, (uint32_t)name_length, &object);
    hpclang_object_store_close(store);
    if (result != 0)
        return NULL;

    return satdir_cache_object(directory, name, name_length, object);
}

static struct sat_compat_object *satdir_private_object(SAT_OBJ_DSCR *descriptor)
{
    for (struct sat_compat_object *item = satdir_objects; item != NULL; item = item->next) {
        if (&item->descriptor == descriptor)
            return item;
    }
    return NULL;
}

void sat_dir_open(void)
{
    struct hpclang_object_store *home = hpclang_object_store_home();
    if (home != NULL) {
        hpclang_object_store_close(home);
        memcpy(satdir_cwd, "HOME", 5u);
    }
}

void sat_dir_close(void)
{
    struct sat_compat_object *item = satdir_objects;
    while (item != NULL) {
        struct sat_compat_object *next = item->next;
        hpclang_object_release(item->snapshot);
        free(item->descriptor.name);
        free(item->path);
        free(item);
        item = next;
    }
    satdir_objects = NULL;
    satdir_next_token = 1u;
    memcpy(satdir_cwd, "HOME", 5u);
}

char *sat_dir_curdir(void)
{
    char *copy = satdir_strdup(satdir_cwd);
    if (copy == NULL)
        errno = ENOMEM;
    return copy;
}

int sat_dir_chdir(char *path)
{
    char normalized[SATDIR_PATH_CAPACITY];
    if (satdir_normalize_directory(path, normalized) != 0)
        return -1;

    struct hpclang_object_store *store = satdir_resolve_directory(normalized);
    if (store == NULL)
        return -1;
    hpclang_object_store_close(store);

    memcpy(satdir_cwd, normalized, strlen(normalized) + 1u);
    return 0;
}

SAT_OBJ_DSCR *sat_dir_getobject(char *path)
{
    char directory[SATDIR_PATH_CAPACITY];
    const char *base = NULL;
    size_t base_length = 0u;
    if (satdir_split_object_path(path, directory, &base, &base_length) != 0) {
        return NULL;
    }
    return satdir_get_from_directory(directory, base, base_length);
}

SAT_OBJ_DSCR *sat_dir_rclobject(char *path)
{
    if (path == NULL) {
        errno = EFAULT;
        return NULL;
    }

    if (*path == '/' || strcmp(path, "HOME") == 0 || strncmp(path, "HOME/", 5u) == 0) {
        return sat_dir_getobject(path);
    }

    char directory[SATDIR_PATH_CAPACITY];
    const char *base = NULL;
    size_t base_length = 0u;
    if (satdir_split_object_path(path, directory, &base, &base_length) != 0) {
        return NULL;
    }

    for (;;) {
        SAT_OBJ_DSCR *object = satdir_get_from_directory(directory, base, base_length);
        if (object != NULL)
            return object;
        if (errno != ENOENT && errno != EISDIR)
            return NULL;
        if (strcmp(directory, "HOME") == 0)
            return NULL;
        if (satdir_pop_component(directory) != 0)
            return NULL;
    }
}

static int satdir_entry_text_name(const struct hpclang_object_store_entry *entry, const char **name,
                                  size_t *length)
{
    if (entry->name_length == 0u || entry->name_length >= HPCLANG_OBJECT_STORE_NAME_CAPACITY) {
        return 0;
    }
    if (memchr(entry->name, '\0', entry->name_length) != NULL ||
        memchr(entry->name, '/', entry->name_length) != NULL) {
        return 0;
    }
    *name = (const char *)entry->name;
    *length = entry->name_length;
    return 1;
}

static int satdir_search_directory(const char *directory, const char *target, size_t target_length,
                                   SAT_DIR_SLIST **result, unsigned depth)
{
    if (depth > SATDIR_MAX_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    struct hpclang_object_store *store = satdir_resolve_directory(directory);
    if (store == NULL)
        return -1;

    struct hpclang_object_store_iterator *iterator = hpclang_object_store_iterator_open(store);
    hpclang_object_store_close(store);
    if (iterator == NULL)
        return -1;

    int count = 0;
    struct hpclang_object_store_entry entry;
    for (;;) {
        memset(&entry, 0, sizeof(entry));
        entry.size = sizeof(entry);
        entry.version = HPCLANG_OBJECT_STORE_ENTRY_VERSION;

        int read = hpclang_object_store_iterator_read(iterator, &entry);
        if (read == 0)
            break;
        if (read < 0) {
            hpclang_object_store_iterator_close(iterator);
            return -1;
        }

        const char *entry_name = NULL;
        size_t entry_length = 0u;
        if (!satdir_entry_text_name(&entry, &entry_name, &entry_length))
            continue;

        if ((entry.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u) {
            char child[SATDIR_PATH_CAPACITY];
            size_t dir_length = strlen(directory);
            if (dir_length + 1u + entry_length >= sizeof(child))
                continue;
            memcpy(child, directory, dir_length);
            child[dir_length] = '/';
            memcpy(child + dir_length + 1u, entry_name, entry_length);
            child[dir_length + 1u + entry_length] = '\0';

            int nested = satdir_search_directory(child, target, target_length, result, depth + 1u);
            if (nested < 0) {
                hpclang_object_store_iterator_close(iterator);
                return -1;
            }
            if (nested > INT_MAX - count) {
                hpclang_object_store_iterator_close(iterator);
                errno = EOVERFLOW;
                return -1;
            }
            count += nested;
            continue;
        }

        if (entry_length == target_length && memcmp(entry_name, target, target_length) == 0) {
            size_t dir_length = strlen(directory);
            if (dir_length + 1u + entry_length >= SATDIR_PATH_CAPACITY)
                continue;

            SAT_DIR_SLIST *node = calloc(1u, sizeof(*node));
            if (node == NULL) {
                hpclang_object_store_iterator_close(iterator);
                errno = ENOMEM;
                return -1;
            }
            node->str = malloc(dir_length + 1u + entry_length + 1u);
            if (node->str == NULL) {
                free(node);
                hpclang_object_store_iterator_close(iterator);
                errno = ENOMEM;
                return -1;
            }
            memcpy(node->str, directory, dir_length);
            node->str[dir_length] = '/';
            memcpy(node->str + dir_length + 1u, entry_name, entry_length);
            node->str[dir_length + 1u + entry_length] = '\0';
            node->next = *result;
            *result = node;
            ++count;
        }
    }

    hpclang_object_store_iterator_close(iterator);
    return count;
}

void _sat_dir_destroy_slist(SAT_DIR_SLIST *list)
{
    while (list != NULL) {
        SAT_DIR_SLIST *next = list->next;
        free(list->str);
        free(list);
        list = next;
    }
}

int sat_dir_searchobject(char *name, SAT_DIR_SLIST **path_list)
{
    if (name == NULL || path_list == NULL) {
        errno = EFAULT;
        return -1;
    }
    *path_list = NULL;

    size_t length = strlen(name);
    if (length == 0u || length >= HPCLANG_OBJECT_STORE_NAME_CAPACITY || strchr(name, '/') != NULL) {
        errno = EINVAL;
        return -1;
    }

    int result = satdir_search_directory("HOME", name, length, path_list, 0u);
    if (result < 0) {
        _sat_dir_destroy_slist(*path_list);
        *path_list = NULL;
    }
    return result;
}

int sat_obj_is_real(SAT_OBJ_DSCR *obj)
{
    struct sat_compat_object *item = satdir_private_object(obj);
    return item != NULL && hpclang_object_kind(item->snapshot) == HPCLANG_OBJECT_REAL;
}

double _sat_dir_fetchreal(char *path, double defval, char *mode)
{
    if (mode == NULL)
        return defval;

    SAT_OBJ_DSCR *descriptor = *mode == 'r' ? sat_dir_rclobject(path) : sat_dir_getobject(path);
    struct sat_compat_object *item = satdir_private_object(descriptor);
    if (item == NULL || hpclang_object_kind(item->snapshot) != HPCLANG_OBJECT_REAL) {
        return defval;
    }

    double value = defval;
    if (hpclang_object_real_f64(item->snapshot, &value) != 0)
        return defval;
    return value;
}
