#ifndef HPCLANG_HPGCC_KOS_H
#define HPCLANG_HPGCC_KOS_H

/* Supported source-compatible KOS sound subset; no raw syscall ABI. */
typedef unsigned int U32;
typedef int S32;
typedef unsigned int TIME_T;
typedef int BOOL;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    /* Hz (1..15000), milliseconds (nonzero). Bit 0 allows a later KOS request to
     * replace this tone; bit 1 waits for completion. Other bits do not affect these
     * policies. A nonreplaceable active KOS tone rejects the new request but still
     * returns 0, and waits according to the OLD tone's mode, like the ROM wrapper.
     * Invalid arguments return the historical -51 (also errno=EINVAL). Runtime
     * resource/clock failures return -1 with errno. A canonical async tone is not
     * implicitly replaced: EBUSY. Same foreground/IRQ constraints as sound_start.
     * Cancel via hpclang_sound_stop; exit/_Exit also clean up. */
    S32 kos_beep(U32 frequency_hz, TIME_T duration_ms, U32 mode);
    /* FALSE only for an active KOS tone whose waiting bit is set. TRUE does not
     * necessarily mean silence. Errors collect the lease and return TRUE, errno
     * set; use hpclang_sound_status for an unambiguous runtime failure result. */
    BOOL kos_CheckBeepEnd(void);
#ifdef __cplusplus
}
#endif
#endif
