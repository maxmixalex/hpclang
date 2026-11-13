/* Route the adapter's POSIX calls into the actual namespace/FatFs fixture.
 * Standard declarations are included before substitution so the host libc
 * itself is unaffected. No adapter algorithm is replaced. */
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "storage-namespace.h"

static int adapter_open(const char *path, int flags, ...)
{
    return hpclang_namespace_open(path, flags);
}

static int adapter_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return hpclang_namespace_mkdir(path);
}

#define open adapter_open
#define close hpclang_namespace_close
#define read hpclang_namespace_read
#define write hpclang_namespace_write
#define lseek hpclang_namespace_lseek
#define ftruncate hpclang_namespace_ftruncate
#define unlink hpclang_namespace_unlink
#define rename hpclang_namespace_rename
#define mkdir adapter_mkdir
#define rmdir hpclang_namespace_rmdir
#define hpclang_stream_path_query hpclang_namespace_path_query
#define hpclang_stream_path_set_attributes hpclang_namespace_path_set_attributes
#include "../../runtime/libc/hpgcc-fsystem.c"
