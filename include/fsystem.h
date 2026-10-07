#ifndef HPCLANG_HPGCC_FSYSTEM_H
#define HPCLANG_HPGCC_FSYSTEM_H

/*
 * Source-compatibility subset of the historical HPGCC fsystem.h API.
 *
 * This is an independently implemented adapter over hpclang's clean storage
 * service. It does not expose historical SD-controller/FAT internals or binary
 * compatibility. Only semantics that can be represented honestly by the
 * hpclang single-SD-volume namespace are declared here.
 */

#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FSATTR_RDONLY 1
#define FSATTR_HIDDEN 2
#define FSATTR_SYSTEM 4
#define FSATTR_VOLUME 8
#define FSATTR_DIR 16
#define FSATTR_ARCHIVE 32
#define FSATTR_LONGNAME 0x0f
#define FSATTR_LONGMASK 0x3f

#define FSMODE_READ 0
#define FSMODE_WRITE 2
#define FSMODE_APPEND 4
#define FSMODE_MODIFY 8
#define FSMODE_NOGROW 16
#define FSMODE_NOCREATE 32

#define FS_OK 1
#define FS_ERROR 0
#define FS_EOF -1
#define FS_BADNAME -2
#define FS_BADVOLUME -3
#define FS_NOTFOUND -4
#define FS_CANTWRITE -5
#define FS_NOCARD -6
#define FS_CHANGED -7
#define FS_MAXFILES -8
#define FS_OPENDIR -9
#define FS_OPENFILE -9
#define FS_USED -9
#define FS_DISKFULL -10
#define FS_EXIST -11
#define FS_INVHANDLE -12

#define FSCASE_SENS 0
#define FSCASE_SENSHP 1
#define FSCASE_INSENS 2
#define FSCASE_SENSHPTRUE 3

#define FSNAME_HASVOL 1
#define FSNAME_HASPATH 2
#define FSNAME_ABSPATH 4
#define FSNAME_ENDSLASH 8
#define FSNAME_VOLHP 16
#define FSNAME_EMPTY 32
#define FSNAME_INVALID -1

    /*
     * The historical structure exposed many implementation fields. Keep the
     * application-visible metadata fields commonly used by source programs, then
     * append hpclang-private adapter state. Source compatibility is intended; the
     * historical binary layout is deliberately not reproduced.
     */
    typedef struct hpclang_hpgcc_fs_file
    {
        char *Name;
        unsigned Volume;
        unsigned Mode;
        unsigned Attr;
        unsigned NTRes;
        unsigned CrtTmTenth;
        unsigned LastAccDate;
        unsigned int CreatTimeDate;
        unsigned int WriteTimeDate;
        int FirstCluster;
        unsigned int FileSize;
        unsigned int CurrentOffset;
        int DirEntryOffset;
        int DirEntryNum;
        struct hpclang_hpgcc_fs_file *Dir;

        int _hpclang_kind;
        int _hpclang_fd;
        void *_hpclang_directory;
        unsigned _hpclang_eof;
        unsigned _hpclang_owned;
        struct tm _hpclang_write_tm;
        unsigned _hpclang_write_time_valid;
        char *_hpclang_entry_path;
        struct tm _hpclang_created_tm;
        struct tm _hpclang_accessed_tm;
        unsigned _hpclang_time_validity;
    } FS_FILE;

    int FSVolumeMounted(int volume);
    int FSVolumeInserted(int volume);
    int FSSetCurrentVolume(int volume);
    int FSGetCurrentVolume(void);
    int FSGetVolumeSize(int volume);
    int FSGetVolumeFree(int volume);

    int FSMkdir(char *name);
    int FSRmdir(char *name);
    int FSChdir(char *name);
    char *FSGetcwd(int volume);
    int FSOpenDir(char *name, FS_FILE **fileptr);
    int FSGetNextEntry(FS_FILE *entry, FS_FILE *dir);
    void FSReleaseEntry(FS_FILE *file);

    int FSCreate(char *name, int attr, FS_FILE **fileptr);
    int FSOpen(char *name, int mode, FS_FILE **fileptr);
    int FSClose(FS_FILE *file);
    int FSCloseAndDelete(FS_FILE *file);
    int FSSeek(FS_FILE *file, int offset, int position);
    int FSTell(FS_FILE *file);
    int FSRead(char *buffer, int nbytes, FS_FILE *file);
    int FSWrite(char *buffer, int nbytes, FS_FILE *file);
    int FSEof(FS_FILE *file);
    int FSDelete(char *name);
    int FSRename(char *oldname, char *newname);

    int FSNameCompare(char *name1, char *name2, int caseflags);
    int FSGetNameType(char *name);
    void FSStripSemi(char *name);
    char *FSGetErrorMsg(int errornum);
    int FSAttr(FS_FILE *file);
    int FSChAttr(FS_FILE *file, int newattr);
    int FSFileLength(FS_FILE *file);
    void FSGetWriteTime(FS_FILE *file, struct tm *timedate);
    void FSGetCreatTime(FS_FILE *file, struct tm *timedate);
    void FSGetAccessDate(FS_FILE *file, struct tm *timedate);
    char *FSGetFileName(FS_FILE *file, int pathflags);

#ifdef __cplusplus
}
#endif

#endif
