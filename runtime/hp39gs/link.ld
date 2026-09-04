OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_loader)

/*
 * Clean hp39gs image model:
 *
 *   VMA 0                 ROPI code/rodata
 *   VMA 0x09000400        RWPI data/bss
 *
 * __data_load is an offset inside the packed ARM payload, not a runtime
 * absolute address. loader.S adds the actual image base obtained with ADR.
 */
PHDRS
{
    text PT_LOAD FLAGS(5); /* R|X */
    ram  PT_LOAD FLAGS(6); /* R|W */
}

SECTIONS
{
    . = 0;
    __code_start = .;

    .text :
    {
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(4);
    __data_load = .;

    .data 0x09000400 : AT(__data_load)
    {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } :ram

    __data_size = SIZEOF(.data);

    .bss :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } :ram

    __bss_size = SIZEOF(.bss);
    _end = .;

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.note.GNU-stack)
        *(.comment)
    }
}
