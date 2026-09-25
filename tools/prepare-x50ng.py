#!/usr/bin/env python3
"""Prepare one pinned x50ng build for all hpclang headless E2E modes/models."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str, what: str) -> None:
    text = path.read_text()
    if old not in text:
        raise SystemExit(f"{path}: {what} anchor changed")
    path.write_text(text.replace(old, new, 1))


def copy_frontend(source: Path, destination: Path, entrypoint: str) -> None:
    text = source.read_text()
    old = "int main(int argc, char **argv)"
    if text.count(old) != 1:
        raise SystemExit(f"{source}: expected exactly one x50ng frontend main()")
    destination.write_text(text.replace(old, f"int {entrypoint}(int argc, char **argv)", 1))


def verify_ui_build_inheritance(root: Path) -> None:
    makefile = root / "Makefile"
    matches = []
    for index, line in enumerate(makefile.read_text().splitlines()):
        tokens = line.split()
        if (
            "libui4x.a" in tokens
            and "WITH_SDL=$(WITH_SDL)" in tokens
            and "WITH_SDL2=$(WITH_SDL2)" in tokens
        ):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one inherited ui4x SDL selector line, found {len(matches)}"
        )


def patch_universal_flash(root: Path) -> None:
    replace_once(
        root / "src" / "flash.c",
        'memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0',
        '''memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP39G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP40G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP48GIIMAGE", 16 ) != 0''',
        "universal HP39/40/48 firmware headers",
    )


def patch_runtime_sram(root: Path) -> None:
    path = root / "src" / "sram.c"

    replace_once(
        path,
        "#define S3C2410_SRAM_SIZE 0x00080000\n",
        '''#define S3C2410_SRAM_DEFAULT_SIZE 0x00080000
#define S3C2410_SRAM_HP48_SIZE 0x00040000

static size_t hpclang_sram_size( void )
{
    const char* text = getenv( "HPCLANG_X50NG_SRAM_SIZE" );
    char* end = NULL;
    unsigned long value;

    if ( text == NULL || *text == '\\0' )
        return S3C2410_SRAM_DEFAULT_SIZE;

    errno = 0;
    value = strtoul( text, &end, 0 );
    if ( errno != 0 || end == text || *end != '\\0' ||
         ( value != S3C2410_SRAM_DEFAULT_SIZE &&
           value != S3C2410_SRAM_HP48_SIZE ) ) {
        fprintf( stderr,
                 "hpclang-x50ng: unsupported SRAM size %s "
                 "(expected 0x40000 or 0x80000)\\n",
                 text );
        return 0;
    }

    return ( size_t )value;
}
''',
        "runtime SRAM selector",
    )
    replace_once(
        path,
        "    sram->size = 0x00080000;\n",
        "    if ( sram->size == 0 ) return -EINVAL;\n",
        "SRAM load keeps runtime-selected size",
    )
    replace_once(
        path,
        '''    sram->data = ( void* )-1;
    sram->shadow = ( void* )-1;
    sram->offset = phys_ram_size;
    phys_ram_size += S3C2410_SRAM_SIZE;
    phys_ram_size += S3C2410_SRAM_SIZE;

    cpu_register_physical_memory( S3C2410_SRAM_BASE, S3C2410_SRAM_SIZE, sram->offset | IO_MEM_RAM );
    cpu_register_physical_memory( S3C2410_SRAM_BASE + S3C2410_SRAM_SIZE, S3C2410_SRAM_SIZE,
                                  ( sram->offset + S3C2410_SRAM_SIZE ) | IO_MEM_RAM );''',
        '''    sram->data = ( void* )-1;
    sram->shadow = ( void* )-1;
    sram->size = hpclang_sram_size();
    if ( sram->size == 0 ) {
        free( sram );
        module->user_data = NULL;
        return -EINVAL;
    }
    sram->offset = phys_ram_size;
    phys_ram_size += sram->size;
    phys_ram_size += sram->size;

    cpu_register_physical_memory( S3C2410_SRAM_BASE, sram->size, sram->offset | IO_MEM_RAM );
    cpu_register_physical_memory( S3C2410_SRAM_BASE + sram->size, sram->size,
                                  ( sram->offset + sram->size ) | IO_MEM_RAM );''',
        "runtime SRAM map size",
    )


def patch_host_usbdev(root: Path, args: argparse.Namespace) -> None:
    """Add the headless S3C2410 UDC + USB byte-stream host bridge."""
    path = root / "src" / "s3c2410" / "s3c2410_usbdev.c"
    support = args.headless_header.parent
    shutil.copy2(
        support / "hpclang-usbdev-host.h",
        root / "src" / "s3c2410" / "hpclang-usbdev-host.h",
    )
    shutil.copy2(
        support / "hpclang-usbdev-host.c",
        root / "src" / "s3c2410" / "hpclang-usbdev-host.c",
    )

    replace_once(
        path,
        '#include "../module.h"\n\n#include "s3c2410.h"\n',
        '''#include <errno.h>
#include "../module.h"
#include "../timer.h"

#include "s3c2410.h"
#include "s3c2410_intc.h"
#include "s3c2410_io_port.h"
#include "hpclang-usbdev-host.h"
#include "hpclang-usbdev-host.c"
''',
        "USB host includes",
    )

    replace_once(
        path,
        '''    unsigned int nr_regs;
    s3c2410_offset_t* regs;
} s3c2410_usbdev_t;''',
        '''    unsigned int nr_regs;
    s3c2410_offset_t* regs;

    struct hpusb_core host;
    hdw_t* hdw_state;
    int host_fd;
    hdw_timer_t* host_timer;
    int64_t next_sof;
    enum hpusb_enum_state trace_state;
    int cable_asserted;
} s3c2410_usbdev_t;''',
        "USB host state",
    )

    read_start = path.read_text().find(
        "static uint32_t s3c2410_usbdev_read( void* opaque, target_phys_addr_t offset )"
    )
    write_start = path.read_text().find(
        "static void s3c2410_usbdev_write( void* opaque, target_phys_addr_t offset, uint32_t data )"
    )
    load_start = path.read_text().find(
        "static int s3c2410_usbdev_load( hdw_module_t* module, GKeyFile* key )"
    )
    if read_start < 0 or write_start < 0 or load_start < 0 or not (
        read_start < write_start < load_start
    ):
        raise SystemExit(f"{path}: USB read/write function anchors changed")

    text = path.read_text()
    host_impl = r'''static void hpclang_usbdev_irq( void* opaque, int level )
{
    s3c2410_usbdev_t* usbdev = opaque;
    if ( level )
        s3c2410_intc_assert( usbdev->hdw_state, INT_USBD, 1 );
    else
        s3c2410_intc_deassert( usbdev->hdw_state, INT_USBD );
}

static int hpclang_usbdev_trace_cpu_enabled( void )
{
    const char* value = getenv( "HPCLANG_X50NG_USB_TRACE_CPU" );
    return value != NULL && *value != '\0';
}

static int hpclang_usbdev_trace_cpu_offset( target_phys_addr_t offset )
{
    return offset == HPUSB_EP_INT ||
           offset == HPUSB_INDEX ||
           offset == HPUSB_OUT_CSR1 ||
           offset == HPUSB_OUT_CNT1 ||
           offset == HPUSB_OUT_CNT2;
}

static void hpclang_usbdev_trace_cpu(
    s3c2410_usbdev_t* usbdev,
    const char* operation,
    target_phys_addr_t offset,
    uint32_t data )
{
    static int ep3_code_dumped = 0;

    if ( !hpclang_usbdev_trace_cpu_enabled() ||
         !hpclang_usbdev_trace_cpu_offset( offset ) )
        return;

    fprintf(
        stderr,
        "hpclang-usbdev-cpu:%s: %s offset=0x%04lx data=0x%08x "
        "pc=0x%08x lr=0x%08x\n",
        getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?",
        operation,
        ( unsigned long )offset,
        data,
        usbdev->hdw_state->cpu->regs[ 15 ],
        usbdev->hdw_state->cpu->regs[ 14 ] );

    /*
     * The HP49/50 Stock USB ISR executes from relocated low RAM.  The
     * packaged firmware offset is therefore not the runtime PC.  Capture the
     * actual instructions once, at the first EP3 interrupt acknowledge, so
     * the handler can be disassembled at its true runtime addresses.
     */
    if ( !ep3_code_dumped &&
         strcmp( operation, "write" ) == 0 &&
         offset == HPUSB_EP_INT && ( data & 0x08u ) != 0 ) {
        static const struct {
            target_ulong start;
            size_t size;
        } ranges[] = {
            { 0x0000df80u, 0x100u },
            { 0x0000e700u, 0x300u },
        };
        size_t range_index;

        for ( range_index = 0;
              range_index < sizeof( ranges ) / sizeof( ranges[ 0 ] );
              ++range_index ) {
            uint8_t code[ 0x300 ];
            target_ulong start = ranges[ range_index ].start;
            size_t size = ranges[ range_index ].size;
            size_t pos;

            if ( cpu_memory_rw_debug(
                     usbdev->hdw_state->cpu, start, code, size, 0 ) == 0 ) {
                fprintf(
                    stderr,
                    "hpclang-usbdev-code:%s: start=0x%08lx size=0x%zx cpsr=0x%08x\n",
                    getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?",
                    ( unsigned long )start, size,
                    usbdev->hdw_state->cpu->uncached_cpsr );
                for ( pos = 0; pos < size; pos += 16 ) {
                    size_t i;
                    fprintf(
                        stderr, "hpclang-usbdev-code:%s: %08lx:",
                        getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?",
                        ( unsigned long )( start + pos ) );
                    for ( i = 0; i < 16 && pos + i < size; ++i )
                        fprintf( stderr, "%02x", code[ pos + i ] );
                    fputc( '\n', stderr );
                }
            } else {
                fprintf(
                    stderr,
                    "hpclang-usbdev-code:%s: runtime RAM dump failed at 0x%08lx\n",
                    getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?",
                    ( unsigned long )start );
            }
        }
        ep3_code_dumped = 1;
    }
}

static int hpclang_usbdev_stream_read(
    void* opaque, uint8_t* buffer, size_t capacity )
{
    s3c2410_usbdev_t* usbdev = opaque;
    ssize_t count;

    if ( usbdev->host_fd < 0 )
        return 0;
    count = read( usbdev->host_fd, buffer, capacity );
    if ( count > 0 )
        return ( int )count;
    if ( count < 0 && errno != EAGAIN && errno != EWOULDBLOCK )
        fprintf( stderr, "hpclang-usbdev: host read failed: %s\n",
                 strerror( errno ) );
    return 0;
}

static int hpclang_usbdev_stream_write(
    void* opaque, const uint8_t* buffer, size_t length )
{
    s3c2410_usbdev_t* usbdev = opaque;
    ssize_t count;

    if ( usbdev->host_fd < 0 )
        return 0;
    count = write( usbdev->host_fd, buffer, length );
    if ( count >= 0 )
        return ( int )count;
    if ( errno != EAGAIN && errno != EWOULDBLOCK )
        fprintf( stderr, "hpclang-usbdev: host write failed: %s\n",
                 strerror( errno ) );
    return 0;
}

static int hpclang_usbdev_connect_ready( void )
{
    const char* marker = getenv( "HPCLANG_X50NG_USB_CONNECT_FILE" );
    if ( marker == NULL || *marker == '\0' )
        return 1;
    return access( marker, F_OK ) == 0;
}

static uint32_t hpclang_usbdev_phys32( target_phys_addr_t address )
{
    uint32_t value = 0;
    cpu_physical_memory_read(
        address, ( uint8_t* )&value, sizeof( value ) );
    return value;
}

static void hpclang_usbdev_trace_cable_irq_state(
    const char* target,
    const char* phase )
{
    if ( getenv( "HPCLANG_X50NG_USB_TRACE_STATE" ) == NULL )
        return;

    fprintf(
        stderr,
        "hpclang-usbdev-cable:%s:%s "
        "GPFCON=%08x GPFDAT=%08x EXTINT0=%08x "
        "SRCPND=%08x INTMSK=%08x INTPND=%08x\n",
        target,
        phase,
        hpclang_usbdev_phys32( 0x56000050u ),
        hpclang_usbdev_phys32( 0x56000054u ),
        hpclang_usbdev_phys32( 0x56000088u ),
        hpclang_usbdev_phys32( 0x4a000000u ),
        hpclang_usbdev_phys32( 0x4a000008u ),
        hpclang_usbdev_phys32( 0x4a000010u ) );
}

static void hpclang_usbdev_host_poll( void* opaque )
{
    static int usb_active_last = -1;
    s3c2410_usbdev_t* usbdev = opaque;
    const char* target =
        getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?";

    /*
     * HP49/50 Stock treats cable insertion and host reset as two distinct
     * events.  On real hardware GPF1/EINT1 lets Stock re-initialize the UDC
     * before the host starts bus reset.  Do not collapse both events into the
     * same 100-us poll: doing so lets stale endpoint-enable state trigger a
     * reset before Stock's cable interrupt has armed the receive path.
     *
     * The explicit connect marker is HP49/50-only.  Other targets retain the
     * historical immediate-connect behavior below.
     */
    {
        const char* connect_marker =
            getenv( "HPCLANG_X50NG_USB_CONNECT_FILE" );

        if ( connect_marker != NULL && *connect_marker != '\0' ) {
            if ( hpclang_usbdev_connect_ready() &&
                 !usbdev->cable_asserted &&
                 usbdev->hdw_state->s3c2410_io_port != NULL ) {
                hpclang_usbdev_trace_cable_irq_state( target, "before" );
                s3c2410_io_port_f_set_bit( usbdev->hdw_state, 1, 1 );
                usbdev->cable_asserted = 1;
                hpclang_usbdev_trace_cable_irq_state( target, "after" );
                if ( getenv( "HPCLANG_X50NG_USB_TRACE_STATE" ) != NULL )
                    fprintf(
                        stderr,
                        "hpclang-usbdev-state:%s: cable GPF1 asserted; "
                        "waiting for Stock USB active state "
                        "pc=0x%08x lr=0x%08x\n",
                        target,
                        usbdev->hdw_state->cpu->regs[ 15 ],
                        usbdev->hdw_state->cpu->regs[ 14 ] );
            }

            if ( usbdev->cable_asserted && !usbdev->host.connected ) {
                uint8_t usb_active = 0;
                if ( cpu_memory_rw_debug(
                         usbdev->hdw_state->cpu,
                         0x08000cf9u,
                         &usb_active,
                         sizeof( usb_active ),
                         0 ) == 0 &&
                     usb_active == 1u ) {
                    hpusb_set_connected( &usbdev->host, 1 );
                    usbdev->next_sof = timer_get_clock() + 1000;
                    if ( getenv( "HPCLANG_X50NG_USB_TRACE_STATE" ) != NULL )
                        fprintf(
                            stderr,
                            "hpclang-usbdev-state:%s: Stock USB active; "
                            "host bus connected "
                            "pc=0x%08x lr=0x%08x\n",
                            target,
                            usbdev->hdw_state->cpu->regs[ 15 ],
                            usbdev->hdw_state->cpu->regs[ 14 ] );
                }
            }
        } else {
            if ( !usbdev->host.connected && hpclang_usbdev_connect_ready() ) {
                hpusb_set_connected( &usbdev->host, 1 );
                usbdev->next_sof = timer_get_clock() + 1000;
            }

            if ( usbdev->host.connected &&
                 !usbdev->cable_asserted &&
                 usbdev->hdw_state->s3c2410_io_port != NULL ) {
                s3c2410_io_port_f_set_bit( usbdev->hdw_state, 1, 1 );
                usbdev->cable_asserted = 1;
            }
        }
    }

    if ( !usbdev->host.connected ) {
        if ( usbdev->host_timer )
            timer_mod( usbdev->host_timer, timer_get_clock() + 100 );
        return;
    }

    if ( getenv( "HPCLANG_X50NG_USB_TRACE_STATE" ) != NULL ) {
        uint8_t usb_active = 0xffu;
        if ( cpu_memory_rw_debug(
                 usbdev->hdw_state->cpu,
                 0x08000cf9u,
                 &usb_active,
                 sizeof( usb_active ),
                 0 ) == 0 &&
             usb_active_last != ( int )usb_active ) {
            fprintf(
                stderr,
                "hpclang-usbdev-state:%s: active@08000cf9=%u "
                "pc=0x%08x lr=0x%08x enum=%u pwr=0x%02x\n",
                target,
                ( unsigned )usb_active,
                usbdev->hdw_state->cpu->regs[ 15 ],
                usbdev->hdw_state->cpu->regs[ 14 ],
                ( unsigned )usbdev->host.enum_state,
                usbdev->host.pwr );
            usb_active_last = ( int )usb_active;
        }
    }

    /*
     * x50ng's timer clock is in microseconds.  Keep the byte-stream bridge
     * responsive at 100 us, but advance S3C2410 FRAME_NUM only from the
     * full-speed USB 1 ms SOF time base.
     */
    int64_t now = timer_get_clock();
    if ( usbdev->next_sof == 0 )
        usbdev->next_sof = now + 1000;
    while ( now >= usbdev->next_sof ) {
        hpusb_sof( &usbdev->host );
        usbdev->next_sof += 1000;
    }

    hpusb_tick( &usbdev->host );

    if ( getenv( "HPCLANG_X50NG_USB_TRACE" ) != NULL &&
         usbdev->trace_state != usbdev->host.enum_state ) {
        fprintf(
            stderr,
            "hpclang-usbdev: enum state %u -> %u "
            "in=ep%u out=ep%u addr=0x%02x\n",
            ( unsigned )usbdev->trace_state,
            ( unsigned )usbdev->host.enum_state,
            usbdev->host.bulk_in_ep,
            usbdev->host.bulk_out_ep,
            usbdev->host.func_addr );
        usbdev->trace_state = usbdev->host.enum_state;
    }

    if ( usbdev->host_timer )
        timer_mod( usbdev->host_timer, timer_get_clock() + 100 );
}

static uint32_t s3c2410_usbdev_read(
    void* opaque, target_phys_addr_t offset )
{
    s3c2410_usbdev_t* usbdev = opaque;
    s3c2410_offset_t* reg;
    uint32_t data;

    if ( !S3C2410_OFFSET_OK( usbdev, offset ) )
        return ~( 0 );

    reg = S3C2410_OFFSET_ENTRY( usbdev, offset );
    if ( usbdev->host_fd < 0 )
        return *( reg->datap );

    data = hpusb_read( &usbdev->host, ( uint32_t )offset );
    hpclang_usbdev_trace_cpu( usbdev, "read", offset, data );
    if ( getenv( "HPCLANG_X50NG_USB_TRACE" ) != NULL )
        fprintf(
            stderr,
            "hpclang-usbdev: read offset=0x%04lx data=0x%08x\n",
            ( unsigned long )offset,
            data );
    return data;
}

static void s3c2410_usbdev_write(
    void* opaque, target_phys_addr_t offset, uint32_t data )
{
    s3c2410_usbdev_t* usbdev = opaque;
    s3c2410_offset_t* reg;

    if ( !S3C2410_OFFSET_OK( usbdev, offset ) )
        return;

    reg = S3C2410_OFFSET_ENTRY( usbdev, offset );
    *( reg->datap ) = data;
    if ( offset == HPUSB_PWR &&
         getenv( "HPCLANG_X50NG_USB_TRACE_STATE" ) != NULL ) {
        uint32_t stack_words[ 8 ] = { 0 };
        target_ulong sp = usbdev->hdw_state->cpu->regs[ 13 ];
        int stack_ok =
            cpu_memory_rw_debug(
                usbdev->hdw_state->cpu,
                sp,
                ( uint8_t* )stack_words,
                sizeof( stack_words ),
                0 ) == 0;

        fprintf(
            stderr,
            "hpclang-usbdev-state:%s: PWR write=0x%08x "
            "pc=0x%08x lr=0x%08x sp=0x%08lx "
            "stack=%s%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
            getenv( "TARGET" ) != NULL ? getenv( "TARGET" ) : "?",
            data,
            usbdev->hdw_state->cpu->regs[ 15 ],
            usbdev->hdw_state->cpu->regs[ 14 ],
            ( unsigned long )sp,
            stack_ok ? "" : "unreadable:",
            stack_words[ 0 ], stack_words[ 1 ],
            stack_words[ 2 ], stack_words[ 3 ],
            stack_words[ 4 ], stack_words[ 5 ],
            stack_words[ 6 ], stack_words[ 7 ] );
    }
    hpclang_usbdev_trace_cpu( usbdev, "write", offset, data );

    if ( getenv( "HPCLANG_X50NG_USB_TRACE" ) != NULL )
        fprintf(
            stderr,
            "hpclang-usbdev: write offset=0x%04lx reg=%s data=0x%08x\n",
            ( unsigned long )offset,
            reg->name,
            data );

    if ( usbdev->host_fd >= 0 )
        hpusb_write( &usbdev->host, ( uint32_t )offset, data );
}

'''
    path.write_text(text[:read_start] + host_impl + text[load_start:])

    # x50ng resets all modules after a missing/partial state file and on
    # guest software reset. Preserve the open host bridge, but reset the UDC
    # protocol state and re-assert cable detect after io_port is reset.
    text = path.read_text()
    reset_start = text.find(
        "static int s3c2410_usbdev_reset( hdw_module_t* module, hdw_reset_t reset )"
    )
    readfn_start = text.find("static CPUReadMemoryFunc*", reset_start)
    if reset_start < 0 or readfn_start < 0:
        raise SystemExit(f"{path}: USB reset function anchors changed")
    reset_body = text[reset_start:readfn_start]
    needle = "\n    return 0;\n}\n"
    pos = reset_body.rfind(needle)
    if pos < 0:
        raise SystemExit(f"{path}: USB reset return anchor changed")
    reset_body = (
        reset_body[:pos]
        + """
    if ( usbdev->host_fd >= 0 ) {
        hpusb_reset_device( &usbdev->host );
        usbdev->next_sof = timer_get_clock() + 1000;
        usbdev->cable_asserted = 0;
    }
"""
        + reset_body[pos:]
    )
    path.write_text(text[:reset_start] + reset_body + text[readfn_start:])
    # USB host reset integration

    replace_once(
        path,
        '''    module->user_data = usbdev;

    iotype = cpu_register_io_memory''',
        '''    module->user_data = usbdev;
    usbdev->hdw_state = module->hdw_state;
    usbdev->host_fd = -1;
    hpusb_init( &usbdev->host, hpclang_usbdev_irq, usbdev );
    {
        const char* recovery = getenv( "HPCLANG_X50NG_USB_RESET_RECOVERY_TICKS" );
        if ( recovery != NULL && *recovery != '\0' ) {
            char* end = NULL;
            unsigned long ticks = strtoul( recovery, &end, 10 );
            if ( end != recovery && *end == '\0' && ticks > 0ul )
                hpusb_set_reset_recovery_ticks(
                    &usbdev->host, ( unsigned )ticks );
        }
    }

    const char* host_path = getenv( "HPCLANG_X50NG_USB0" );
    if ( host_path && *host_path ) {
        usbdev->host_fd =
            open( host_path, O_RDWR | O_NONBLOCK | O_NOCTTY );
        if ( usbdev->host_fd < 0 ) {
            fprintf(
                stderr,
                "s3c2410-usbdev: cannot open host transport %s: %s\\n",
                host_path,
                strerror( errno ) );
            return -errno;
        }

        hpusb_set_stream(
            &usbdev->host,
            hpclang_usbdev_stream_read,
            hpclang_usbdev_stream_write,
            usbdev );
        if ( hpclang_usbdev_connect_ready() )
            hpusb_set_connected( &usbdev->host, 1 );
        usbdev->next_sof = timer_get_clock() + 1000;
        usbdev->trace_state = usbdev->host.enum_state;
        usbdev->host_timer =
            timer_new(
                HDW_TIMER_REALTIME,
                hpclang_usbdev_host_poll,
                usbdev );
        if ( usbdev->host_timer == NULL )
            return -ENOMEM;
        timer_mod( usbdev->host_timer, timer_get_clock() + 100 );
    }

    iotype = cpu_register_io_memory''',
        "USB host open",
    )

    replace_once(
        path,
        '''    if ( module->user_data ) {
        usbdev = module->user_data;
        if ( usbdev->regs )
            free( usbdev->regs );
        free( usbdev );
    }''',
        '''    if ( module->user_data ) {
        usbdev = module->user_data;
        if ( usbdev->host_timer ) {
            timer_del( usbdev->host_timer );
            timer_free( usbdev->host_timer );
        }
        if ( usbdev->host_fd >= 0 )
            close( usbdev->host_fd );
        if ( usbdev->cable_asserted &&
             usbdev->hdw_state->s3c2410_io_port != NULL )
            s3c2410_io_port_f_set_bit( usbdev->hdw_state, 1, 0 );
        if ( usbdev->regs )
            free( usbdev->regs );
        free( usbdev );
    }''',
        "USB host close",
    )

def patch_host_uart0(root: Path) -> None:
    path = root / "src" / "s3c2410" / "s3c2410_uart.c"

    replace_once(
        path,
        '#include "../module.h"\n',
        '#include <errno.h>\n#include "../module.h"\n#include "../timer.h"\n',
        "UART host includes",
    )
    replace_once(
        path,
        "    hdw_t* hdw_state;\n} s3c2410_uart_reg_t;",
        """    hdw_t* hdw_state;

    int host_fd;
    hdw_timer_t* host_timer;
} s3c2410_uart_reg_t;""",
        "UART host state",
    )
    replace_once(
        path,
        "static uint32_t s3c2410_uart_read( void* opaque, target_phys_addr_t offset )\n",
        """static void s3c2410_uart_host_poll( void* opaque )
{
    s3c2410_uart_reg_t* uart_regs = opaque;
    unsigned char byte;
    ssize_t count;

    if ( uart_regs->host_fd >= 0 && !( uart_regs->utrstat & 1 ) ) {
        count = read( uart_regs->host_fd, &byte, 1 );
        if ( count == 1 ) {
            uart_regs->urxh = byte;
            uart_regs->utrstat |= 1;
            if ( uart_regs->ucon & ( 1 << 8 ) )
                s3c2410_intc_sub_assert( uart_regs->hdw_state, uart_regs->int_rxd, 1 );
            else
                s3c2410_intc_sub_assert( uart_regs->hdw_state, uart_regs->int_rxd, 0 );
        }
    }

    if ( uart_regs->host_timer )
        timer_mod( uart_regs->host_timer, timer_get_clock() + 100 );
}

static uint32_t s3c2410_uart_read( void* opaque, target_phys_addr_t offset )
""",
        "UART host poll",
    )
    replace_once(
        path,
        """        case S3C2410_UART0_UTXH:
            if ( uart_regs->ucon & ( 1 << 9 ) )""",
        """        case S3C2410_UART0_UTXH:
            if ( uart_regs->host_fd >= 0 ) {
                unsigned char byte = ( unsigned char )data;
                ( void )write( uart_regs->host_fd, &byte, 1 );
            }

            if ( uart_regs->ucon & ( 1 << 9 ) )""",
        "UART host transmit",
    )
    replace_once(
        path,
        """    cpu_register_physical_memory( S3C2410_UART0_BASE, S3C2410_MAP_SIZE, iotype );

    return 0;""",
        """    uint32_t uart_base =
        uart_regs->int_rxd == SUB_INT_RXD0 ? S3C2410_UART0_BASE :
        uart_regs->int_rxd == SUB_INT_RXD1 ? S3C2410_UART1_BASE :
                                             S3C2410_UART2_BASE;
    cpu_register_physical_memory( uart_base, 0x4000, iotype );

    if ( uart_regs->int_rxd == SUB_INT_RXD0 ) {
        const char* host_path = getenv( "HPCLANG_X50NG_UART0" );
        if ( host_path && *host_path ) {
            uart_regs->host_fd =
                open( host_path, O_RDWR | O_NONBLOCK | O_NOCTTY );
            if ( uart_regs->host_fd < 0 ) {
                fprintf(
                    stderr,
                    "s3c2410-uart0: cannot open host transport %s: %s\\n",
                    host_path,
                    strerror( errno ) );
                return -errno;
            }
            uart_regs->host_timer =
                timer_new( HDW_TIMER_REALTIME, s3c2410_uart_host_poll, uart_regs );
            if ( uart_regs->host_timer == NULL )
                return -ENOMEM;
            timer_mod( uart_regs->host_timer, timer_get_clock() + 100 );
        }
    }

    return 0;""",
        "UART host open",
    )
    replace_once(
        path,
        """    if ( module->user_data ) {
        uart_regs = module->user_data;
        if ( uart_regs->regs )
            free( uart_regs->regs );
    }""",
        """    if ( module->user_data ) {
        uart_regs = module->user_data;
        if ( uart_regs->host_timer ) {
            timer_del( uart_regs->host_timer );
            timer_free( uart_regs->host_timer );
        }
        if ( uart_regs->host_fd >= 0 )
            close( uart_regs->host_fd );
        if ( uart_regs->regs )
            free( uart_regs->regs );
    }""",
        "UART host close",
    )
    replace_once(
        path,
        """    memset( uart, 0, sizeof( s3c2410_uart_t ) );

    if ( s3c2410_uart_data_init( uart ) ) {""",
        """    memset( uart, 0, sizeof( s3c2410_uart_t ) );
    for ( int i = 0; i < 3; ++i )
        uart->uart[ i ].host_fd = -1;

    if ( s3c2410_uart_data_init( uart ) ) {""",
        "UART host fd init",
    )


def patch_usb_active_store_trace(root: Path) -> None:
    path = root / "src" / "qemu" / "softmmu_header.h"

    replace_once(
        path,
        "/* generic store macro */\n\n"
        "static inline void glue(glue(st, SUFFIX), MEMSUFFIX)(target_ulong ptr, RES_TYPE v)\n"
        "{\n"
        "    int page_index;\n"
        "    target_ulong addr;\n",
        "/* generic store macro */\n\n"
        "static inline void glue(glue(st, SUFFIX), MEMSUFFIX)(target_ulong ptr, RES_TYPE v)\n"
        "{\n"
        "    int page_index;\n"
        "    target_ulong addr;\n",
        "USB active store trace anchor",
    )

    text = path.read_text()
    needle = (
        "    addr = ptr;\n"
        "    page_index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);\n"
    )
    replacement = (
        "    addr = ptr;\n"
        "    if (getenv(\"HPCLANG_X50NG_USB_TRACE_STATE\") != NULL &&\n"
        "        addr <= 0x08000cf9u &&\n"
        "        addr + DATA_SIZE > 0x08000cf9u) {\n"
        "        fprintf(stderr,\n"
        "                \"hpclang-usb-active-store: addr=0x%08lx size=%u value=0x%016llx \"\n"
        "                \"pc=0x%08x lr=0x%08x\\n\",\n"
        "                (unsigned long)addr, (unsigned)DATA_SIZE,\n"
        "                (unsigned long long)v, env->regs[15], env->regs[14]);\n"
        "    }\n"
        "    page_index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);\n"
    )
    if needle not in text:
        raise SystemExit(f"{path}: active-store trace insertion anchor missing")
    path.write_text(text.replace(needle, replacement, 1))


def patch_reserved_ram(root: Path) -> None:
    replace_once(
        root / "src" / "module.c",
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "    /* hpclang E2E reserved RAM, inert unless the Direct harness maps it. */\n"
        "    phys_ram_size += 0x00100000;\n\n"
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "reserved E2E RAM",
    )


def patch_frontends(root: Path, args: argparse.Namespace) -> None:
    copy_frontend(args.direct_frontend, root / "src" / "hpclang_direct.c", "hpclang_direct_main")
    copy_frontend(args.stock_frontend, root / "src" / "hpclang_stock.c", "hpclang_stock_main")
    shutil.copy2(args.selector, root / "src" / "main.c")
    shutil.copy2(args.headless_header, root / "src" / "headless-ui-init.h")
    shutil.copy2(
        args.stock_frontend.parent / "stock-return-phase.h",
        root / "src" / "stock-return-phase.h",
    )

    makefile = root / "Makefile"
    lines = makefile.read_text().splitlines(keepends=True)
    matches = []
    for index, line in enumerate(lines):
        logical = line.strip()
        if logical.endswith("\\"):
            logical = logical[:-1].rstrip()
        if logical.endswith("src/main.c"):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one src/main.c source line, found {len(matches)}"
        )

    index = matches[0]
    original = lines[index]
    leading = original[: len(original) - len(original.lstrip())]
    continuation = chr(92)
    lines[index:index + 1] = [
        f"{leading}./src/hpclang_direct.c {continuation}\n",
        f"{leading}./src/hpclang_stock.c {continuation}\n",
        original,
    ]
    makefile.write_text("".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--direct-frontend", type=Path, required=True)
    parser.add_argument("--stock-frontend", type=Path, required=True)
    parser.add_argument("--selector", type=Path, required=True)
    parser.add_argument("--headless-header", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    patch_frontends(root, args)
    verify_ui_build_inheritance(root)
    patch_universal_flash(root)
    patch_runtime_sram(root)
    patch_host_usbdev(root, args)
    patch_host_uart0(root)
    patch_usb_active_store_trace(root)
    patch_reserved_ram(root)


if __name__ == "__main__":
    main()
