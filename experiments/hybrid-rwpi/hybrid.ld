OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_loader)

/*
 * Minimal research linker script.
 *
 * The key experiment is not the exact list of sections; it is the single RAM
 * PT_LOAD rooted at the historical HPGCC static base, 0x09000400.
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
    *(.text .text.*)
  } :text

  . = ALIGN(4);
  __got_start = .;

  .got 0x09000400 : AT(__got_start)
  {
    *(.got.plt)
    *(.got)
  } :ram

  __got_size = . - 0x09000400;
  __data_start = LOADADDR(.got) + SIZEOF(.got);
  __tmp_datastart = .;

  .rodata :
  {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  } :ram

  . = ALIGN(4);

  .data :
  {
    *(.data .data.* .gnu.linkonce.d.*)
    . = ALIGN(4);
  } :ram

  __data_size = . - __tmp_datastart;
  __bss_start = .;

  .bss :
  {
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
  } :ram

  __bss_size = SIZEOF(.bss);
  _end = .;

  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.note.GNU-stack)
  }
}
