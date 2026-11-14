ENTRY(entry)
SECTIONS { . = 0; .text : { *(.entry) *(.text*) *(.rodata*) } /DISCARD/ : { *(.ARM.exidx*) *(.comment) } }
