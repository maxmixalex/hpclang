#ifndef HPCLANG_STORAGE_NAMESPACE_H
#define HPCLANG_STORAGE_NAMESPACE_H

#include <hpclang/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

int hpclang_storage_stock_idle(void);
void hpclang_storage_after_stock_reinit(void);

int hpclang_namespace_direct_active(void);
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
