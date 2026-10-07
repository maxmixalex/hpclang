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
        """memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP39G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP40G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP48GIIMAGE", 16 ) != 0""",
        "universal HP39/40/48 firmware headers",
    )


def patch_runtime_sram(root: Path) -> None:
    path = root / "src" / "sram.c"

    replace_once(
        path,
        "#define S3C2410_SRAM_SIZE 0x00080000\n",
        """#define S3C2410_SRAM_DEFAULT_SIZE 0x00080000
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
""",
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
        """    sram->data = ( void* )-1;
    sram->shadow = ( void* )-1;
    sram->offset = phys_ram_size;
    phys_ram_size += S3C2410_SRAM_SIZE;
    phys_ram_size += S3C2410_SRAM_SIZE;

    cpu_register_physical_memory( S3C2410_SRAM_BASE, S3C2410_SRAM_SIZE, sram->offset | IO_MEM_RAM );
    cpu_register_physical_memory( S3C2410_SRAM_BASE + S3C2410_SRAM_SIZE, S3C2410_SRAM_SIZE,
                                  ( sram->offset + S3C2410_SRAM_SIZE ) | IO_MEM_RAM );""",
        """    sram->data = ( void* )-1;
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
                                  ( sram->offset + sram->size ) | IO_MEM_RAM );""",
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
        """#include <errno.h>
#include "../module.h"
#include "../timer.h"
#include "../qemu/exec-all.h"

#include "s3c2410.h"
#include "s3c2410_intc.h"
#include "s3c2410_io_port.h"
#include "hpclang-usbdev-host.h"
#include "hpclang-usbdev-host.c"
""",
        "USB host includes",
    )

    replace_once(
        path,
        """    unsigned int nr_regs;
    s3c2410_offset_t* regs;
} s3c2410_usbdev_t;""",
        """    unsigned int nr_regs;
    s3c2410_offset_t* regs;

    struct hpusb_core host;
    hdw_t* hdw_state;
    int host_fd;
    hdw_timer_t* host_timer;
    int64_t next_sof;
    enum hpusb_enum_state trace_state;
    int cable_asserted;
} s3c2410_usbdev_t;""",
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
    if (
        read_start < 0
        or write_start < 0
        or load_start < 0
        or not (read_start < write_start < load_start)
    ):
        raise SystemExit(f"{path}: USB read/write function anchors changed")

    text = path.read_text()
    host_impl = r"""static void hpclang_usbdev_irq( void* opaque, int level )
{
    s3c2410_usbdev_t* usbdev = opaque;
    if ( level )
        s3c2410_intc_assert( usbdev->hdw_state, INT_USBD, 1 );
    else
        s3c2410_intc_deassert( usbdev->hdw_state, INT_USBD );
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

static void hpclang_usbdev_host_poll( void* opaque )
{
    s3c2410_usbdev_t* usbdev = opaque;

    if ( !usbdev->cable_asserted &&
         usbdev->hdw_state->s3c2410_io_port != NULL ) {
        s3c2410_io_port_f_set_bit( usbdev->hdw_state, 1, 1 );
        usbdev->cable_asserted = 1;
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

"""
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
        """    module->user_data = usbdev;

    iotype = cpu_register_io_memory""",
        """    module->user_data = usbdev;
    usbdev->hdw_state = module->hdw_state;
    usbdev->host_fd = -1;
    hpusb_init( &usbdev->host, hpclang_usbdev_irq, usbdev );

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

    iotype = cpu_register_io_memory""",
        "USB host open",
    )

    replace_once(
        path,
        """    if ( module->user_data ) {
        usbdev = module->user_data;
        if ( usbdev->regs )
            free( usbdev->regs );
        free( usbdev );
    }""",
        """    if ( module->user_data ) {
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
    }""",
        "USB host close",
    )


def patch_rtc_clock(root: Path) -> None:
    """Model writable calculator civil time independently of host wall-clock reads."""
    project = Path(__file__).resolve().parent.parent
    support = project / "tests/e2e/harness/x50ng"
    for name in ("rtc-clock.c", "rtc-clock.h"):
        shutil.copyfile(support / name, root / "src" / name)
    (root / "src/hpclang").mkdir(exist_ok=True)
    shutil.copyfile(project / "include/hpclang/datetime.h", root / "src/hpclang/datetime.h")
    for name in ("datetime.c", "datetime-internal.h", "rtc-decode.c"):
        shutil.copyfile(project / "runtime/core" / name, root / "src" / name)
    replace_once(
        root / "Makefile",
        "\tsrc/s3c2410/s3c2410_rtc.c \\\n",
        "\tsrc/s3c2410/s3c2410_rtc.c \\\n\tsrc/rtc-clock.c \\\n\tsrc/datetime.c \\\n\tsrc/rtc-decode.c \\\n",
        "RTC clock sources",
    )
    path = root / "src/s3c2410/s3c2410_rtc.c"
    replace_once(
        path,
        '#include "../module.h"',
        '#include "../module.h"\n#include "../rtc-clock.h"',
        "RTC clock include",
    )
    replace_once(
        path,
        "    uint32_t rtccon;",
        "    struct hpclang_rtc_clock clock;\n    uint32_t rtccon;",
        "RTC clock state",
    )
    replace_once(
        path,
        "    memset( rtc, 0, sizeof( s3c2410_rtc_t ) );",
        """    memset( rtc, 0, sizeof( s3c2410_rtc_t ) );
    time_t initial_time = time( NULL );
    struct tm initial = *localtime( &initial_time );
    struct hpclang_datetime civil = {
        initial.tm_year + 1900, initial.tm_mon + 1, initial.tm_mday,
        initial.tm_hour, initial.tm_min, initial.tm_sec
    };
    hpclang_rtc_clock_init( &rtc->clock, &civil, timer_get_clock() );""",
        "RTC clock initialization",
    )
    text = path.read_text()
    start = text.index("    if ( S3C2410_RTC_BCDSEC <= offset")
    end = text.index("\n#ifdef DEBUG_S3C2410_RTC", start)
    text = (
        text[:start]
        + """    if ( S3C2410_RTC_BCDSEC <= offset && offset <= S3C2410_RTC_BCDYEAR ) {
        uint8_t raw[7];
        hpclang_rtc_clock_read( &rtc->clock, timer_get_clock(), raw );
        *( reg->datap ) = raw[(offset - S3C2410_RTC_BCDSEC) / 4];
    }
"""
        + text[end:]
    )
    path.write_text(text)
    replace_once(
        path,
        "    switch ( offset ) {\n        case S3C2410_RTC_TICNT:",
        """    if ( S3C2410_RTC_BCDSEC <= offset && offset <= S3C2410_RTC_BCDYEAR ) {
        if ( rtc->rtccon & 1 ) {
            hpclang_rtc_clock_write( &rtc->clock,
                (offset - S3C2410_RTC_BCDSEC) / 4, data, timer_get_clock() );
            *( reg->datap ) = data;
        }
        return;
    }
    switch ( offset ) {
        case S3C2410_RTC_RTCCON:
            hpclang_rtc_clock_stop( &rtc->clock, !!(data & 8), timer_get_clock() );
            *( reg->datap ) = data;
            break;
        case S3C2410_RTC_TICNT:""",
        "RTC calendar writes",
    )
    replace_once(
        path,
        "    tm = localtime( &tv.tv_sec );",
        """    uint8_t calendar[7];
    hpclang_rtc_clock_read( &rtc->clock, timer_get_clock(), calendar );
    struct tm calendar_tm = {0};
    calendar_tm.tm_sec = bcd2bin(calendar[0]);
    calendar_tm.tm_min = bcd2bin(calendar[1]);
    calendar_tm.tm_hour = bcd2bin(calendar[2]);
    calendar_tm.tm_mday = bcd2bin(calendar[3]);
    calendar_tm.tm_mon = bcd2bin(calendar[5]) - 1;
    calendar_tm.tm_year = bcd2bin(calendar[6]) + 100;
    tm = &calendar_tm;""",
        "RTC alarm calendar",
    )

    replace_once(
        path,
        "    s3c2410_rtc_set_ticnt( rtc );\n    s3c2410_rtc_set_rtcalm( rtc );\n\n    return error;",
        """    uint32_t version, seconds_lo, seconds_hi, anchor_lo, anchor_hi, valid;
    if ( module_get_u32(module, key, "hpclang_clock_version", 0, &version) == 0 && version == 1 &&
         module_get_u32(module, key, "hpclang_clock_seconds_lo", 0, &seconds_lo) == 0 &&
         module_get_u32(module, key, "hpclang_clock_seconds_hi", 0, &seconds_hi) == 0 &&
         module_get_u32(module, key, "hpclang_clock_anchor_lo", 0, &anchor_lo) == 0 &&
         module_get_u32(module, key, "hpclang_clock_anchor_hi", 0, &anchor_hi) == 0 &&
         module_get_u32(module, key, "hpclang_clock_valid", 0, &valid) == 0 ) {
        rtc->clock.seconds = (int64_t)(((uint64_t)seconds_hi << 32) | seconds_lo);
        rtc->clock.anchor_us = (int64_t)(((uint64_t)anchor_hi << 32) | anchor_lo);
        rtc->clock.valid = !!valid;
        rtc->clock.stopped = !!(rtc->rtccon & 8);
        uint8_t raw[7] = {rtc->bcdsec, rtc->bcdmin, rtc->bcdhour, rtc->bcddate,
                          rtc->bcdday, rtc->bcdmon, rtc->bcdyear};
        memcpy(rtc->clock.raw, raw, sizeof(raw));
        rtc->clock.weekday = raw[4];
    }
    s3c2410_rtc_set_ticnt( rtc );
    s3c2410_rtc_set_rtcalm( rtc );

    return error;""",
        "RTC saved clock load",
    )
    replace_once(
        path,
        "static int s3c2410_rtc_save( hdw_module_t* module, GKeyFile* key )\n{\n    s3c2410_rtc_t* rtc = module->user_data;",
        """static int s3c2410_rtc_save( hdw_module_t* module, GKeyFile* key )
{
    s3c2410_rtc_t* rtc = module->user_data;
    uint8_t raw[7];
    int64_t now_us = timer_get_clock();
    hpclang_rtc_clock_read(&rtc->clock, now_us, raw);
    hpclang_rtc_clock_write(&rtc->clock, 0, raw[0], now_us);
    rtc->bcdsec = raw[0]; rtc->bcdmin = raw[1]; rtc->bcdhour = raw[2];
    rtc->bcddate = raw[3]; rtc->bcdday = raw[4]; rtc->bcdmon = raw[5]; rtc->bcdyear = raw[6];
    module_set_u32(module, key, "hpclang_clock_version", 1);
    module_set_u32(module, key, "hpclang_clock_seconds_lo", (uint32_t)rtc->clock.seconds);
    module_set_u32(module, key, "hpclang_clock_seconds_hi", (uint64_t)rtc->clock.seconds >> 32);
    module_set_u32(module, key, "hpclang_clock_anchor_lo", (uint32_t)rtc->clock.anchor_us);
    module_set_u32(module, key, "hpclang_clock_anchor_hi", (uint64_t)rtc->clock.anchor_us >> 32);
    module_set_u32(module, key, "hpclang_clock_valid", rtc->clock.valid);""",
        "RTC saved clock store",
    )
    replace_once(
        path,
        "    s3c2410_rtc_set_ticnt( rtc );\n\n    return 0;",
        "    hpclang_rtc_clock_stop(&rtc->clock, !!(rtc->rtccon & 8), timer_get_clock());\n    s3c2410_rtc_set_ticnt( rtc );\n\n    return 0;",
        "RTC reset divider state",
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


def patch_reserved_ram(root: Path) -> None:
    replace_once(
        root / "src" / "module.c",
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "    /* hpclang E2E reserved RAM, inert unless the Direct harness maps it. */\n"
        "    phys_ram_size += 0x00200000;\n\n"
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "reserved E2E RAM",
    )


def patch_optional_state_logging(root: Path) -> None:
    path = root / "src" / "module.c"
    old = '        fprintf( stderr, "%s: %s:%u: key \\"%s\\" not found\\n", module->name, __func__, __LINE__, name );'
    new = """        /* A fresh E2E state uses the register reset defaults. Missing
           optional keys/groups are diagnostic detail, not startup errors. */
        if ( ui4x_config.verbose ||
             !( g_error_matches( gerror, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND ) ||
                g_error_matches( gerror, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND ) ) )
            fprintf( stderr, "%s: state key \\"%s\\": %s\\n", module->name, name,
                     gerror ? gerror->message : "lookup failed" );
        g_clear_error( &gerror );"""
    if path.read_text().count(old) != 2:
        raise SystemExit(f"{path}: expected the integer and string state lookup diagnostics")
    for kind in ("integer", "string"):
        replace_once(path, old, new, f"optional {kind} state logging")


def patch_frontends(root: Path, args: argparse.Namespace) -> None:
    copy_frontend(args.direct_frontend, root / "src" / "hpclang_direct.c", "hpclang_direct_main")
    copy_frontend(args.stock_frontend, root / "src" / "hpclang_stock.c", "hpclang_stock_main")
    copy_frontend(
        args.gallery_frontend,
        root / "src" / "hpclang_gallery.c",
        "hpclang_gallery_main",
    )
    shutil.copy2(args.selector, root / "src" / "main.c")
    shutil.copy2(args.headless_header, root / "src" / "headless-ui-init.h")
    shutil.copy2(args.stage_layout, root / "src" / "stage-layout.h")
    for header in (
        "stock-return-phase.h",
        "lcd-quiescence.h",
        "event-queue.h",
        "timed-sequence.h",
        "ready-window.h",
        "key-sample.h",
        "key-sequence.h",
        "key-delivery.h",
        "lcd-stability.h",
        "frame-checkpoint.h",
    ):
        shutil.copy2(args.stock_frontend.parent / header, root / "src" / header)

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
        raise SystemExit(f"{makefile}: expected one src/main.c source line, found {len(matches)}")

    index = matches[0]
    original = lines[index]
    leading = original[: len(original) - len(original.lstrip())]
    continuation = chr(92)
    lines[index : index + 1] = [
        f"{leading}./src/hpclang_direct.c {continuation}\n",
        f"{leading}./src/hpclang_stock.c {continuation}\n",
        f"{leading}./src/hpclang_gallery.c {continuation}\n",
        original,
    ]
    makefile.write_text("".join(lines))


def patch_key_observation(root: Path) -> None:
    """Expose actual guest GPIO sampling to the Stock input sequencer."""
    api = root / "src/emulator_api.c"
    replace_once(
        api,
        '#include "emulator_api.h"',
        '#include "emulator_api.h"\n#include "key-sample.h"',
        "key sampling helper",
    )
    replace_once(
        api,
        "static void set_key( int hpkey, bool state )",
        """static unsigned key_observed[sizeof(x50ng_keyboard) / sizeof(x50ng_keyboard[0])];

unsigned emulator_key_observed(int key) { return key_observed[key]; }

int emulator_key_ready(int key)
{
    hp50g_key_t k = x50ng_keyboard[key];
    return s3c2410_io_port_key_ready(__hdw_state, k.column, k.row, k.eint);
}

void emulator_observe_keys(int port, uint32_t config, uint32_t value)
{
    for (unsigned i = 0; i < sizeof(x50ng_keyboard) / sizeof(x50ng_keyboard[0]); ++i) {
        hp50g_key_t key = x50ng_keyboard[i];
        if (hpclang_key_sampled(port, config, value, key.column, key.row, key.eint, key.pressed))
            ++key_observed[i];
    }
}

static void set_key( int hpkey, bool state )""",
        "guest key observation",
    )
    replace_once(
        api,
        "    set_key_state( x50ng_keyboard[ hpkey ], state );",
        "    key_observed[hpkey] = 0;\n    set_key_state( x50ng_keyboard[ hpkey ], state );",
        "key observation reset",
    )
    header = root / "src/emulator_api.h"
    replace_once(
        header,
        "extern void press_key( int hpkey );",
        "extern int emulator_key_ready(int key);\n"
        "extern unsigned emulator_key_observed(int key);\n"
        "extern void emulator_observe_keys(int port, uint32_t config, uint32_t value);\n"
        "extern void press_key( int hpkey );",
        "key observation API",
    )
    gpio_header = root / "src/s3c2410/s3c2410_io_port.h"
    with gpio_header.open("a") as f:
        f.write("\nextern int s3c2410_io_port_key_ready(hdw_t *, unsigned, unsigned, int);\n")
    gpio = root / "src/s3c2410/s3c2410_io_port.c"
    replace_once(
        gpio,
        "static void s3c2410_io_port_write(",
        """int s3c2410_io_port_key_ready(hdw_t *hdw, unsigned column, unsigned row, int eint)
{
    s3c2410_io_port_t *io = hdw->s3c2410_io_port;
    return eint >= 0
        ? hpclang_key_ready(0, io->gpfcon, io->gpfdat, column, row, eint)
        : hpclang_key_ready(1, io->gpgcon, io->gpgdat, column, row, eint);
}

static void s3c2410_io_port_write(""",
        "key input readiness",
    )
    replace_once(
        gpio,
        '#include "../module.h"',
        '#include "../module.h"\n#include "../emulator_api.h"\n#include "../key-sample.h"',
        "GPIO key observer include",
    )
    replace_once(
        gpio,
        """        case S3C2410_IO_PORT_GPGDAT:
            return s3c2410_scan_keys( io->hdw_state, io->gpgcon, io->gpgdat );""",
        """        case S3C2410_IO_PORT_GPGDAT: {
            uint32_t value = s3c2410_scan_keys(io->hdw_state, io->gpgcon, io->gpgdat);
            emulator_observe_keys(1, io->gpgcon, value);
            return value;
        }""",
        "guest matrix read observation",
    )
    replace_once(
        gpio,
        """        case S3C2410_IO_PORT_GPFDAT:
            if""",
        """        case S3C2410_IO_PORT_GPFDAT:
            emulator_observe_keys(0, io->gpfcon, io->gpfdat);
            if""",
        "guest dedicated key read observation",
    )


def patch_lcd_scanout(root: Path) -> None:
    """Decode STN pixels from mode/pitch, independent of power/polarity bits."""
    path = root / "src/s3c2410/s3c2410_lcd.c"
    replace_once(
        path,
        "int bits_per_pixel = lcd->lcdcon5 > 2 ? 1 : 4 >> lcd->lcdcon5;",
        """unsigned mode = (lcd->lcdcon1 >> 1) & 15u;
    if (mode > 2u || x < 0 || y < 0)
        return 0;
    unsigned bits_per_pixel = 1u << mode;
    unsigned pagewidth = lcd->lcdsaddr3 & 2047u;
    unsigned offsize = (lcd->lcdsaddr3 >> 11) & 2047u;
    if ((unsigned)x >= pagewidth * 16u / bits_per_pixel ||
        (unsigned)y > ((lcd->lcdcon2 >> 14) & 1023u))
        return 0;""",
        "LCD STN mode decoding",
    )
    replace_once(
        path,
        "pixel_offset = ( 160 * y + x ) * bits_per_pixel;",
        "pixel_offset = (pagewidth + offsize) * 16u * (unsigned)y + (unsigned)x * bits_per_pixel;",
        "LCD DMA row pitch",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--direct-frontend", type=Path, required=True)
    parser.add_argument("--stock-frontend", type=Path, required=True)
    parser.add_argument("--gallery-frontend", type=Path, required=True)
    parser.add_argument("--selector", type=Path, required=True)
    parser.add_argument("--headless-header", type=Path, required=True)
    parser.add_argument("--stage-layout", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    patch_frontends(root, args)
    verify_ui_build_inheritance(root)
    patch_universal_flash(root)
    patch_runtime_sram(root)
    patch_host_usbdev(root, args)
    patch_host_uart0(root)
    patch_rtc_clock(root)
    patch_reserved_ram(root)
    patch_optional_state_logging(root)
    patch_key_observation(root)
    patch_lcd_scanout(root)


if __name__ == "__main__":
    main()
