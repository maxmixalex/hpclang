#include <stdint.h>

#include <hpclang/abi.h>

/*
 * Real-hardware return-only gate.
 *
 * Entry into HPC1 has already been proven on HP39gs.  This test deliberately
 * does not touch the framebuffer, GPIO, interrupt mask or CPSR and performs no
 * busy-wait.  Its only job is to prove that a normal HPC1 main() can return
 * through the common bootstrap and stock ARMToolbox launcher.
 */
volatile uint32_t hpclang_e2e_result;
volatile uint32_t hpclang_e2e_model;
volatile uint32_t hpclang_e2e_rw_bytes;

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    hpclang_e2e_model = boot->model;
    hpclang_e2e_rw_bytes = boot->rw_bytes;
    hpclang_e2e_result = 0x50415353u;

    return 0;
}
