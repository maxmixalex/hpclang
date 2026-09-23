OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(hp39_rom_dump)

PHDRS
{
    image PT_LOAD FLAGS(5);
}

SECTIONS
{
    . = 0;
    .image :
    {
        KEEP(*(.text.rom_dump))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } :image

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
