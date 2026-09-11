.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.tamzen_powerline8x16.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_tamzen_powerline8x16_cp\cp\()_bitmap
hpclang_font_tamzen_powerline8x16_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.tamzen_powerline8x16.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline8x16_cp\cp
hpclang_font_tamzen_powerline8x16_cp\cp:
.word \cp
.word hpclang_font_tamzen_powerline8x16_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 57504,0,-4,8,17,8,1,0,192,196,206,223,196,196,196,196,140,28,56,112,224,192,192,192
G 57505,0,-4,8,17,8,1,0,128,128,128,128,128,128,248,0,33,49,41,37,35,33,0,0
G 57506,0,-4,8,17,8,1,0,60,66,66,66,66,66,255,255,255,253,251,245,235,213,171,255
G 57520,0,-4,8,17,8,1,0,128,192,224,240,248,252,254,255,255,254,252,248,240,224,192,128
G 57521,0,-4,8,17,8,1,0,128,64,32,16,8,4,2,1,1,2,4,8,16,32,64,128
G 57522,0,-4,8,17,8,1,0,1,3,7,15,31,63,127,255,255,127,63,31,15,7,3,1
G 57523,0,-4,8,17,8,1,0,1,2,4,8,16,32,64,128,128,64,32,16,8,4,2,1

.purgem G

.syntax unified

.section .rodata.hpclang.font.tamzen_powerline8x16.full.map,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline8x16_full_glyphs
hpclang_font_tamzen_powerline8x16_full_glyphs:
.word hpclang_font_tamzen_powerline8x16_cp57504
.word hpclang_font_tamzen_powerline8x16_cp57505
.word hpclang_font_tamzen_powerline8x16_cp57506
.word hpclang_font_tamzen_powerline8x16_cp57520
.word hpclang_font_tamzen_powerline8x16_cp57521
.word hpclang_font_tamzen_powerline8x16_cp57522
.word hpclang_font_tamzen_powerline8x16_cp57523

.section .rodata.hpclang.font.tamzen_powerline8x16.full.data,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline8x16_full_data
hpclang_font_tamzen_powerline8x16_full_data:
.word hpclang_font_tamzen_powerline8x16_full_glyphs
.word 7
.word 57504
.hword 12,4,16,8

.section .rodata.hpclang.font.tamzen_powerline8x16.full.font,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline8x16_full
hpclang_font_tamzen_powerline8x16_full:
.word 1,1
.word hpclang_font_tamzen_powerline8x16_full_data
