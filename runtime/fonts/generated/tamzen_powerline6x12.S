.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.tamzen_powerline6x12.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_tamzen_powerline6x12_cp\cp\()_bitmap
hpclang_font_tamzen_powerline6x12_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.tamzen_powerline6x12.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline6x12_cp\cp
hpclang_font_tamzen_powerline6x12_cp\cp:
.word \cp
.word hpclang_font_tamzen_powerline6x12_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 57504,0,-2,6,12,6,1,192,192,200,220,200,200,152,48,96,192,192,192
G 57505,0,-2,6,12,6,1,128,128,128,128,128,240,0,68,100,84,76,68
G 57506,0,-2,6,12,6,1,48,72,72,72,72,252,252,244,236,212,172,252
G 57520,0,-2,6,12,6,1,128,192,224,240,248,252,252,248,240,224,192,128
G 57521,0,-2,6,12,6,1,128,64,32,16,8,4,4,8,16,32,64,128
G 57522,0,-2,6,12,6,1,4,12,28,60,124,252,252,124,60,28,12,4
G 57523,0,-2,6,12,6,1,4,8,16,32,64,128,128,64,32,16,8,4

.purgem G

.syntax unified

.section .rodata.hpclang.font.tamzen_powerline6x12.full.map,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline6x12_full_glyphs
hpclang_font_tamzen_powerline6x12_full_glyphs:
.word hpclang_font_tamzen_powerline6x12_cp57504
.word hpclang_font_tamzen_powerline6x12_cp57505
.word hpclang_font_tamzen_powerline6x12_cp57506
.word hpclang_font_tamzen_powerline6x12_cp57520
.word hpclang_font_tamzen_powerline6x12_cp57521
.word hpclang_font_tamzen_powerline6x12_cp57522
.word hpclang_font_tamzen_powerline6x12_cp57523

.section .rodata.hpclang.font.tamzen_powerline6x12.full.data,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline6x12_full_data
hpclang_font_tamzen_powerline6x12_full_data:
.word hpclang_font_tamzen_powerline6x12_full_glyphs
.word 7
.word 57504
.hword 10,2,12,6

.section .rodata.hpclang.font.tamzen_powerline6x12.full.font,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline6x12_full
hpclang_font_tamzen_powerline6x12_full:
.word 1,1
.word hpclang_font_tamzen_powerline6x12_full_data
