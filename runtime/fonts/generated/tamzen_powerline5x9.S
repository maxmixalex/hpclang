.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.tamzen_powerline5x9.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_tamzen_powerline5x9_cp\cp\()_bitmap
hpclang_font_tamzen_powerline5x9_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.tamzen_powerline5x9.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline5x9_cp\cp
hpclang_font_tamzen_powerline5x9_cp\cp:
.word \cp
.word hpclang_font_tamzen_powerline5x9_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 57504,0,-2,5,9,5,1,128,144,184,144,144,48,96,192,192
G 57505,0,-2,5,9,5,1,128,128,128,224,0,72,104,88,72
G 57506,0,-2,5,9,5,1,32,80,80,80,248,232,216,168,248
G 57520,0,-2,5,9,5,1,128,192,224,240,248,240,224,192,128
G 57521,0,-2,5,9,5,1,128,64,32,16,8,16,32,64,128
G 57522,0,-2,5,9,5,1,8,24,56,120,248,120,56,24,8
G 57523,0,-2,5,9,5,1,8,16,32,64,128,64,32,16,8

.purgem G

.syntax unified

.section .rodata.hpclang.font.tamzen_powerline5x9.full.map,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline5x9_full_glyphs
hpclang_font_tamzen_powerline5x9_full_glyphs:
.word hpclang_font_tamzen_powerline5x9_cp57504
.word hpclang_font_tamzen_powerline5x9_cp57505
.word hpclang_font_tamzen_powerline5x9_cp57506
.word hpclang_font_tamzen_powerline5x9_cp57520
.word hpclang_font_tamzen_powerline5x9_cp57521
.word hpclang_font_tamzen_powerline5x9_cp57522
.word hpclang_font_tamzen_powerline5x9_cp57523

.section .rodata.hpclang.font.tamzen_powerline5x9.full.data,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline5x9_full_data
hpclang_font_tamzen_powerline5x9_full_data:
.word hpclang_font_tamzen_powerline5x9_full_glyphs
.word 7
.word 57504
.hword 7,2,9,5

.section .rodata.hpclang.font.tamzen_powerline5x9.full.font,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline5x9_full
hpclang_font_tamzen_powerline5x9_full:
.word 1,1
.word hpclang_font_tamzen_powerline5x9_full_data
