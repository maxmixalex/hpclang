OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS
{
    image PT_LOAD FLAGS(5); /* R|X */
    rw    PT_LOAD FLAGS(6); /* R|W */
    tls   PT_TLS  FLAGS(4); /* R */
}

SECTIONS
{
    . = 0x01220000;
    __code_start = .;

    .text :
    {
        KEEP(*(.text.start))

        *(.text.unlikely .text.unlikely.*)
        *(.text.startup .text.startup.*)
        *(.text .text.*)
        *(.gnu.linkonce.t.*)

        *(.rdata)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
        *(.srodata .srodata.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.got .got.*)

        . = ALIGN(8);

        __preinit_array_start = .;
        KEEP(*(.preinit_array))
        __preinit_array_end = .;

        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;

        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        __fini_array_end = .;

        KEEP(*(.fini .fini.*))
        . = ALIGN(8);
    } :image

    /*
     * Writable initializers live in the HPC1 code image but execute from the
     * fixed writable arena.
     */
    __data_load = .;

    .data 0x01101000 : AT(__data_load)
    {
        __data_start = .;
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        *(.sdata .sdata.* .sdata2.*)
        *(.gnu.linkonce.s.*)
        . = ALIGN(8);
    } :rw

    __tdata_load = LOADADDR(.data) + SIZEOF(.data);

    .tdata : AT(__tdata_load)
    {
        __tls_base = .;
        __tdata_start = .;
        *(.tdata .tdata.* .gnu.linkonce.td.*)
        . = ALIGN(8);
        __tdata_end = .;
        __data_end = .;
    } :rw :tls

    __data_size = __data_end - __data_start;
    __tdata_size = SIZEOF(.tdata);

    .tbss (NOLOAD) :
    {
        __bss_start = .;
        __tbss_start = .;
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)
        __tbss_end = .;
        __tls_end = .;
    } :rw :tls

    __tbss_size = SIZEOF(.tbss);
    __tls_size = __tls_end - __tls_base;
    __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss));
    __arm32_tls_tcb_offset = MAX(8, __tls_align);

    /*
     * ELF TLS NOBITS does not advance the normal location counter. Reserve its
     * bytes explicitly so regular BSS starts after the static TLS block.
     */
    .tbss_space (NOLOAD) :
    {
        . = ADDR(.tbss) + SIZEOF(.tbss);
    } :rw

    .bss (NOLOAD) :
    {
        *(.sbss .sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } :rw

    __rw_static_end = __bss_end;
    __rw_static_size = __rw_static_end - 0x01101000;

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
        *(.note .note.*)
        *(.note.GNU-stack)
        *(.comment)
    }
}

ASSERT(
    SIZEOF(.data) + SIZEOF(.tdata) == __data_size,
    "initialized RAM sections must form one contiguous copy range"
)
