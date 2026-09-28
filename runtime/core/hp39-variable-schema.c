#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "hp39-variable-schema.h"
#include "object-snapshot.h"

enum
{
    RPL_DOLIST = 0x02a74u,
    RPL_DOCSTR = 0x02a2cu,
    RPL_DOIDNT = 0x02e48u,
    RPL_SEMI = 0x0312bu,
    HP39_SATURN_LIMIT = 0x00100000u,
    HP39_VARIABLE_DESCRIPTOR_NIBBLES = 15u,
    HP39_VARIABLE_ID_DESCRIPTOR_BYTES = 3u
};

struct snapshot_reader_context
{
    const struct hpclang_object *object;
};

static int snapshot_read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct snapshot_reader_context *snapshot = (struct snapshot_reader_context *)context;
    return snapshot != NULL &&
           hpclang_object_snapshot_read_nibble(snapshot->object, address, value);
}

static struct hpclang_rpl_reader snapshot_reader(struct snapshot_reader_context *context,
                                                 const struct hpclang_object *object)
{
    context->object = object;
    return (struct hpclang_rpl_reader){
        .read_nibble = snapshot_read_nibble,
        .context = context,
    };
}

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t nibbles,
                     uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL || nibbles == 0u ||
        nibbles > 8u)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) || nibble > 0x0fu)
            return 0;
        out |= (uint32_t)nibble << (4u * i);
    }
    *value = out;
    return 1;
}

static int read_byte(const struct hpclang_rpl_reader *reader, uint32_t address, uint8_t *value)
{
    uint32_t word = 0u;
    if (value == NULL || !read_word(reader, address, 2u, &word))
        return 0;
    *value = (uint8_t)word;
    return 1;
}

static int bytes_match(const struct hpclang_rpl_reader *reader, uint32_t offset,
                       size_t stored_length, const uint8_t *expected, size_t expected_length)
{
    if (stored_length != expected_length)
        return 0;

    for (size_t i = 0u; i < expected_length; ++i) {
        if (i > (UINT32_MAX - offset) / 2u)
            return -1;
        uint8_t byte = 0u;
        if (!read_byte(reader, offset + (uint32_t)(i * 2u), &byte))
            return -1;
        if (byte != expected[i])
            return 0;
    }
    return 1;
}

static int object_span(const struct hpclang_rpl_reader *reader, uint32_t offset, uint32_t limit,
                       uint32_t *span)
{
    if (offset > limit) {
        errno = EILSEQ;
        return -1;
    }
    int result = hpclang_rpl_object_span(reader, offset, limit - offset, span);
    if (result == HPCLANG_RPL_SPAN_OK && *span != 0u)
        return 0;

    errno = result == HPCLANG_RPL_SPAN_UNAVAILABLE   ? EIO
            : result == HPCLANG_RPL_SPAN_UNSUPPORTED ? ENOTSUP
                                                     : EILSEQ;
    return -1;
}

static int parse_category_string(const struct hpclang_rpl_reader *reader, uint32_t offset,
                                 uint32_t limit, uint32_t *span, uint32_t *bytes_offset,
                                 size_t *bytes_length)
{
    uint32_t prologue = 0u;
    uint32_t length = 0u;
    if (!read_word(reader, offset, 5u, &prologue) || prologue != RPL_DOCSTR ||
        !read_word(reader, offset + 5u, 5u, &length) || length < 5u || ((length - 5u) & 1u) != 0u ||
        length > UINT32_MAX - 5u || offset > limit || limit - offset < 5u + length) {
        errno = EILSEQ;
        return -1;
    }

    *span = 5u + length;
    *bytes_offset = offset + 10u;
    *bytes_length = (size_t)((length - 5u) / 2u);
    return 0;
}

static int parse_extended_id(const struct hpclang_rpl_reader *reader, uint32_t offset,
                             uint32_t limit, struct hpclang_hp39_variable_schema_match *view,
                             uint32_t *span_out, uint32_t *name_offset_out)
{
    uint32_t prologue = 0u;
    uint32_t payload_bytes = 0u;
    if (!read_word(reader, offset, 5u, &prologue) || prologue != RPL_DOIDNT ||
        !read_word(reader, offset + 5u, 2u, &payload_bytes) ||
        payload_bytes <= HP39_VARIABLE_ID_DESCRIPTOR_BYTES) {
        errno = EILSEQ;
        return -1;
    }

    if (payload_bytes > (UINT32_MAX - 7u) / 2u) {
        errno = EOVERFLOW;
        return -1;
    }
    uint32_t span = 7u + payload_bytes * 2u;
    if (offset > limit || limit - offset < span) {
        errno = EILSEQ;
        return -1;
    }

    size_t visible_length = (size_t)(payload_bytes - HP39_VARIABLE_ID_DESCRIPTOR_BYTES);
    uint32_t descriptor_offset = offset + 7u + (uint32_t)(visible_length * 2u);

    uint8_t descriptor_bytes[3] = {0u, 0u, 0u};
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (!read_byte(reader, descriptor_offset + i * 2u, &descriptor_bytes[i])) {
            errno = EILSEQ;
            return -1;
        }
    }

    uint32_t descriptor_address = (uint32_t)descriptor_bytes[0] |
                                  ((uint32_t)descriptor_bytes[1] << 8u) |
                                  ((uint32_t)descriptor_bytes[2] << 16u);
    if (descriptor_address == 0u ||
        descriptor_address > HP39_SATURN_LIMIT - HP39_VARIABLE_DESCRIPTOR_NIBBLES) {
        errno = EILSEQ;
        return -1;
    }

    view->visible_name_length = visible_length;
    view->descriptor_address = descriptor_address;
    if (span_out != NULL)
        *span_out = span;
    if (name_offset_out != NULL)
        *name_offset_out = offset + 7u;
    return 0;
}

static int resolve_reference(hpclang_hp39_variable_reference_resolver resolver,
                             void *resolver_context, uint32_t reference, uint32_t expected_prologue,
                             struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;
    if (resolver == NULL) {
        errno = ENOTSUP;
        return -1;
    }
    if (resolver(resolver_context, reference, expected_prologue, object) != 0)
        return -1;
    if (*object == NULL || (*object)->native_prologue != expected_prologue) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int find_in_variable_list_reader(const struct hpclang_rpl_reader *reader,
                                        uint32_t list_offset, uint32_t list_span,
                                        const uint8_t *name, size_t name_length,
                                        hpclang_hp39_variable_reference_resolver resolver,
                                        void *resolver_context,
                                        struct hpclang_hp39_variable_schema_match *match)
{
    uint32_t end = list_offset + list_span;
    uint32_t cursor = list_offset + 5u;

    while (cursor < end) {
        uint32_t word = 0u;
        if (!read_word(reader, cursor, 5u, &word)) {
            errno = EILSEQ;
            return -1;
        }
        if (word == RPL_SEMI) {
            if (cursor + 5u != end) {
                errno = EILSEQ;
                return -1;
            }
            errno = ENOENT;
            return -1;
        }

        struct hpclang_hp39_variable_schema_match candidate = {0u};
        uint32_t span = 0u;
        uint32_t name_offset = 0u;

        if (word == RPL_DOIDNT) {
            if (parse_extended_id(reader, cursor, end, &candidate, &span, &name_offset) != 0)
                return -1;

            int same =
                bytes_match(reader, name_offset, candidate.visible_name_length, name, name_length);
            if (same < 0) {
                errno = EILSEQ;
                return -1;
            }
            if (same != 0) {
                *match = candidate;
                return 0;
            }
        } else {
            struct hpclang_object *id = NULL;
            if (resolve_reference(resolver, resolver_context, word, RPL_DOIDNT, &id) != 0)
                return -1;

            struct snapshot_reader_context id_context;
            struct hpclang_rpl_reader id_reader = snapshot_reader(&id_context, id);
            uint32_t id_name_offset = 0u;
            if (parse_extended_id(&id_reader, 0u, id->nibble_count, &candidate, NULL,
                                  &id_name_offset) != 0) {
                hpclang_object_release(id);
                return -1;
            }

            int same = bytes_match(&id_reader, id_name_offset, candidate.visible_name_length, name,
                                   name_length);
            hpclang_object_release(id);
            if (same < 0) {
                errno = EILSEQ;
                return -1;
            }
            if (same != 0) {
                /*
                 * Preserve the live Stock object reference. The resolved
                 * snapshot proves the object shape, but value recall should
                 * execute the calculator's original extended ID instead of a
                 * byte-identical reconstruction in hpclang-owned memory.
                 */
                candidate.source_reference = word;
                *match = candidate;
                return 0;
            }
            span = 5u;
        }

        if (span == 0u || span > end - cursor) {
            errno = EILSEQ;
            return -1;
        }
        cursor += span;
    }

    errno = EILSEQ;
    return -1;
}

static int find_in_variable_list_object(const struct hpclang_object *list, const uint8_t *name,
                                        size_t name_length,
                                        hpclang_hp39_variable_reference_resolver resolver,
                                        void *resolver_context,
                                        struct hpclang_hp39_variable_schema_match *match)
{
    if (list == NULL || list->native_prologue != RPL_DOLIST || list->kind != HPCLANG_OBJECT_LIST) {
        errno = EILSEQ;
        return -1;
    }
    struct snapshot_reader_context context;
    struct hpclang_rpl_reader reader = snapshot_reader(&context, list);
    return find_in_variable_list_reader(&reader, 0u, list->nibble_count, name, name_length,
                                        resolver, resolver_context, match);
}

int hpclang_hp39_variable_schema_find_resolved(const struct hpclang_object *schema,
                                               const uint8_t *category, size_t category_length,
                                               const uint8_t *name, size_t name_length,
                                               hpclang_hp39_variable_reference_resolver resolver,
                                               void *resolver_context,
                                               struct hpclang_hp39_variable_schema_match *match)
{
    if (schema == NULL || match == NULL || category == NULL || category_length == 0u ||
        name == NULL || name_length == 0u || schema->kind != HPCLANG_OBJECT_LIST ||
        schema->native_prologue != RPL_DOLIST || schema->nibble_count < 10u) {
        errno = EINVAL;
        return -1;
    }

    *match = (struct hpclang_hp39_variable_schema_match){0u};

    struct snapshot_reader_context context;
    struct hpclang_rpl_reader reader = snapshot_reader(&context, schema);

    uint32_t cursor = 5u;
    while (cursor < schema->nibble_count) {
        uint32_t word = 0u;
        if (!read_word(&reader, cursor, 5u, &word)) {
            errno = EILSEQ;
            return -1;
        }
        if (word == RPL_SEMI) {
            if (cursor + 5u != schema->nibble_count) {
                errno = EILSEQ;
                return -1;
            }
            errno = ENOENT;
            return -1;
        }

        int same_category = 0;
        uint32_t category_span = 0u;
        if (word == RPL_DOCSTR) {
            uint32_t category_bytes = 0u;
            size_t stored_category_length = 0u;
            if (parse_category_string(&reader, cursor, schema->nibble_count, &category_span,
                                      &category_bytes, &stored_category_length) != 0)
                return -1;
            same_category = bytes_match(&reader, category_bytes, stored_category_length, category,
                                        category_length);
            if (same_category < 0) {
                errno = EILSEQ;
                return -1;
            }
        } else {
            struct hpclang_object *category_object = NULL;
            if (resolve_reference(resolver, resolver_context, word, RPL_DOCSTR, &category_object) !=
                0)
                return -1;

            struct snapshot_reader_context category_context;
            struct hpclang_rpl_reader category_reader =
                snapshot_reader(&category_context, category_object);
            uint32_t ignored_span = 0u;
            uint32_t category_bytes = 0u;
            size_t stored_category_length = 0u;
            if (parse_category_string(&category_reader, 0u, category_object->nibble_count,
                                      &ignored_span, &category_bytes,
                                      &stored_category_length) != 0) {
                hpclang_object_release(category_object);
                return -1;
            }
            same_category = bytes_match(&category_reader, category_bytes, stored_category_length,
                                        category, category_length);
            hpclang_object_release(category_object);
            if (same_category < 0) {
                errno = EILSEQ;
                return -1;
            }
            category_span = 5u;
        }

        cursor += category_span;
        if (cursor >= schema->nibble_count) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t list_word = 0u;
        if (!read_word(&reader, cursor, 5u, &list_word)) {
            errno = EILSEQ;
            return -1;
        }

        if (list_word == RPL_DOLIST) {
            uint32_t list_span = 0u;
            if (object_span(&reader, cursor, schema->nibble_count, &list_span) != 0)
                return -1;
            if (same_category != 0)
                return find_in_variable_list_reader(&reader, cursor, list_span, name, name_length,
                                                    resolver, resolver_context, match);
            cursor += list_span;
        } else {
            if (same_category != 0) {
                struct hpclang_object *list = NULL;
                if (resolve_reference(resolver, resolver_context, list_word, RPL_DOLIST, &list) !=
                    0)
                    return -1;
                int result = find_in_variable_list_object(list, name, name_length, resolver,
                                                          resolver_context, match);
                hpclang_object_release(list);
                return result;
            }
            cursor += 5u;
        }
    }

    errno = EILSEQ;
    return -1;
}

int hpclang_hp39_variable_schema_find(const struct hpclang_object *schema, const uint8_t *category,
                                      size_t category_length, const uint8_t *name,
                                      size_t name_length,
                                      struct hpclang_hp39_variable_schema_match *match)
{
    return hpclang_hp39_variable_schema_find_resolved(schema, category, category_length, name,
                                                      name_length, NULL, NULL, match);
}

int hpclang_hp39_variable_descriptor_read(const struct hpclang_rpl_reader *reader, uint32_t address,
                                          struct hpclang_hp39_variable_descriptor *descriptor)
{
    if (reader == NULL || reader->read_nibble == NULL || descriptor == NULL || address == 0u ||
        address > HP39_SATURN_LIMIT - HP39_VARIABLE_DESCRIPTOR_NIBBLES) {
        errno = EINVAL;
        return -1;
    }

    struct hpclang_hp39_variable_descriptor out = {
        .address = address,
    };
    if (!read_word(reader, address, 5u, &out.recall_word) ||
        !read_word(reader, address + 5u, 5u, &out.type_predicate_word) ||
        !read_word(reader, address + 10u, 5u, &out.store_word)) {
        errno = EIO;
        return -1;
    }

    if (out.recall_word == 0u || out.type_predicate_word == 0u || out.store_word == 0u) {
        errno = EILSEQ;
        return -1;
    }

    *descriptor = out;
    return 0;
}
