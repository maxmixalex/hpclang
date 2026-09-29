OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(hpclang_armtoolbox_stage1_entry)

SECTIONS
{
    . = 0x09000400;

    /* The raw image is copied at this VMA; preserve section alignment gaps. */
    .text :
    {
        KEEP(*(.text.stage1.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }

    .data :
    {
        *(.data .data.*)
        . = ALIGN(8);
    }

    .bss (NOLOAD) :
    {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
    }

    __stage1_end = .;
    __stage1_size = __stage1_end - 0x09000400;

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
