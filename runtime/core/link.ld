OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS
{
    image PT_LOAD FLAGS(5); /* R|X */
    rw    PT_LOAD FLAGS(6); /* R|W */
}

SECTIONS
{
    . = 0x01220000;
    __code_start = .;

    .text :
    {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } :image

    /*
     * Initialized writable bytes are stored immediately after the executable
     * image but execute from the fixed writable arena.
     */
    __data_load = .;

    .data 0x01101000 : AT(__data_load)
    {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } :rw

    .bss :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } :rw

    __rw_static_end = .;
    __rw_static_size = __rw_static_end - 0x01101000;

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.note.GNU-stack)
        *(.comment)
    }
}
