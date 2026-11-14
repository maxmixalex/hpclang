ENTRY(entry)
SECTIONS { . = 0; .text : { *(.entry) *(.text*) } /DISCARD/ : { *(.ARM.exidx*) *(.comment) } }
