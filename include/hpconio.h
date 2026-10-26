#ifndef HPCLANG_HPGCC_HPCONIO_H
#define HPCLANG_HPGCC_HPCONIO_H
#include <stdio.h>
#include <hpstring.h>
#include <hpclang/hpgcc-format.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Including this compatibility header selects lazy calculator-byte output for
     * unbound standard streams. Explicit hpclang_stdio_bind owners take priority.
     * HPG's mode/buffering policy is shared; its double-buffered modes require flip.
     * getchar blocks for a model-native text key without echo: Enter returns LF,
     * deletion returns BS, ON reports EOF/ferror with ECANCELED. Alpha/Shift state
     * persists across reads; physical event timings/queue are not reset.
     * Legacy gets/_gets calls use compiler-known bounds below. Legacy numeric
     * formatting requires HPCLANG_HPGCC_LEGACY_FORMAT=1 before inclusion.
     * hpstdio.h provides blocking readint/readdouble adapters. */
    int hpclang_hpgcc_console_enable(void);
    /* Edited calculator-byte line, capacity including NUL; initially empty.
     * Echo/BS correction, CR/LF acceptance; excess input is ignored until edited.
     * Returns buffer, or NULL on empty EOF/error/cancel. Partial text remains
     * terminated. Invalid/unknown size: EINVAL before consuming input. Uses the
     * selected standard streams; default-console editing owns a temporary image
     * for scroll undo and publishes each edit even in HPG double-buffer mode. */
    char *hpclang_hpgcc_gets(char *buffer, size_t capacity);
    void home(void);
    int getx(void);
    int gety(void);
    void gotoxy(int column, int row);
    void clreol(void);
    void _clear_screen(void);
    int screen_width(void);
    int screen_height(void);
    int get_tabsize(void);
    void set_tabsize(int width);
    void beep(void);
#ifdef __cplusplus
}
#endif
/* Historical calls lack a capacity argument. Fixed char arrays use their own
 * extent, including struct members. Pointer expressions have no declared array
 * extent and fail with EINVAL before input; pass an explicit capacity to
 * hpclang_hpgcc_gets for pointers or dynamic buffers. The buffer is evaluated
 * once. These call macros do not provide addressable legacy function symbols. */
#ifndef HPCLANG_HPCONIO_NO_GETS_ALIASES
#ifdef __cplusplus
template <typename T> struct hpclang_hpgcc_line_capacity
{
    static const size_t value = 0;
};
template <size_t N> struct hpclang_hpgcc_line_capacity<char (&)[N]>
{
    static const size_t value = N;
};
template <typename T> static inline char *hpclang_hpgcc_gets_array(T &&buffer)
{
    return hpclang_hpgcc_gets(buffer, hpclang_hpgcc_line_capacity<T>::value);
}
#define gets(buffer) hpclang_hpgcc_gets_array((buffer))
#elif defined(__clang__) || defined(__GNUC__)
#define gets(buffer)                                                                               \
    hpclang_hpgcc_gets(                                                                            \
        (buffer), __builtin_constant_p(sizeof(buffer)) &&                                          \
                          __builtin_types_compatible_p(__typeof__(buffer), char[sizeof(buffer)])   \
                      ? sizeof(buffer)                                                             \
                      : 0)
#endif
#define _gets(buffer) gets(buffer)
#endif
#ifndef HPCLANG_HPCONIO_NO_AUTOINIT
static void __attribute__((constructor)) hpclang_hpgcc_console_autoinit(void)
{
    (void)hpclang_hpgcc_console_enable();
}
#endif
#endif
