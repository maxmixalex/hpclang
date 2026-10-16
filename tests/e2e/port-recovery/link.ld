/* This image is copied into CODE-owned RAM at an address chosen by Stock. */
ENTRY(entry) SECTIONS {
 . = 0; .text : { *(.entry) *(.text*) *(.rodata*) *(.data*) *(.got*) }
 .tdata : { __tls_base = .; *(.tdata*) }
 .tbss (NOLOAD) : { __zero_start = .; *(.tbss*) __tls_end = .; }
 . = ADDR(.tbss)+SIZEOF(.tbss);
 .bss (NOLOAD) : { *(.bss*) . = ALIGN(8); __image_end = .; }
 .ARM.attributes 0 : { *(.ARM.attributes) }
 .symtab 0 : { *(.symtab) }
 .shstrtab 0 : { *(.shstrtab) }
 .strtab 0 : { *(.strtab) }
 /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
 ASSERT(entry == 0, "Recovery entry must be first")
 ASSERT((__zero_start & 3) == 0, "BSS clear requires word alignment")
 ASSERT((__image_end & 7) == 0, "Recovery image requires eight-byte alignment")
}
