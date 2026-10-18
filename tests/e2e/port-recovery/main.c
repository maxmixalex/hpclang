/* Standalone native recovery witness, not a user-facing recovery application.
 * The saved-state verifier owns acceptance: the final marker alone is not PASS.
 * Diagnostics retain the entry/mapping/TLS/clock and first/repeated replay results.
 */
#include "rpl-flash-replay.h"
#include <stddef.h>
#include "sst-flash.h"
#include <hpclang/time.h>
__thread int errno;
__thread unsigned tls_probe = 0x12345678;
#ifdef HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT
/* Packaging may bind this image to another reserved pair without recompiling.
 * Volatile prevents embedding the build defaults in instructions. These fields
 * identify allocations; they do not establish installation trust or freshness. */
const volatile struct
{
    unsigned magic[2], journal_slot, authority_slot;
} hpclang_recovery_binding = {{0x42435048u, 0x31444e49u},
                              HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT,
                              HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT +
                                  HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES};
_Static_assert(sizeof hpclang_recovery_binding == 16, "Recovery packager binding ABI");
#define AUTHORITY_SLOT hpclang_recovery_binding.authority_slot
#endif

static int replay_snapshot(const struct hpclang_rpl_flash_replay_io *io, uint32_t slot,
                           const uint8_t *expected, uint32_t bytes, uint8_t *scratch,
                           uint32_t capacity)
{
#ifdef HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT
    return hpclang_rpl_flash_sector_replay_bound(io, AUTHORITY_SLOT, slot, expected, bytes, scratch,
                                                 capacity);
#else
    return hpclang_rpl_flash_sector_replay_expected(io, slot, expected, bytes, scratch, capacity);
#endif
}
struct workspace
{
    unsigned marker;
    int result;
    unsigned slot, target, reads, sp;
    unsigned char scratch[8248];
    unsigned diagnostics[32];
};
_Static_assert(sizeof(struct workspace) == 8400, "entry.S workspace reservation");
_Static_assert(offsetof(struct workspace, slot) == 8, "entry.S recovery restart condition");
_Static_assert(offsetof(struct workspace, diagnostics[7]) == 8300,
               "entry.S restored CPSR diagnostic offset");
static int read_flash(void *context, unsigned address, unsigned short *value)
{
    struct workspace *out = context;
    if ((address & 1) || address < 0x140000 || address >= 0x200000)
        return 0;
    *value = *(volatile unsigned short *)address;
    ++out->reads;
    return 1;
}
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *to = d;
    const unsigned char *from = s;
    for (size_t i = 0; i < n; ++i)
        to[i] = from[i];
    return d;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; ++i)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}
void __aeabi_memcpy(void *d, const void *s, size_t n)
{
    memcpy(d, s, n);
}
void __aeabi_memcpy4(void *d, const void *s, size_t n)
{
    memcpy(d, s, n);
}
void __aeabi_memclr4(void *d, size_t n)
{
    unsigned char *to = d;
    for (size_t i = 0; i < n; ++i)
        to[i] = 0;
}

unsigned short hpclang_flash_read16(void *, unsigned);
void hpclang_flash_write16(void *, unsigned, unsigned short);
static unsigned short bus_read(void *context, unsigned address)
{
    return hpclang_flash_read16(context, address);
}
static void bus_write(void *context, unsigned address, unsigned short value)
{
    hpclang_flash_write16(context, address, value);
}
static unsigned flash_time(void *context)
{
    (void)context;
    return (unsigned)hpclang_time_monotonic_us();
}
static struct hpclang_sst_bus bus_for(void *context)
{
    struct hpclang_sst_bus bus = {context, bus_read, bus_write, flash_time};
    return bus;
}
static int program_flash(void *context, unsigned address, unsigned short value)
{
    struct workspace *out = context;
    if ((address & ~4095u) != out->target
#ifdef HPCLANG_RECOVERY_RETIRE_JOURNAL
        && address != (out->slot & ~1u)
#endif
#ifdef HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT
        && address != (AUTHORITY_SLOT & ~1u)
#endif
    )
        return 0;
#ifdef HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY
    /* Test-only command-boundary interruption. The caller retains ownership
     * in its failure loop; this does not simulate a Flash pulse in progress. */
    if (address == out->target + 0x100u)
        return 0;
#endif
    struct hpclang_sst_bus bus = bus_for(context);
    struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    ++out->diagnostics[29];
    int status = hpclang_sst_program_word(&bus, &region, address, value, 1000);
    if (status == HPCLANG_SST_BUSY) {
        do {
            status = hpclang_sst_wait_ready(&bus, address, 100000);
        } while (status == HPCLANG_SST_BUSY);
    }
    return status == HPCLANG_SST_OK && hpclang_flash_read16(context, address) == value;
}
static int erase_flash(void *context, unsigned address)
{
    struct workspace *out = context;
    if (address != out->target)
        return 0;
    struct hpclang_sst_bus bus = bus_for(context);
    struct hpclang_sst_region region = {0x140000, 0x200000, 4096};
    ++out->diagnostics[28];
    int status = hpclang_sst_erase_sector(&bus, &region, address, 100000);
    if (status == HPCLANG_SST_BUSY) {
        do {
            status = hpclang_sst_wait_ready(&bus, address, 100000);
        } while (status == HPCLANG_SST_BUSY);
    }
    if (status != HPCLANG_SST_OK)
        return 0;
    for (unsigned i = 0; i < 4096; i += 2)
        if (hpclang_flash_read16(context, address + i) != 65535)
            return 0;
    return 1;
}

void hpclang_flash_mapping_swap(unsigned descriptors[2]);
unsigned recovery_cpsr(void);
void exercise(struct workspace *out, unsigned caller_cpsr)
{
    unsigned sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    out->sp = sp;
    unsigned cpsr = recovery_cpsr();
    out->diagnostics[0] = caller_cpsr;
    out->diagnostics[1] = cpsr;
    out->diagnostics[2] = *(volatile unsigned *)0x08000008;
    out->reads = 0;
    out->slot = 0;
    out->target = 0;
    const volatile unsigned *l1 = (void *)0x07f00000;
    if ((l1[0] & 0xfff00803) != 0x802 || (l1[1] & 0xfff00803) != 0x100802) {
        out->result = -99;
        out->marker = 0x48505046;
        return;
    }
    if ((cpsr & 0xc0) != 0xc0) {
        out->result = -98;
        out->marker = 0x48505046;
        return;
    }
    unsigned descriptors[2];
    for (unsigned i = 0; i < 2; ++i) {
        out->diagnostics[3 + i] = l1[i];
        descriptors[i] = (l1[i] & ~0xcu) | 0xc00u;
    }
    hpclang_flash_mapping_swap(descriptors);
    out->diagnostics[5] = l1[0];
    out->diagnostics[6] = l1[1];
    struct hpclang_rpl_flash_io io = {out, read_flash, 0, 0x140000, 0x200000};
#ifdef HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT
    out->result = HPCLANG_RPL_FLASH_INVALID;
    if (hpclang_recovery_binding.magic[0] == 0x42435048u &&
        hpclang_recovery_binding.magic[1] == 0x31444e49u)
        out->result = hpclang_rpl_flash_preflight_bound(
            &io, AUTHORITY_SLOT, hpclang_recovery_binding.journal_slot, out->scratch,
            sizeof(out->scratch), &out->target);
    if (out->result > 0)
        out->slot = hpclang_recovery_binding.journal_slot;
#else
    out->result = hpclang_rpl_flash_preflight_sector(&io, out->scratch, sizeof(out->scratch),
                                                     &out->slot, &out->target);
#endif
    uint64_t clock;
    int recoverable = out->result == 1;
#ifdef HPCLANG_RECOVERY_RETIRE_JOURNAL
    recoverable = recoverable || out->result == 2;
#endif
    if (recoverable && hpclang_time_read_us(&clock) == 0) {
        struct hpclang_sst_bus bus = bus_for(out);
        struct hpclang_sst_device device;
        out->diagnostics[26] = hpclang_sst_identify(&bus, &device);
        if (out->diagnostics[26] == HPCLANG_SST_OK && device.bytes == 0x200000 &&
            device.sector_bytes == 4096) {
            struct hpclang_rpl_flash_replay_io replay = {
                {out, read_flash, program_flash, 0x140000, 0x200000}, erase_flash};
            const uint32_t record_bytes = HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES;
            if (out->result == 1) {
                out->diagnostics[27] = replay_snapshot(&replay, out->slot, out->scratch,
                                                       record_bytes, out->scratch + record_bytes,
                                                       sizeof(out->scratch) - record_bytes);
                if (out->diagnostics[27] != 0) {
                    out->marker = 0x48504641;
                    for (;;)
                        __asm__ volatile("nop");
                }
                unsigned erases = out->diagnostics[28], programs = out->diagnostics[29];
                out->diagnostics[30] = replay_snapshot(&replay, out->slot, out->scratch,
                                                       record_bytes, out->scratch + record_bytes,
                                                       sizeof(out->scratch) - record_bytes);
                out->diagnostics[31] =
                    (out->diagnostics[28] - erases) + (out->diagnostics[29] - programs);
                if (out->diagnostics[30] != 0 || out->diagnostics[31] != 0) {
                    out->marker = 0x48504641;
                    for (;;)
                        __asm__ volatile("nop");
                }
            }
#ifdef HPCLANG_RECOVERY_RETIRE_JOURNAL
            /* This witness owns the injected transaction. Production recovery
             * still needs freshness and reference authority before retirement.
             * Diagnostic 30 is the final operation result; 31 counts retirement
             * writes, including the required write-free repeated invocation. */
            unsigned retirement_programs = out->diagnostics[29];
            out->diagnostics[30] = hpclang_rpl_flash_retire_sector(&replay.flash, out->slot,
                                                                   out->scratch, record_bytes);
            if (out->diagnostics[30] != 0) {
                out->marker = 0x48504641;
                for (;;)
                    __asm__ volatile("nop");
            }
            unsigned repeated_programs = out->diagnostics[29];
            out->diagnostics[30] = hpclang_rpl_flash_retire_sector(&replay.flash, out->slot,
                                                                   out->scratch, record_bytes);
            out->diagnostics[31] = out->diagnostics[29] - retirement_programs;
            if (out->diagnostics[30] != 0 || out->diagnostics[29] != repeated_programs) {
                out->marker = 0x48504641;
                for (;;)
                    __asm__ volatile("nop");
            }
#ifdef HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT
            out->diagnostics[30] = hpclang_rpl_flash_close_authority(
                &replay.flash, AUTHORITY_SLOT, out->slot, out->scratch, record_bytes);
            if (out->diagnostics[30] != 0) {
                out->marker = 0x48504641;
                for (;;)
                    __asm__ volatile("nop");
            }
#endif
#endif
        }
    }

    hpclang_flash_mapping_swap(descriptors);
    out->diagnostics[8] = l1[0];
    out->diagnostics[9] = l1[1];
    out->diagnostics[10] = *(volatile unsigned *)0x08000008;
    out->diagnostics[11] = *(volatile unsigned *)0x07600000;
    out->diagnostics[12] = *(volatile unsigned *)0x07600004;
    out->diagnostics[13] = *(volatile unsigned *)0x07600008;
    out->diagnostics[14] = *(volatile unsigned short *)0x0760000c;
    out->diagnostics[15] = *(volatile unsigned short *)0x07600014;
    uint64_t before = 0, after = 0;
    out->diagnostics[20] = hpclang_time_read_us(0);
    out->diagnostics[21] = errno;
    out->diagnostics[22] = (unsigned)&errno;
    out->diagnostics[23] = tls_probe++;
    out->diagnostics[24] = tls_probe;
    out->diagnostics[16] = hpclang_time_read_us(&before);
    unsigned iterations = 0;
    if (out->diagnostics[16] == 0) {
        after = before;
        while (iterations++ < 100000 && after - before < 1000) {
            if (hpclang_time_read_us(&after)) {
                out->diagnostics[16] = 1;
                break;
            }
        }
    }
    out->diagnostics[17] = (unsigned)before;
    out->diagnostics[18] = (unsigned)after;
    out->diagnostics[19] = iterations;

    out->marker = 0x48505046;
}
