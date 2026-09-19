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

static int supported_volume_char(char value)
{
    return value == '3' || value == 'C' || value == 'c';
}

static int normalize_path(const char *source, char *dest, size_t capacity)
{
    if (source == NULL || dest == NULL || capacity < 2u) {
        errno = EINVAL;
        return -1;
    }

    const char *p = source;
    if (p[0] == ':' && p[1] != '\0' && p[2] == ':') {
        if (!supported_volume_char(p[1])) {
            errno = ENODEV;
            return -1;
        }
        p += 3;
    } else if (p[0] != '\0' && p[1] == ':') {
        if (!supported_volume_char(p[0])) {
            errno = ENODEV;
            return -1;
        }
        p += 2;
    }

    size_t out = 0u;
    if (*p != '/' && *p != '\\') {
        if (out + 1u >= capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        dest[out++] = '/';
    }

    while (*p != '\0') {
        char ch = *p++;
        if (ch == '\\')
            ch = '/';
        if (out + 1u >= capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        if (ch == '/' && out != 0u && dest[out - 1u] == '/')
            continue;
        dest[out++] = ch;
    }

    if (out == 0u)
        dest[out++] = '/';
    dest[out] = '\0';
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

    ssize_t written = write(file->_hpclang_fd, buffer, (size_t)nbytes);
    if (written < 0)
        return 0;

    off_t position = lseek(file->_hpclang_fd, 0, SEEK_CUR);
    if (position >= 0)
        file->CurrentOffset = (unsigned int)position;
    if (file->CurrentOffset > file->FileSize)
        file->FileSize = file->CurrentOffset;
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

    const char *p = name;
    int flags = 0;

    if (p[0] == ':' && p[1] != '\0' && p[2] == ':') {
        flags |= FSNAME_HASVOL | FSNAME_VOLHP;
        p += 3;
    } else if (p[0] != '\0' && p[1] == ':') {
        flags |= FSNAME_HASVOL;
        p += 2;
    }

    if (*p == '/' || *p == '\\')
        flags |= FSNAME_ABSPATH;

    int components = 0;
    int saw_separator = 0;
    for (const char *q = p; *q != '\0'; ++q) {
        if (*q == ':')
            return FSNAME_INVALID;
        if (*q == '/' || *q == '\\') {
            flags |= FSNAME_HASPATH;
            saw_separator = 1;
            if (q[1] != '\0')
                ++components;
        }
    }

    size_t length = strlen(p);
    if (length == 0u ||
        (length != 0u && (p[length - 1u] == '/' || p[length - 1u] == '\\')))
        flags |= FSNAME_EMPTY | FSNAME_ENDSLASH;
    else if (saw_separator && components > 0)
        --components;

    return flags | (components << 16);
}

char *FSGetFileName(FS_FILE *file, int pathflags)
{
    if (file == NULL || file->Name == NULL)
        return NULL;

    const char *source = file->Name;
    if ((pathflags & (FSNAME_HASPATH | FSNAME_ABSPATH)) == 0) {
        const char *slash = strrchr(source, '/');
        const char *backslash = strrchr(source, '\\');
        const char *base = slash;
        if (base == NULL || (backslash != NULL && backslash > base))
            base = backslash;
        if (base != NULL)
            source = base + 1;
    }
    return compat_strdup(source);
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
