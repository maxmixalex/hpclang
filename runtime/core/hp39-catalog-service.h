#ifndef HPCLANG_HP39_CATALOG_SERVICE_H
#define HPCLANG_HP39_CATALOG_SERVICE_H

#include <hpclang/catalog.h>

int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object);
int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object);
int hpclang_hp39_program_body_snapshot(const uint8_t *name, size_t length,
                                       struct hpclang_object **object);
int hpclang_hp39_note_body_snapshot(const uint8_t *name, size_t length,
                                    struct hpclang_object **object);
int hpclang_hp39_catalog_edit(enum hpclang_catalog catalog, const uint8_t *name, uint32_t length,
                              const struct hpclang_object *body);

#endif
