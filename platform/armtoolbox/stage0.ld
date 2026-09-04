OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(hpclang_armtoolbox_stage0)

PHDRS
{
    image PT_LOAD FLAGS(5);
}

SECTIONS
{
    . = 0;

    .image :
    {
        KEEP(*(.text.stage0.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        KEEP(*(.rodata.hpclang.bundle))
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
