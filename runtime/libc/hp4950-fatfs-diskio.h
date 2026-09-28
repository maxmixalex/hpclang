#ifndef HPCLANG_HP4950_FATFS_DISKIO_H
#define HPCLANG_HP4950_FATFS_DISKIO_H

#ifdef __cplusplus
extern "C"
{
#endif

    int hpclang_hp4950_fatfs_controller_acquire(int read_only);
    void hpclang_hp4950_fatfs_controller_release(void);
    int hpclang_hp4950_fatfs_controller_owned(void);

#ifdef __cplusplus
}
#endif

#endif
