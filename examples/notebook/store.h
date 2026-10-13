#ifndef HPCLANG_NOTEBOOK_STORE_H
#define HPCLANG_NOTEBOOK_STORE_H

#include <stddef.h>

#define NOTEBOOK_CAPACITY 192u

/* Printable single-line UTF-8 is stored as bytes inside a native string object. These helpers
 * never change the caller's buffer on failure. ENOENT means no saved note;
 * EILSEQ means existing data does not belong to this example. */
int notebook_load(char text[NOTEBOOK_CAPACITY]);
int notebook_save(const char *text);
int notebook_backup_load(unsigned port, char text[NOTEBOOK_CAPACITY]);
int notebook_backup_save(unsigned port, const char *text);
int notebook_text_size(const char *text, size_t *length);
int notebook_export_file(const char *path, const char *text);

#endif
