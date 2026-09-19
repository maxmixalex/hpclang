#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fsystem.h>
#include <hpclang/storage.h>

enum {
    HPCLANG_HPGCC_FS_FILE = 1,
    HPCLANG_HPGCC_FS_DIRECTORY = 2,
    HPCLANG_HPGCC_FS_ENTRY = 3
};

static char *compat_strdup(const char *text)
{
    if (text == NULL)
        return NULL;
    size_t size = strlen(text) + 1u;
    char *copy = malloc(size);
    if (copy != NULL)
        memcpy(copy, text, size);
    return copy;
}

static int compat_error(void)
{
    switch (errno) {
    case EINVAL:
    case EFAULT:
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
#endif
        return FS_BADNAME;
    case ENOENT:
        return FS_NOTFOUND;
    case EACCES:
    case EROFS:
#ifdef EPERM
    case EPERM:
#endif
        return FS_CANTWRITE;
    case EXDEV:
        return FS_BADVOLUME;
    case ENODEV:
#ifdef ENXIO
    case ENXIO:
#endif
        return FS_NOCARD;
    case EMFILE:
#ifdef ENFILE
    case ENFILE:
#endif
        return FS_MAXFILES;
    case ENOSPC:
        return FS_DISKFULL;
    case EEXIST:
        return FS_EXIST;
    case EBADF:
        return FS_INVHANDLE;
    case EBUSY:
#ifdef ENOTEMPTY
    case ENOTEMPTY:
#endif
        return FS_USED;
    default:
        return FS_ERROR;
    }
}

static char compat_cwd[HPCLANG_STREAM_NAME_CAPACITY] = "/";

static int valid_volume_char(char value)
{
    return (value >= '3' && value <= '6') ||
           (value >= 'C' && value <= 'F') ||
           (value >= 'c' && value <= 'f');
}

static int supported_volume_char(char value)
{
    return value == '3' || value == 'C' || value == 'c';
}

static int parse_volume_prefix(const char **source)
{
    const char *p = *source;
    char volume = 0;

    if (p[0] == ':' && p[1] != '\0' && p[2] == ':') {
        volume = p[1];
        if (!valid_volume_char(volume)) {
            errno = EINVAL;
            return -1;
        }
        p += 3;
    } else if (p[0] != '\0' && p[1] == ':') {
        volume = p[0];
        if (!valid_volume_char(volume)) {
            errno = EINVAL;
            return -1;
        }
        p += 2;
    }

    if (volume != 0 && !supported_volume_char(volume)) {
        errno = EXDEV;
        return -1;
    }

    *source = p;
    return 0;
}

static int pop_component(char *path, size_t *length)
{
    if (*length <= 1u)
        return 0;

    size_t out = *length;
    while (out > 1u && path[out - 1u] != '/')
        --out;
    if (out > 1u)
        --out;

    path[out] = '\0';
    *length = out;
    return 0;
}

static int append_component(
    char *path,
    size_t *length,
    size_t capacity,
    const char *component,
    size_t component_length)
{
    if (component_length == 0u)
        return 0;

    if (component_length == 1u && component[0] == '.')
        return 0;
    if (component_length == 2u &&
        component[0] == '.' && component[1] == '.')
        return pop_component(path, length);

    size_t need_separator = *length > 1u ? 1u : 0u;
    if (*length + need_separator + component_length >= capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (need_separator != 0u)
        path[(*length)++] = '/';
    memcpy(path + *length, component, component_length);
    *length += component_length;
    path[*length] = '\0';
    return 0;
}

static int normalize_path(const char *source, char *dest, size_t capacity)
{
    if (source == NULL || dest == NULL || capacity < 2u) {
        errno = EINVAL;
        return -1;
    }

    const char *p = source;
    if (parse_volume_prefix(&p) != 0)
        return -1;

    int absolute = *p == '/' || *p == '\\';
    size_t out;

    if (absolute) {
        dest[0] = '/';
        dest[1] = '\0';
        out = 1u;
        ++p;
        if (*p == '/' || *p == '\\') {
            errno = EINVAL;
            return -1;
        }
    } else {
        size_t cwd_length = strlen(compat_cwd);
        if (cwd_length >= capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(dest, compat_cwd, cwd_length + 1u);
        out = cwd_length;
    }

    const char *component = p;
    for (;;) {
        char ch = *p;
        int separator = ch == '/' || ch == '\\';

        if (ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
            ch == '<' || ch == '>' || ch == '|' ||
            ((unsigned char)ch < 0x20u && ch != '\0')) {
            errno = EINVAL;
            return -1;
        }

        if (separator || ch == '\0') {
            size_t component_length = (size_t)(p - component);

            if (component_length == 0u && separator) {
                errno = EINVAL;
                return -1;
            }

            if (append_component(
                    dest, &out, capacity, component, component_length) != 0)
                return -1;

            if (ch == '\0')
                break;

            ++p;
            if (*p == '\0')
                break;
            if (*p == '/' || *p == '\\') {
                errno = EINVAL;
                return -1;
            }
            component = p;
            continue;
        }
        ++p;
    }

    if (out == 0u) {
        dest[0] = '/';
        dest[1] = '\0';
    }
    return 0;
}

static unsigned compat_attrs(uint32_t type, uint32_t attributes)
{
    unsigned result = 0u;
    if ((attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY) != 0u)
        result |= FSATTR_RDONLY;
    if ((attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN) != 0u)
        result |= FSATTR_HIDDEN;
    if ((attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_SYSTEM) != 0u)
        result |= FSATTR_SYSTEM;
    if ((attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE) != 0u)
        result |= FSATTR_ARCHIVE;
    if (type == HPCLANG_STREAM_PATH_TYPE_DIRECTORY)
        result |= FSATTR_DIR;
    if (type == HPCLANG_STREAM_PATH_TYPE_VOLUME_LABEL)
        result |= FSATTR_VOLUME;
    return result;
}

static void compat_time(
    FS_FILE *file,
    const struct hpclang_stream_civil_datetime *time,
    uint32_t validity)
{
    file->_hpclang_write_time_valid = 0u;
    memset(&file->_hpclang_write_tm, 0, sizeof(file->_hpclang_write_tm));
    file->WriteTimeDate = 0u;

    if ((validity & HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID) == 0u)
        return;

    file->_hpclang_write_tm.tm_year = (int)time->year - 1900;
    file->_hpclang_write_tm.tm_mon = (int)time->month - 1;
    file->_hpclang_write_tm.tm_mday = time->day;
    file->_hpclang_write_tm.tm_hour = time->hour;
    file->_hpclang_write_tm.tm_min = time->minute;
    file->_hpclang_write_tm.tm_sec = time->second;
    file->_hpclang_write_time_valid = 1u;

    if (time->year >= 1980u && time->year <= 2107u) {
        uint16_t date =
            (uint16_t)(((time->year - 1980u) << 9) |
                       ((uint16_t)time->month << 5) |
                       time->day);
        uint16_t clock =
            (uint16_t)(((uint16_t)time->hour << 11) |
                       ((uint16_t)time->minute << 5) |
                       (time->second / 2u));
        file->WriteTimeDate = ((uint32_t)date << 16) | clock;
    }
}

static int refresh_file_metadata(FS_FILE *file)
{
    if (file == NULL || file->Name == NULL)
        return FS_INVHANDLE;

    struct hpclang_stream_path_info info;
    if (hpclang_stream_path_query(file->Name, &info) != 0)
        return compat_error();

    file->Attr = compat_attrs(info.type, info.attributes);
    file->FileSize =
        info.byte_size > UINT_MAX ? UINT_MAX : (unsigned int)info.byte_size;
    compat_time(file, &info.last_modified, info.time_validity);
    return FS_OK;
}

static FS_FILE *alloc_file(const char *name, unsigned mode)
{
    FS_FILE *file = calloc(1u, sizeof(*file));
    if (file == NULL)
        return NULL;

    file->Name = compat_strdup(name);
    if (file->Name == NULL) {
        free(file);
        return NULL;
    }

    file->Volume = 0u;
    file->Mode = mode;
    file->_hpclang_fd = -1;
    file->_hpclang_owned = 1u;
    return file;
}

static int open_flags(int mode)
{
    int base = mode & (FSMODE_WRITE | FSMODE_APPEND | FSMODE_MODIFY);
    int flags;

    if ((base & FSMODE_APPEND) != 0) {
        flags = O_RDWR | O_APPEND;
    } else if ((base & FSMODE_MODIFY) != 0) {
        flags = O_RDWR;
    } else if ((base & FSMODE_WRITE) != 0) {
        flags = O_RDWR | O_TRUNC;
    } else {
        return O_RDONLY;
    }

    if ((mode & FSMODE_NOCREATE) == 0)
        flags |= O_CREAT;
    return flags;
}

int FSOpen(char *name, int mode, FS_FILE **fileptr)
{
    if (fileptr == NULL)
        return FS_BADNAME;
    *fileptr = NULL;

    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();

    FS_FILE *file = alloc_file(path, (unsigned)mode);
    if (file == NULL)
        return FS_MAXFILES;

    int fd = open(path, open_flags(mode), 0666);
    if (fd < 0) {
        int error = compat_error();
        free(file->Name);
        free(file);
        return error;
    }

    file->_hpclang_kind = HPCLANG_HPGCC_FS_FILE;
    file->_hpclang_fd = fd;
    if (refresh_file_metadata(file) != FS_OK) {
        (void)close(fd);
        free(file->Name);
        free(file);
        return compat_error();
    }

    if ((mode & FSMODE_APPEND) != 0) {
        off_t end = lseek(fd, 0, SEEK_END);
        if (end >= 0)
            file->CurrentOffset = (unsigned int)end;
    }

    *fileptr = file;
    return FS_OK;
}

int FSCreate(char *name, int attr, FS_FILE **fileptr)
{
    if ((attr & (FSATTR_RDONLY | FSATTR_HIDDEN | FSATTR_SYSTEM |
                 FSATTR_DIR | FSATTR_VOLUME)) != 0)
        return FS_CANTWRITE;
    if (fileptr == NULL)
        return FS_BADNAME;
    *fileptr = NULL;

    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();

    FS_FILE *file = alloc_file(path, FSMODE_WRITE);
    if (file == NULL)
        return FS_MAXFILES;

    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        int error = compat_error();
        free(file->Name);
        free(file);
        return error;
    }

    file->_hpclang_kind = HPCLANG_HPGCC_FS_FILE;
    file->_hpclang_fd = fd;
    file->Attr = (unsigned)attr;
    (void)refresh_file_metadata(file);
    *fileptr = file;
    return FS_OK;
}

int FSClose(FS_FILE *file)
{
    if (file == NULL || !file->_hpclang_owned)
        return FS_INVHANDLE;

    int result = 0;
    if (file->_hpclang_kind == HPCLANG_HPGCC_FS_FILE) {
        result = close(file->_hpclang_fd);
    } else if (file->_hpclang_kind == HPCLANG_HPGCC_FS_DIRECTORY) {
        result = hpclang_stream_directory_close(
            (struct hpclang_stream_directory *)file->_hpclang_directory);
    } else {
        return FS_INVHANDLE;
    }

    if (result != 0)
        return compat_error();

    free(file->Name);
    file->Name = NULL;
    free(file);
    return FS_OK;
}

int FSCloseAndDelete(FS_FILE *file)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE ||
        file->Name == NULL)
        return FS_INVHANDLE;

    char path[HPCLANG_STREAM_NAME_CAPACITY];
    size_t length = strlen(file->Name);
    if (length >= sizeof(path))
        return FS_BADNAME;
    memcpy(path, file->Name, length + 1u);

    int closed = FSClose(file);
    if (closed != FS_OK)
        return closed;
    return unlink(path) == 0 ? FS_OK : compat_error();
}

int FSRead(char *buffer, int nbytes, FS_FILE *file)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE ||
        nbytes < 0 || (buffer == NULL && nbytes != 0))
        return 0;

    ssize_t got = read(file->_hpclang_fd, buffer, (size_t)nbytes);
    if (got < 0)
        return 0;

    off_t position = lseek(file->_hpclang_fd, 0, SEEK_CUR);
    if (position >= 0)
        file->CurrentOffset = (unsigned int)position;
    file->_hpclang_eof = got < nbytes;
    return (int)got;
}

int FSWrite(char *buffer, int nbytes, FS_FILE *file)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE ||
        nbytes < 0 || (buffer == NULL && nbytes != 0))
        return 0;
    if ((file->Mode & (FSMODE_WRITE | FSMODE_APPEND | FSMODE_MODIFY)) == 0)
        return 0;
    if ((file->Mode & FSMODE_NOGROW) != 0 &&
        (uint64_t)file->CurrentOffset + (uint64_t)(unsigned)nbytes >
            file->FileSize)
        return 0;

    int base_mode =
        file->Mode & (FSMODE_WRITE | FSMODE_APPEND | FSMODE_MODIFY);

    /*
     * Historical MODIFY preserves the original extent. Cap the request at EOF
     * instead of growing the file. NOGROW is stricter: a request that would
     * grow at all is rejected as a whole.
     */
    if (base_mode == FSMODE_MODIFY) {
        if (file->CurrentOffset >= file->FileSize)
            return 0;
        unsigned int remaining = file->FileSize - file->CurrentOffset;
        if ((unsigned int)nbytes > remaining)
            nbytes = (int)remaining;
    }

    ssize_t written = write(file->_hpclang_fd, buffer, (size_t)nbytes);
    if (written < 0)
        return 0;

    off_t position = lseek(file->_hpclang_fd, 0, SEEK_CUR);
    if (position >= 0)
        file->CurrentOffset = (unsigned int)position;

    if (base_mode == FSMODE_WRITE) {
        /*
         * HPGCC FSMODE_WRITE truncates after every write, not only when the
         * file is opened. This matters after seeking backwards.
         */
        if (ftruncate(file->_hpclang_fd, (off_t)file->CurrentOffset) != 0)
            return 0;
        file->FileSize = file->CurrentOffset;
    } else if (file->CurrentOffset > file->FileSize) {
        file->FileSize = file->CurrentOffset;
    }

    file->_hpclang_eof = 0u;
    return (int)written;
}

int FSSeek(FS_FILE *file, int offset, int position)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE)
        return FS_INVHANDLE;
    if (position != SEEK_SET && position != SEEK_CUR && position != SEEK_END)
        return FS_BADNAME;

    int64_t base;
    if (position == SEEK_SET)
        base = 0;
    else if (position == SEEK_CUR)
        base = file->CurrentOffset;
    else
        base = file->FileSize;

    int64_t target = base + offset;
    if (target < 0)
        target = 0;
    if (target > INT_MAX)
        return FS_BADNAME;

    off_t result = lseek(file->_hpclang_fd, (off_t)target, SEEK_SET);
    if (result < 0)
        return compat_error();

    file->CurrentOffset = (unsigned int)result;
    file->_hpclang_eof = 0u;
    return FS_OK;
}

int FSTell(FS_FILE *file)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE)
        return FS_INVHANDLE;
    if (file->CurrentOffset > INT_MAX)
        return FS_ERROR;
    return (int)file->CurrentOffset;
}

int FSEof(FS_FILE *file)
{
    if (file == NULL || file->_hpclang_kind != HPCLANG_HPGCC_FS_FILE)
        return 1;
    return file->_hpclang_eof != 0u ||
           file->CurrentOffset >= file->FileSize;
}

int FSDelete(char *name)
{
    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();
    return unlink(path) == 0 ? FS_OK : compat_error();
}

int FSRename(char *oldname, char *newname)
{
    char old_path[HPCLANG_STREAM_NAME_CAPACITY];
    char new_path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(oldname, old_path, sizeof(old_path)) != 0 ||
        normalize_path(newname, new_path, sizeof(new_path)) != 0)
        return compat_error();
    return rename(old_path, new_path) == 0 ? FS_OK : compat_error();
}

int FSMkdir(char *name)
{
    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();
    return mkdir(path, 0777) == 0 ? FS_OK : compat_error();
}

int FSRmdir(char *name)
{
    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();
    return rmdir(path) == 0 ? FS_OK : compat_error();
}

int FSChdir(char *name)
{
    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();

    struct hpclang_stream_path_info info;
    if (hpclang_stream_path_query(path, &info) != 0)
        return compat_error();
    if (info.type != HPCLANG_STREAM_PATH_TYPE_DIRECTORY)
        return FS_BADNAME;

    size_t length = strlen(path);
    if (length >= sizeof(compat_cwd))
        return FS_BADNAME;
    memcpy(compat_cwd, path, length + 1u);
    return FS_OK;
}

char *FSGetcwd(int volume)
{
    if (volume != 0)
        return NULL;

    size_t length = strlen(compat_cwd);
    char *result = malloc(length + 1u);
    if (result == NULL)
        return NULL;

    for (size_t i = 0u; i < length; ++i)
        result[i] = compat_cwd[i] == '/' ? '\\' : compat_cwd[i];
    result[length] = '\0';
    return result;
}

int FSOpenDir(char *name, FS_FILE **fileptr)
{
    if (fileptr == NULL)
        return FS_BADNAME;
    *fileptr = NULL;

    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(name, path, sizeof(path)) != 0)
        return compat_error();

    struct hpclang_stream_directory *directory =
        hpclang_stream_directory_open(path);
    if (directory == NULL)
        return compat_error();

    FS_FILE *file = alloc_file(path, FSMODE_READ);
    if (file == NULL) {
        (void)hpclang_stream_directory_close(directory);
        return FS_MAXFILES;
    }

    file->_hpclang_kind = HPCLANG_HPGCC_FS_DIRECTORY;
    file->_hpclang_directory = directory;
    file->Attr = FSATTR_DIR;
    *fileptr = file;
    return FS_OK;
}

int FSGetNextEntry(FS_FILE *entry, FS_FILE *dir)
{
    if (entry == NULL || dir == NULL ||
        dir->_hpclang_kind != HPCLANG_HPGCC_FS_DIRECTORY ||
        dir->_hpclang_directory == NULL)
        return FS_INVHANDLE;

    /*
     * Historical callers commonly pass an uninitialized stack FS_FILE here.
     * A previous successful entry must be released explicitly with
     * FSReleaseEntry(), as required by the original API contract.
     */
    memset(entry, 0, sizeof(*entry));
    entry->_hpclang_fd = -1;

    struct hpclang_stream_directory_entry raw;
    int result = hpclang_stream_directory_read(
        (struct hpclang_stream_directory *)dir->_hpclang_directory,
        &raw);
    if (result < 0)
        return compat_error();
    if (result == 0)
        return FS_EOF;

    memset(entry, 0, sizeof(*entry));
    entry->Name = compat_strdup(raw.name);
    if (entry->Name == NULL)
        return FS_MAXFILES;

    entry->Volume = 0u;
    entry->Mode = FSMODE_READ;
    entry->Attr = compat_attrs(raw.type, raw.attributes);
    entry->FileSize =
        raw.byte_size > UINT_MAX ? UINT_MAX : (unsigned int)raw.byte_size;
    entry->_hpclang_kind = HPCLANG_HPGCC_FS_ENTRY;
    compat_time(entry, &raw.last_modified, raw.time_validity);
    return FS_OK;
}

void FSReleaseEntry(FS_FILE *file)
{
    if (file == NULL)
        return;
    if (file->_hpclang_kind == HPCLANG_HPGCC_FS_ENTRY && file->Name != NULL)
        free(file->Name);
    memset(file, 0, sizeof(*file));
    file->_hpclang_fd = -1;
}

int FSAttr(FS_FILE *file)
{
    if (file == NULL)
        return 0;
    return (int)file->Attr;
}

int FSFileLength(FS_FILE *file)
{
    if (file == NULL)
        return FS_INVHANDLE;
    if (file->FileSize > INT_MAX)
        return FS_ERROR;
    return (int)file->FileSize;
}

void FSGetWriteTime(FS_FILE *file, struct tm *timedate)
{
    if (timedate == NULL)
        return;
    memset(timedate, 0, sizeof(*timedate));
    if (file != NULL && file->_hpclang_write_time_valid)
        *timedate = file->_hpclang_write_tm;
}

static int volume_info(
    int volume,
    struct hpclang_stream_storage_info *info)
{
    if (volume != 0)
        return FS_BADVOLUME;
    if (hpclang_stream_storage_query(info) != 0)
        return compat_error();
    if ((info->flags & HPCLANG_STREAM_STORAGE_MEDIA_PRESENT) == 0u)
        return FS_NOCARD;
    return FS_OK;
}

int FSVolumeMounted(int volume)
{
    struct hpclang_stream_storage_info info;
    return volume_info(volume, &info) == FS_OK;
}

int FSVolumeInserted(int volume)
{
    struct hpclang_stream_storage_info info;
    return volume_info(volume, &info);
}

int FSSetCurrentVolume(int volume)
{
    return volume == 0 ? FS_OK : FS_BADVOLUME;
}

int FSGetCurrentVolume(void)
{
    return 0;
}

int FSGetVolumeSize(int volume)
{
    struct hpclang_stream_storage_info info;
    int result = volume_info(volume, &info);
    if (result != FS_OK)
        return result;
    if ((info.flags & HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID) == 0u ||
        info.capacity_bytes > INT_MAX)
        return FS_ERROR;
    return (int)info.capacity_bytes;
}

int FSGetVolumeFree(int volume)
{
    struct hpclang_stream_storage_info info;
    int result = volume_info(volume, &info);
    if (result != FS_OK)
        return result;
    if ((info.flags & HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID) == 0u ||
        info.free_bytes > INT_MAX)
        return FS_ERROR;
    return (int)info.free_bytes;
}

void FSStripSemi(char *name)
{
    if (name == NULL)
        return;
    size_t length = strlen(name);
    while (length != 0u && name[length - 1u] == ';')
        name[--length] = '\0';
}

static size_t semicolon_trimmed_length(const char *name)
{
    size_t length = strlen(name);
    while (length != 0u && name[length - 1u] == ';')
        --length;
    return length;
}

int FSNameCompare(char *name1, char *name2, int caseflags)
{
    if (name1 == NULL || name2 == NULL)
        return 0;

    size_t len1 = strlen(name1);
    size_t len2 = strlen(name2);
    if (caseflags == FSCASE_SENSHP || caseflags == FSCASE_SENSHPTRUE) {
        len1 = semicolon_trimmed_length(name1);
        len2 = semicolon_trimmed_length(name2);
    }
    if (len1 != len2)
        return 0;

    for (size_t i = 0u; i < len1; ++i) {
        unsigned char a = (unsigned char)name1[i];
        unsigned char b = (unsigned char)name2[i];
        if (caseflags == FSCASE_INSENS) {
            a = (unsigned char)toupper(a);
            b = (unsigned char)toupper(b);
        }
        if (a != b)
            return 0;
    }
    return 1;
}

int FSGetNameType(char *name)
{
    if (name == NULL || name[0] == '\0')
        return FSNAME_INVALID;
    if (strlen(name) > 255u)
        return FSNAME_INVALID;

    const char *p = name;
    int flags = 0;

    if (p[0] == ':' && p[1] != '\0' && p[2] == ':') {
        if (!valid_volume_char(p[1]))
            return FSNAME_INVALID;
        flags |= FSNAME_HASVOL | FSNAME_VOLHP;
        p += 3;
    } else if (p[0] != '\0' && p[1] == ':') {
        if (!valid_volume_char(p[0]))
            return FSNAME_INVALID;
        flags |= FSNAME_HASVOL;
        p += 2;
    }

    if (*p == '/' || *p == '\\')
        flags |= FSNAME_ABSPATH;

    size_t components = 0u;
    size_t component_length = 0u;
    int saw_separator = 0;
    int trailing_separator = 0;

    for (const char *q = p;; ++q) {
        unsigned char ch = (unsigned char)*q;
        int separator = ch == '/' || ch == '\\';

        if (ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
            ch == '<' || ch == '>' || ch == '|' ||
            (ch < 0x20u && ch != 0u))
            return FSNAME_INVALID;

        if (separator || ch == 0u) {
            if (separator)
                flags |= FSNAME_HASPATH;

            if (component_length == 0u) {
                if (separator) {
                    if (q != p)
                        return FSNAME_INVALID;
                }
            } else {
                ++components;
            }

            if (ch == 0u) {
                trailing_separator = q != p &&
                    (q[-1] == '/' || q[-1] == '\\');
                break;
            }

            saw_separator = 1;
            component_length = 0u;
            continue;
        }

        ++component_length;
    }

    if (*p == '\0' || trailing_separator) {
        flags |= FSNAME_EMPTY;
        if (trailing_separator)
            flags |= FSNAME_ENDSLASH;
    }

    size_t subdirectories = components;
    if (!trailing_separator && component_length != 0u && subdirectories != 0u)
        --subdirectories;
    if (!saw_separator)
        subdirectories = 0u;
    if (subdirectories > 0x7fffu)
        return FSNAME_INVALID;

    return flags | ((int)subdirectories << 16);
}

char *FSGetFileName(FS_FILE *file, int pathflags)
{
    if (file == NULL || file->Name == NULL)
        return NULL;

    const char *source = file->Name;
    char relative[HPCLANG_STREAM_NAME_CAPACITY];
    const char *body = source;

    if ((pathflags & FSNAME_HASPATH) == 0 &&
        (pathflags & FSNAME_ABSPATH) == 0) {
        const char *slash = strrchr(source, '/');
        if (slash != NULL)
            body = slash + 1;
    } else if ((pathflags & FSNAME_ABSPATH) == 0) {
        size_t cwd_length = strlen(compat_cwd);
        if (strncmp(source, compat_cwd, cwd_length) == 0 &&
            (compat_cwd[cwd_length - 1u] == '/' ||
             source[cwd_length] == '/' ||
             source[cwd_length] == '\0')) {
            body = source + cwd_length;
            if (*body == '/')
                ++body;
        }
    }

    size_t prefix = 0u;
    if ((pathflags & FSNAME_HASVOL) != 0) {
        const char *volume =
            (pathflags & FSNAME_VOLHP) != 0 ? ":3:" : "C:";
        prefix = strlen(volume);
        memcpy(relative, volume, prefix);
    }

    size_t body_length = strlen(body);
    int need_absolute_slash =
        (pathflags & FSNAME_ABSPATH) != 0 &&
        (body_length == 0u || body[0] != '/');

    if (prefix + (size_t)need_absolute_slash + body_length + 2u >
        sizeof(relative))
        return NULL;

    size_t out = prefix;
    if (need_absolute_slash)
        relative[out++] = '/';
    memcpy(relative + out, body, body_length);
    out += body_length;

    if ((pathflags & FSNAME_ENDSLASH) != 0 &&
        (file->Attr & FSATTR_DIR) != 0 &&
        (out == 0u || relative[out - 1u] != '/'))
        relative[out++] = '/';

    relative[out] = '\0';
    return compat_strdup(relative);
}

char *FSGetErrorMsg(int errornum)
{
    switch (errornum) {
    case FS_OK: return "No error";
    case FS_ERROR: return "Filesystem error";
    case FS_EOF: return "End of file";
    case FS_BADNAME: return "Invalid file name";
    case FS_BADVOLUME: return "Invalid volume";
    case FS_NOTFOUND: return "File not found";
    case FS_CANTWRITE: return "Write failed";
    case FS_NOCARD: return "No card";
    case FS_CHANGED: return "Media changed";
    case FS_MAXFILES: return "Too many open files";
    case FS_USED: return "File or directory in use";
    case FS_DISKFULL: return "Disk full";
    case FS_EXIST: return "File already exists";
    case FS_INVHANDLE: return "Invalid handle";
    default: return "Unknown filesystem error";
    }
}

int fdelete(char *filename)
{
    if (filename == NULL)
        return -1;
    char path[HPCLANG_STREAM_NAME_CAPACITY];
    if (normalize_path(filename, path, sizeof(path)) != 0)
        return -1;
    return remove(path);
}
