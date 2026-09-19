#ifndef HPCLANG_STORAGE_NAMESPACE_H
#define HPCLANG_STORAGE_NAMESPACE_H

#include <stddef.h>
#include <sys/types.h>

#include <hpclang/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

int hpclang_storage_stock_media_query(int *present, int *read_only);
void hpclang_storage_after_stock_reinit(void);

int hpclang_namespace_direct_active(void);
int hpclang_namespace_open(const char *path, int flags);
int hpclang_namespace_close(int fd);
ssize_t hpclang_namespace_read(int fd, void *buffer, size_t count);
ssize_t hpclang_namespace_write(int fd, const void *buffer, size_t count);
off_t hpclang_namespace_lseek(int fd, off_t offset, int whence);
int hpclang_namespace_ftruncate(int fd, off_t length);
int hpclang_namespace_unlink(const char *path);
int hpclang_namespace_rename(const char *old_path, const char *new_path);
int hpclang_namespace_mkdir(const char *path);
int hpclang_namespace_rmdir(const char *path);
int hpclang_namespace_path_query(
    const char *path,
    struct hpclang_stream_path_info *info);
int hpclang_namespace_storage_query(
    struct hpclang_stream_storage_info *info);

void hpclang_storage_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
