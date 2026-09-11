.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.tamzen_powerline7x13.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_tamzen_powerline7x13_cp\cp\()_bitmap
hpclang_font_tamzen_powerline7x13_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.tamzen_powerline7x13.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline7x13_cp\cp
hpclang_font_tamzen_powerline7x13_cp\cp:
.word \cp
.word hpclang_font_tamzen_powerline7x13_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 57504,0,-2,7,13,7,1,128,136,156,190,136,136,152,48,96,192,192,192,192
G 57505,0,-2,7,13,7,1,0,128,128,128,128,240,0,34,50,42,38,34,0
G 57506,0,-2,7,13,7,1,56,68,68,68,68,254,254,250,246,234,214,170,254
G 57520,0,-2,7,13,7,1,128,192,224,240,248,252,254,252,248,240,224,192,128
G 57521,0,-2,7,13,7,1,128,64,32,16,8,4,2,4,8,16,32,64,128
G 57522,0,-2,7,13,7,1,2,6,14,30,62,126,254,126,62,30,14,6,2
G 57523,0,-2,7,13,7,1,2,4,8,16,32,64,128,64,32,16,8,4,2

.purgem G

.syntax unified

.section .rodata.hpclang.font.tamzen_powerline7x13.full.map,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline7x13_full_glyphs
hpclang_font_tamzen_powerline7x13_full_glyphs:
.word hpclang_font_tamzen_powerline7x13_cp57504
.word hpclang_font_tamzen_powerline7x13_cp57505
.word hpclang_font_tamzen_powerline7x13_cp57506
.word hpclang_font_tamzen_powerline7x13_cp57520
.word hpclang_font_tamzen_powerline7x13_cp57521
.word hpclang_font_tamzen_powerline7x13_cp57522
.word hpclang_font_tamzen_powerline7x13_cp57523

.section .rodata.hpclang.font.tamzen_powerline7x13.full.data,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline7x13_full_data
hpclang_font_tamzen_powerline7x13_full_data:
.word hpclang_font_tamzen_powerline7x13_full_glyphs
.word 7
.word 57504
.hword 11,2,13,7

.section .rodata.hpclang.font.tamzen_powerline7x13.full.font,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline7x13_full
hpclang_font_tamzen_powerline7x13_full:
.word 1,1
.word hpclang_font_tamzen_powerline7x13_full_data
