#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/object.h>
#include <hpclang/target.h>
#include <hpclang/variable.h>

int hpclang_hp39_aplet_catalog_snapshot(struct hpclang_object **object);
int hpclang_hp39_aplet_snapshot(const uint8_t *name, size_t name_length,
                                struct hpclang_object **object);
int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object);
int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object);
int hpclang_hp39_variable_schema_snapshot(struct hpclang_object **object);
int hpclang_hp39_program_body_snapshot(const uint8_t *name, size_t name_length,
                                       struct hpclang_object **object);
int hpclang_hp39_note_body_snapshot(const uint8_t *name, size_t name_length,
                                    struct hpclang_object **object);
int hpclang_hp39_catalog_name_count(const struct hpclang_object *catalog, uint32_t *count);
int hpclang_hp39_catalog_contains_name(const struct hpclang_object *catalog, const uint8_t *name,
                                       size_t name_length, int *found);

/* Preserve the original runtime witnesses in the suite that owns HP39
 * catalogs and variables. Exercise Stock behavior and Direct ENOSYS alike. */
uint32_t hpclang_e2e_catalog_variables(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot->model == HPCLANG_MODEL_HP39GPLUS || boot->model == HPCLANG_MODEL_HP39GS ||
        boot->model == HPCLANG_MODEL_HP40GS) {
        errno = 0;
        struct hpclang_object *aplet_catalog = NULL;
        struct hpclang_object *program_catalog = NULL;
        struct hpclang_object *note_catalog = NULL;
        struct hpclang_object *variable_schema = NULL;
        int aplet_result = hpclang_hp39_aplet_catalog_snapshot(&aplet_catalog);
        int program_result =
            aplet_result == 0 ? hpclang_hp39_program_catalog_snapshot(&program_catalog) : -1;
        int note_result =
            program_result == 0 ? hpclang_hp39_note_catalog_snapshot(&note_catalog) : -1;
        int schema_result =
            note_result == 0 ? hpclang_hp39_variable_schema_snapshot(&variable_schema) : -1;
        if (boot->saturn_state != 0u) {
            if (aplet_result != 0 || program_result != 0 || note_result != 0 ||
                schema_result != 0 || aplet_catalog == NULL || program_catalog == NULL ||
                note_catalog == NULL || variable_schema == NULL ||
                hpclang_object_kind(aplet_catalog) != HPCLANG_OBJECT_LIST ||
                hpclang_object_kind(program_catalog) != HPCLANG_OBJECT_LIST ||
                hpclang_object_kind(note_catalog) != HPCLANG_OBJECT_LIST ||
                hpclang_object_kind(variable_schema) != HPCLANG_OBJECT_LIST) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                return 0xb4u;
            }

            static const uint8_t function_aplet_name[] = "Function";
            struct hpclang_object *function_aplet = NULL;
            if (hpclang_hp39_aplet_snapshot(function_aplet_name, sizeof(function_aplet_name) - 1u,
                                            &function_aplet) != 0 ||
                function_aplet == NULL ||
                hpclang_object_kind(function_aplet) != HPCLANG_OBJECT_DIRECTORY) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(function_aplet);
                return 0xbau;
            }
            hpclang_object_release(function_aplet);

            static const uint8_t note_category[] = "Note";
            static const uint8_t note_text_name[] = "NoteText";
            struct hpclang_object *note_text_value = NULL;
            errno = 0;
            int note_text_result =
                hpclang_variable_get(note_category, sizeof(note_category) - 1u, note_text_name,
                                     sizeof(note_text_name) - 1u, &note_text_value);
            if (note_text_result != 0) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                return 0xbfu;
            }
            if (note_text_value == NULL) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                return 0xbdu;
            }
            if (hpclang_object_kind(note_text_value) != HPCLANG_OBJECT_STRING) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                return 0xbeu;
            }

            /*
             * M7.3f first write witness: write the exact owned NoteText value
             * back through its genuine extended-ID descriptor, then read it
             * again. This proves the Stock store dispatcher without changing
             * calculator-visible content or relying on a raw Saturn address.
             */
            size_t note_text_size = 0u;
            if (hpclang_object_string_size(note_text_value, &note_text_size) != 0 ||
                note_text_size > 3000u) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                return 0xd7u;
            }
            if (hpclang_variable_set(note_category, sizeof(note_category) - 1u, note_text_name,
                                     sizeof(note_text_name) - 1u, note_text_value) != 0) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                return 0xd8u;
            }

            struct hpclang_object *note_text_roundtrip = NULL;
            if (hpclang_variable_get(note_category, sizeof(note_category) - 1u, note_text_name,
                                     sizeof(note_text_name) - 1u, &note_text_roundtrip) != 0 ||
                note_text_roundtrip == NULL ||
                hpclang_object_kind(note_text_roundtrip) != HPCLANG_OBJECT_STRING) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                hpclang_object_release(note_text_roundtrip);
                return 0xd9u;
            }

            size_t roundtrip_size = 0u;
            if (hpclang_object_string_size(note_text_roundtrip, &roundtrip_size) != 0 ||
                roundtrip_size != note_text_size) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(note_text_value);
                hpclang_object_release(note_text_roundtrip);
                return 0xdau;
            }

            if (note_text_size != 0u) {
                uint8_t *before = malloc(note_text_size);
                uint8_t *after = malloc(note_text_size);
                if (before == NULL || after == NULL ||
                    hpclang_object_string_copy(note_text_value, before, note_text_size) != 0 ||
                    hpclang_object_string_copy(note_text_roundtrip, after, note_text_size) != 0 ||
                    memcmp(before, after, note_text_size) != 0) {
                    free(after);
                    free(before);
                    hpclang_object_release(aplet_catalog);
                    hpclang_object_release(program_catalog);
                    hpclang_object_release(note_catalog);
                    hpclang_object_release(variable_schema);
                    hpclang_object_release(note_text_value);
                    hpclang_object_release(note_text_roundtrip);
                    return 0xdbu;
                }
                free(after);
                free(before);
            }
            hpclang_object_release(note_text_roundtrip);
            hpclang_object_release(note_text_value);

            uint32_t aplet_count = 0u;
            uint32_t program_count = 0u;
            uint32_t note_count = 0u;
            int fixture_found = 0;
            int editline_found = 0;
            static const uint8_t fixture_name[] = "HPCLANG HPC1 TEST";
            static const uint8_t editline_name[] = "Editline";
            if (hpclang_hp39_catalog_name_count(aplet_catalog, &aplet_count) != 0 ||
                hpclang_hp39_catalog_name_count(program_catalog, &program_count) != 0 ||
                hpclang_hp39_catalog_name_count(note_catalog, &note_count) != 0 ||
                hpclang_hp39_catalog_contains_name(
                    aplet_catalog, fixture_name, sizeof(fixture_name) - 1u, &fixture_found) != 0 ||
                hpclang_hp39_catalog_contains_name(program_catalog, editline_name,
                                                   sizeof(editline_name) - 1u,
                                                   &editline_found) != 0 ||
                fixture_found == 0 || editline_found == 0 || aplet_count == 0u ||
                program_count == 0u) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                return 0xb6u;
            }

            struct hpclang_object *editline_body = NULL;
            size_t editline_size = 0u;
            if (hpclang_hp39_program_body_snapshot(editline_name, sizeof(editline_name) - 1u,
                                                   &editline_body) != 0 ||
                editline_body == NULL ||
                hpclang_object_kind(editline_body) != HPCLANG_OBJECT_STRING ||
                hpclang_object_string_size(editline_body, &editline_size) != 0) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(editline_body);
                return 0xb7u;
            }
            hpclang_object_release(editline_body);

            static const uint8_t missing_body_name[] = "HPCLANG-NO-SUCH";
            struct hpclang_object *missing_program = NULL;
            struct hpclang_object *missing_note = NULL;
            errno = 0;
            int missing_program_result = hpclang_hp39_program_body_snapshot(
                missing_body_name, sizeof(missing_body_name) - 1u, &missing_program);
            int missing_program_errno = errno;
            errno = 0;
            int missing_note_result = hpclang_hp39_note_body_snapshot(
                missing_body_name, sizeof(missing_body_name) - 1u, &missing_note);
            int missing_note_errno = errno;
            if (missing_program_result != -1 || missing_note_result != -1 ||
                missing_program != NULL || missing_note != NULL ||
                missing_program_errno != ENOENT || missing_note_errno != ENOENT) {
                hpclang_object_release(aplet_catalog);
                hpclang_object_release(program_catalog);
                hpclang_object_release(note_catalog);
                hpclang_object_release(variable_schema);
                hpclang_object_release(missing_program);
                hpclang_object_release(missing_note);
                return 0xb8u;
            }
            hpclang_object_release(aplet_catalog);
            hpclang_object_release(program_catalog);
            hpclang_object_release(note_catalog);
            hpclang_object_release(variable_schema);
        } else {
            hpclang_object_release(aplet_catalog);
            hpclang_object_release(program_catalog);
            hpclang_object_release(note_catalog);
            hpclang_object_release(variable_schema);

            static const uint8_t missing_body_name[] = "HPCLANG-NO-SUCH";
            struct hpclang_object *missing_body = NULL;
            struct hpclang_object *direct_aplet = NULL;
            struct hpclang_object *direct_schema = NULL;
            struct hpclang_object *direct_value = NULL;
            errno = 0;
            int missing_result = hpclang_hp39_program_body_snapshot(
                missing_body_name, sizeof(missing_body_name) - 1u, &missing_body);
            int missing_errno = errno;
            errno = 0;
            int direct_aplet_result = hpclang_hp39_aplet_snapshot(
                (const uint8_t *)"Function", sizeof("Function") - 1u, &direct_aplet);
            int aplet_errno = errno;
            errno = 0;
            int direct_schema_result = hpclang_hp39_variable_schema_snapshot(&direct_schema);
            int schema_errno = errno;
            errno = 0;
            int direct_value_result = hpclang_variable_get(
                (const uint8_t *)"Note", sizeof("Note") - 1u, (const uint8_t *)"NoteText",
                sizeof("NoteText") - 1u, &direct_value);
            int value_errno = errno;
            struct hpclang_object *direct_input = NULL;
            if (hpclang_object_create_string(NULL, 0u, &direct_input) != 0)
                return 0xb5u;
            errno = 0;
            int direct_set_result = hpclang_variable_set(
                (const uint8_t *)"Note", sizeof("Note") - 1u, (const uint8_t *)"NoteText",
                sizeof("NoteText") - 1u, direct_input);
            int set_errno = errno;
            hpclang_object_release(direct_input);
            hpclang_object_release(missing_body);
            hpclang_object_release(direct_aplet);
            hpclang_object_release(direct_schema);
            hpclang_object_release(direct_value);
            if (aplet_result != -1 || missing_result != -1 || missing_errno != ENOSYS ||
                direct_aplet_result != -1 || aplet_errno != ENOSYS || direct_schema_result != -1 ||
                schema_errno != ENOSYS || direct_value_result != -1 || value_errno != ENOSYS ||
                direct_set_result != -1 || set_errno != ENOSYS)
                return 0xb5u;
        }
    }
    return 0u;
}
