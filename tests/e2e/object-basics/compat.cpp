#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object.h>
#include <saturn.h>

static bool hpclang_e2e_restore_stack(uint32_t before)
{
    uint32_t current = 0u;
    if (hpclang_stack_depth(&current) != 0 || current < before)
        return false;
    if (current > before && hpclang_stack_drop(current - before) != 0)
        return false;
    current = 0u;
    return hpclang_stack_depth(&current) == 0 && current == before;
}

extern "C" int hpclang_e2e_saturn_compat_test(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (device == nullptr)
        return 1;

    const bool rpl_family = device->model == HPCLANG_MODEL_HP48GII ||
                            device->model == HPCLANG_MODEL_HP49GPLUS ||
                            device->model == HPCLANG_MODEL_HP50G;
    const bool live_stock_view = rpl_family && boot != nullptr && boot->saturn_state != 0u;
    if (!live_stock_view)
        return 0;

    uint32_t before = 0u;
    if (hpclang_stack_depth(&before) != 0)
        return 2;

    if (sat_push_zint_llong(21) != TRUE) {
        (void)hpclang_e2e_restore_stack(before);
        return 3;
    }
    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0 || depth != before + 1u || sat_pop_zint_llong() != 21 ||
        !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 3;
    }

    if (sat_push_real(1.5) != TRUE) {
        (void)hpclang_e2e_restore_stack(before);
        return 4;
    }
    double real = sat_pop_real();
    if (real < 1.49999999999 || real > 1.50000000001 || !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 4;
    }

    if (sat_stack_push_string("compat") != 1) {
        (void)hpclang_e2e_restore_stack(before);
        return 5;
    }
    char string_buffer[16];
    if (sat_pop_str(string_buffer) != string_buffer || std::strcmp(string_buffer, "compat") != 0 ||
        !hpclang_e2e_restore_stack(before)) {
        (void)hpclang_e2e_restore_stack(before);
        return 5;
    }

    /*
     * stdargs intentionally consumes the complete historical RPL stack. Only
     * exercise its successful path when the incoming user stack was empty; a
     * non-empty Stock stack must never be cleared just to make an E2E witness.
     */
    if (before == 0u) {
        if (sat_stack_push_string("'left'") != 1 || sat_stack_push_string("right") != 1) {
            (void)hpclang_e2e_restore_stack(before);
            return 6;
        }

        char **argv = nullptr;
        int argc = sat_stack_pop_stdargs(&argv);
        bool args_ok = argc == 2 && argv != nullptr && std::strcmp(argv[0], "left") == 0 &&
                       std::strcmp(argv[1], "right") == 0;
        if (argv != nullptr) {
            if (argc > 0)
                std::free(argv[0]);
            if (argc > 1)
                std::free(argv[1]);
            std::free(argv);
        }
        if (!args_ok || !hpclang_e2e_restore_stack(before)) {
            (void)hpclang_e2e_restore_stack(before);
            return 6;
        }
    }

    return 0;
}
