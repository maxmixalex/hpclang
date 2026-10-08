#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <hpclang/object-store.h>

extern volatile uint32_t hpclang_e2e_result;

static int object_store_find_named(struct hpclang_object_store_iterator *iterator,
                                   const uint8_t *name, uint32_t name_length,
                                   struct hpclang_object_store_entry *found)
{
    struct hpclang_object_store_entry entry = {
        .size = sizeof(struct hpclang_object_store_entry),
        .version = HPCLANG_OBJECT_STORE_ENTRY_VERSION,
    };

    int result = 0;
    while ((result = hpclang_object_store_iterator_read(iterator, &entry)) > 0) {
        if (entry.name_length == name_length && memcmp(entry.name, name, name_length) == 0) {
            if (found != NULL)
                *found = entry;
            return 1;
        }
    }
    return result;
}

#define HPCLANG_E2E_OBJECT_STORE_PHASE UINT32_C(0x4f530000) /* OS + step */

static void object_store_phase(uint32_t step)
{
    hpclang_e2e_result = HPCLANG_E2E_OBJECT_STORE_PHASE | (step & UINT32_C(0xffff));
}

uint32_t hpclang_e2e_object_store(uint32_t model)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    int rpl_family = model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
                     model == HPCLANG_MODEL_HP50G;
    int live_stock_view = rpl_family && boot != NULL && boot->saturn_state != 0u;

    if (!live_stock_view) {
        errno = 0;
        struct hpclang_object_store *store = hpclang_object_store_home();
        if (store != NULL || errno != ENOSYS) {
            hpclang_object_store_close(store);
            return 0xc0u;
        }

        errno = 0;
        store = hpclang_object_store_current();
        if (store != NULL || errno != ENOSYS) {
            hpclang_object_store_close(store);
            return 0xc1u;
        }
        return 0u;
    }

    static const uint8_t e2e_witness_name[] = {'Z', 'Z', 'E', '2', 'E'};
    static const uint8_t e2e_witness_value[] = "HPCLANG-E2E";
    /*
     * x50ng's Stock VVFAT setup exposes a stable empty DOCSTR named
     * X50NG-VV.FAT in USEROB before the payload starts. HPCLANG.HP is the
     * FAT transfer/launcher file and is not a HOME variable, so it is not a
     * valid named-object-store witness on HP49g+/50g.
     */
    static const uint8_t hp4950_witness_name[] = {'X', '5', '0', 'N', 'G', '-',
                                                  'V', 'V', '.', 'F', 'A', 'T'};
    const uint8_t *witness_name =
        model == HPCLANG_MODEL_HP48GII ? e2e_witness_name : hp4950_witness_name;
    uint32_t witness_name_length = model == HPCLANG_MODEL_HP48GII
                                       ? (uint32_t)sizeof(e2e_witness_name)
                                       : (uint32_t)sizeof(hp4950_witness_name);
    int witness_has_e2e_value = model == HPCLANG_MODEL_HP48GII;

    static const uint8_t missing_name[] = {'_', '_', 'H', 'P', 'C', 'L', 'A', 'N', 'G', '_',
                                           'E', '2', 'E', '_', 'M', 'I', 'S', 'S', 'I', 'N',
                                           'G', '_', '8', 'A', '1', '7', '_', '_'};

    uint32_t failure = 0u;
    object_store_phase(1u); /* before HOME snapshot */
    struct hpclang_object_store *home = hpclang_object_store_home();
    struct hpclang_object_store *current = NULL;
    struct hpclang_object_store *child = NULL;
    struct hpclang_object_store_iterator *first = NULL;
    struct hpclang_object_store_iterator *second = NULL;
    struct hpclang_object_store_iterator *current_iterator = NULL;
    struct hpclang_object *witness = NULL;

    if (home == NULL) {
        failure = 0xc2u;
        goto done;
    }

    /*
     * Primary HP49/50 USB Stock runs receive the same stable HOME string
     * witness as HP48gII. The preserved SD/VVFAT secondary gate has no such
     * HOME transfer and keeps using X50NG-VV.FAT. Prefer the stronger USB
     * witness whenever it is present without making transport identity part
     * of the runtime API.
     */
    if (model != HPCLANG_MODEL_HP48GII) {
        struct hpclang_object *candidate = NULL;
        errno = 0;
        if (hpclang_object_store_get(home, e2e_witness_name, sizeof(e2e_witness_name),
                                     &candidate) == 0 &&
            candidate != NULL && hpclang_object_kind(candidate) == HPCLANG_OBJECT_STRING) {
            witness_name = e2e_witness_name;
            witness_name_length = (uint32_t)sizeof(e2e_witness_name);
            witness_has_e2e_value = 1;
        }
        hpclang_object_release(candidate);
    }

    object_store_phase(2u); /* HOME ready */
    object_store_phase(3u); /* before current-directory snapshot */
    current = hpclang_object_store_current();
    if (current == NULL) {
        failure = 0xc3u;
        goto done;
    }

    object_store_phase(4u); /* current path ready */
    object_store_phase(5u); /* before first HOME iterator snapshot */
    first = hpclang_object_store_iterator_open(home);
    if (first == NULL) {
        failure = 0xc4u;
        goto done;
    }
    object_store_phase(6u);
    second = hpclang_object_store_iterator_open(home);
    if (second == NULL) {
        failure = 0xc4u;
        goto done;
    }
    object_store_phase(7u);
    current_iterator = hpclang_object_store_iterator_open(current);
    if (current_iterator == NULL) {
        failure = 0xc4u;
        goto done;
    }

    object_store_phase(8u); /* all iterator snapshots ready */
    struct hpclang_object_store_entry entry1;
    struct hpclang_object_store_entry entry2;
    object_store_phase(9u); /* before first independent lookup */
    int found1 = object_store_find_named(first, witness_name, witness_name_length, &entry1);
    object_store_phase(10u);
    int found2 = object_store_find_named(second, witness_name, witness_name_length, &entry2);
    object_store_phase(11u);
    if (found1 != 1 || found2 != 1) {
        failure = 0xc5u;
        goto done;
    }

    if (entry1.kind != HPCLANG_OBJECT_STRING || entry2.kind != HPCLANG_OBJECT_STRING ||
        (entry1.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u ||
        (entry2.flags & HPCLANG_OBJECT_STORE_ENTRY_DIRECTORY) != 0u) {
        failure = 0xc6u;
        goto done;
    }

    object_store_phase(12u); /* before successful named-object snapshot */
    size_t witness_string_size = 0u;
    const size_t expected_witness_size =
        witness_has_e2e_value ? sizeof(e2e_witness_value) - 1u : 0u;
    errno = 0;
    if (hpclang_object_store_get(home, witness_name, witness_name_length, &witness) != 0 ||
        witness == NULL || hpclang_object_kind(witness) != HPCLANG_OBJECT_STRING ||
        hpclang_object_string_size(witness, &witness_string_size) != 0 ||
        witness_string_size != expected_witness_size) {
        failure = 0xc9u;
        goto done;
    }
    if (witness_has_e2e_value) {
        uint8_t witness_copy[sizeof(e2e_witness_value) - 1u];
        if (hpclang_object_string_copy(witness, witness_copy, sizeof(witness_copy)) != 0 ||
            memcmp(witness_copy, e2e_witness_value, sizeof(witness_copy)) != 0) {
            failure = 0xc9u;
            goto done;
        }
    }

    object_store_phase(13u); /* before ENOTDIR child-open check */
    errno = 0;
    child = hpclang_object_store_open_child(home, witness_name, witness_name_length);
    if (child != NULL || errno != ENOTDIR) {
        failure = 0xc7u;
        goto done;
    }

    object_store_phase(14u); /* before missing-object lookup */
    struct hpclang_object *missing = NULL;
    errno = 0;
    if (hpclang_object_store_get(home, missing_name, sizeof(missing_name), &missing) != -1 ||
        missing != NULL || errno != ENOENT) {
        hpclang_object_release(missing);
        failure = 0xc8u;
        goto done;
    }

    object_store_phase(15u); /* read-only object-store witness complete */

done:
    hpclang_object_release(witness);
    hpclang_object_store_iterator_close(current_iterator);
    hpclang_object_store_iterator_close(second);
    hpclang_object_store_iterator_close(first);
    hpclang_object_store_close(child);
    hpclang_object_store_close(current);
    hpclang_object_store_close(home);
    return failure;
}
