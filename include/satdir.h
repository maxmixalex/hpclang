#ifndef HPCLANG_HPGCC_SATDIR_H
#define HPCLANG_HPGCC_SATDIR_H

/*
 * Bounded source-compatibility subset of the historical HPGCC satdir.h API.
 *
 * The adapter preserves the useful C-side directory/navigation semantics on
 * top of hpclang's owned object-store handles and object snapshots. It does
 * not expose Saturn addresses. SAT_OBJ_DSCR.addr is an adapter-private opaque
 * token and must not be passed to sat_peek(), sat_map_s2a(), or other raw
 * Saturn-memory helpers.
 *
 * Names and paths in this legacy API are C strings. Calculator-native names
 * that contain NUL or '/' remain available only through hpclang's raw-byte
 * object-store API because they cannot be represented losslessly here.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _sat_obj_dscr {
    char *name;
    unsigned addr;
} SAT_OBJ_DSCR;

typedef struct _sat_dir_entry {
    SAT_OBJ_DSCR *sat_obj;
    struct _sat_dir_entry *next;
} SAT_DIR_ENTRY;

typedef struct _sat_dir_node {
    char *name;
    struct _sat_dir_node *parent;
    struct _sat_dir_node *sibling;
    struct _sat_dir_node *child;
    SAT_DIR_ENTRY *object;
} SAT_DIR_NODE;

typedef struct _sat_dir_slist {
    char *str;
    struct _sat_dir_slist *next;
} SAT_DIR_SLIST;

void sat_dir_open(void);
void sat_dir_close(void);
char *sat_dir_curdir(void);
int sat_dir_chdir(char *path);
SAT_OBJ_DSCR *sat_dir_getobject(char *path);
SAT_OBJ_DSCR *sat_dir_rclobject(char *path);
int sat_dir_searchobject(char *name, SAT_DIR_SLIST **path_list);
void _sat_dir_destroy_slist(SAT_DIR_SLIST *list);

int sat_obj_is_real(SAT_OBJ_DSCR *obj);
double _sat_dir_fetchreal(char *path, double defval, char *mode);

#define sat_dir_getreal(p, v) _sat_dir_fetchreal((p), (v), "g")
#define sat_dir_rclreal(p, v) _sat_dir_fetchreal((p), (v), "r")

#ifdef __cplusplus
}
#endif

#endif
