#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>

uint32_t hpclang_e2e_object_stack(uint32_t model)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    int rpl_family = model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
                     model == HPCLANG_MODEL_HP50G;
    int live_stock_view = rpl_family && boot != NULL && boot->saturn_state != 0u;

    uint32_t depth = UINT32_MAX;
    errno = 0;
    int result = hpclang_stack_depth(&depth);

    if (!live_stock_view) {
        if (result != -1 || depth != 0u || errno != ENOSYS)
            return 0xa0u;
        return 0u;
    }

    if (result != 0)
        return 0xa1u;

    uint32_t before = depth;
    if (before != 0u) {
        struct hpclang_object *object = NULL;
        errno = 0;
        result = hpclang_stack_peek(1u, &object);
        if (result == 0) {
            if (object == NULL)
                return 0xa2u;
            (void)hpclang_object_kind(object);
            hpclang_object_release(object);
        } else {
            /*
             * The first M7.3 slice is deliberately conservative: a Stock
             * object whose exact native boundary is not frozen yet reports
             * ENOTSUP rather than being copied heuristically.
             */
            if (object != NULL || errno != ENOTSUP)
                return 0xa3u;
        }
    }

    uint32_t after = UINT32_MAX;
    if (hpclang_stack_depth(&after) != 0 || after != before)
        return 0xa4u;

    if (before != UINT32_MAX) {
        struct hpclang_object *object = NULL;
        errno = 0;
        if (hpclang_stack_peek(before + 1u, &object) != -1 || object != NULL || errno != EINVAL)
            return 0xa5u;
    }

    /*
     * Build the mutation witness entirely inside HPC1. File Manager and VAR
     * recall can place a pre-existing X-register value above the recalled
     * launcher, so UI-prepared stack arguments are intentionally avoided.
     */
    struct hpclang_object *char_a = NULL;
    struct hpclang_object *char_b = NULL;
    if (hpclang_object_create_character('A', &char_a) != 0 ||
        hpclang_object_create_character('B', &char_b) != 0) {
        hpclang_object_release(char_a);
        hpclang_object_release(char_b);
        return 0xa6u;
    }

    uint8_t character = 0u;
    if (hpclang_object_character_value(char_a, &character) != 0 || character != 'A' ||
        hpclang_object_character_value(char_b, &character) != 0 || character != 'B') {
        hpclang_object_release(char_a);
        hpclang_object_release(char_b);
        return 0xa7u;
    }

    if (live_stock_view) {
        if (hpclang_stack_push(char_a) != 0) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xa8u;
        }

        uint32_t mutated_depth = UINT32_MAX;
        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 1u) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xa9u;
        }

        if (hpclang_stack_replace(1u, char_b) != 0) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xaau;
        }

        struct hpclang_object *peek = NULL;
        if (hpclang_stack_peek(1u, &peek) != 0 || peek == NULL ||
            hpclang_object_character_value(peek, &character) != 0 || character != 'B') {
            hpclang_object_release(peek);
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xabu;
        }
        hpclang_object_release(peek);

        if (hpclang_stack_push(char_a) != 0) {
            (void)hpclang_stack_drop(1u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xacu;
        }

        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 2u) {
            (void)hpclang_stack_drop(2u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xadu;
        }

        struct hpclang_object *popped = NULL;
        if (hpclang_stack_pop(&popped) != 0 || popped == NULL ||
            hpclang_object_character_value(popped, &character) != 0 || character != 'A') {
            hpclang_object_release(popped);
            (void)hpclang_stack_drop(2u);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xaeu;
        }
        hpclang_object_release(popped);

        if (hpclang_stack_depth(&mutated_depth) != 0 || mutated_depth != before + 1u ||
            hpclang_stack_drop(1u) != 0 || hpclang_stack_depth(&mutated_depth) != 0 ||
            mutated_depth != before) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xafu;
        }
    } else {
        errno = 0;
        if (hpclang_stack_push(char_a) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb0u;
        }

        errno = 0;
        if (hpclang_stack_replace(1u, char_b) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb1u;
        }

        struct hpclang_object *popped = NULL;
        errno = 0;
        if (hpclang_stack_pop(&popped) != -1 || popped != NULL || errno != ENOSYS) {
            hpclang_object_release(popped);
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb2u;
        }

        errno = 0;
        if (hpclang_stack_drop(1u) != -1 || errno != ENOSYS) {
            hpclang_object_release(char_a);
            hpclang_object_release(char_b);
            return 0xb3u;
        }
    }

    hpclang_object_release(char_a);
    hpclang_object_release(char_b);

    return 0u;
}
