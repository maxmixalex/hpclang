#ifndef HPCLANG_HPGCC_SATURN_H
#define HPCLANG_HPGCC_SATURN_H

/*
 * Bounded source-compatibility subset of the historical HPGCC saturn.h API.
 *
 * hpclang deliberately exposes only operations that can be represented through
 * the owned object/stack service without returning borrowed Saturn addresses or
 * raw emulator-memory pointers. Low-level sat_peek/sat_poke/sat_map_s2a,
 * address-returning stack descriptors, TEMPOB allocation and pointer pushes are
 * Migration/Internal rather than part of this compatibility header.
 */

#ifndef BOOL
#define BOOL int
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifndef HPCLANG_HPGCC_LONG_TYPES_DEFINED
#define HPCLANG_HPGCC_LONG_TYPES_DEFINED
typedef unsigned long long ULONGLONG;
typedef long long LONGLONG;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Historical HPGCC temporarily removed a launcher-bias cell here. hpclang's
     * canonical stack service already normalizes that launcher detail, so init/exit
     * are a source-compatible scope pair and never expose or mutate DSKTOP.
     */
    unsigned sat_stack_init(void);
    void sat_stack_exit(unsigned rpl_stack_bias);

    int sat_stack_depth(void);
    int sat_stack_drop(void);
    int sat_stack_dropn(int n);
    int sat_stack_swap(void);
    int sat_stack_nip(void);

    /* HPGCC 2.0 treats any nonzero argument as TRUE. Errors set errno. */
    void sat_pushtruefalse(BOOL arg);
#define sat_stack_push_bool sat_pushtruefalse

    int sat_stack_push_string(const char *str);
    char *sat_pick_str(int level, char *buf);
    char *sat_pick_str_alloc(int level);
    char *sat_pop_str(char *buf);
    char *sat_pop_str_alloc(void);

#define sat_stack_pop_string(s) sat_pop_str((s))
#define sat_stack_pop_string_alloc sat_pop_str_alloc
#define sat_stack_pick_string(l, b) sat_pick_str((l), (b))
#define sat_stack_pick_string_alloc(l) sat_pick_str_alloc((l))

    /*
     * Build the historical argc/argv view from String objects currently on the
     * calculator stack. The output array contains exactly argc entries and has no
     * trailing NULL, matching HPGCC 2.0. `_sat_...` optionally supplies argv[0].
     */
    int sat_stack_pop_stdargs(char ***argv);
    int _sat_stack_pop_stdargs(const char *arg0, char ***argv);

    /*
     * Historical HPGCC names these as numeric pushes, but their source contract is
     * textual: format the C value, then push a calculator String. They therefore do
     * not create native Integer/REAL objects or expose Saturn representation.
     */
    int _sat_stack_push_int(int z, int radix, int binary);
    int sat_stack_push_int(int z);
    int sat_stack_push_double(double d);

    /* Exact ZINT exchange used by the historical stack examples. */
    BOOL sat_push_zint_llong(LONGLONG num);
    LONGLONG sat_pop_zint_llong(void);

    /* Numeric classic REAL exchange without exposing its native BCD layout. */
    BOOL sat_push_real(double number);
    double sat_pop_real(void);

    BOOL sat_push_char(char byte);

#ifdef __cplusplus
}
#endif

#endif
