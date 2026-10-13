#ifndef HPCLANG_NOTEBOOK_STORE_H
#define HPCLANG_NOTEBOOK_STORE_H

#include <stddef.h>

#define NOTEBOOK_CAPACITY 192u

/* Printable single-line UTF-8 is stored as bytes inside a native string object. These helpers
 * never change the caller's buffer on failure. ENOENT means no saved note;
 * EILSEQ means existing data does not belong to this example. */
int notebook_load(char text[NOTEBOOK_CAPACITY]);
int notebook_save(const char *text);

#endif
