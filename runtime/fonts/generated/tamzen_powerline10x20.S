.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.tamzen_powerline10x20.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_tamzen_powerline10x20_cp\cp\()_bitmap
hpclang_font_tamzen_powerline10x20_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.tamzen_powerline10x20.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline10x20_cp\cp
hpclang_font_tamzen_powerline10x20_cp\cp:
.word \cp
.word hpclang_font_tamzen_powerline10x20_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 57504,0,-6,10,20,10,2,192,0,194,0,199,0,207,128,218,192,194,0,194,0,194,0,194,0,194,0,198,0,140,0,24,0,48,0,96,0,192,0,192,0,192,0,192,0,192,0
G 57505,0,-6,10,20,10,2,0,0,128,0,128,0,128,0,128,0,128,0,128,0,128,0,128,0,255,0,0,0,0,0,32,64,48,64,40,64,36,64,34,64,33,64,32,192,32,64
G 57506,0,-6,10,20,10,2,30,0,33,0,64,128,64,128,64,128,64,128,64,128,64,128,255,192,255,192,255,192,255,64,254,192,253,64,250,192,245,64,234,192,213,64,170,192,255,192
G 57520,0,-6,10,20,10,2,128,0,192,0,224,0,240,0,248,0,252,0,254,0,255,0,255,128,255,192,255,192,255,128,255,0,254,0,252,0,248,0,240,0,224,0,192,0,128,0
G 57521,0,-6,10,20,10,2,128,0,64,0,32,0,16,0,8,0,4,0,2,0,1,0,0,128,0,64,0,64,0,128,1,0,2,0,4,0,8,0,16,0,32,0,64,0,128,0
G 57522,0,-6,10,20,10,2,0,64,0,192,1,192,3,192,7,192,15,192,31,192,63,192,127,192,255,192,255,192,127,192,63,192,31,192,15,192,7,192,3,192,1,192,0,192,0,64
G 57523,0,-6,10,20,10,2,0,64,0,128,1,0,2,0,4,0,8,0,16,0,32,0,64,0,128,0,128,0,64,0,32,0,16,0,8,0,4,0,2,0,1,0,0,128,0,64

.purgem G

.syntax unified

.section .rodata.hpclang.font.tamzen_powerline10x20.full.map,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline10x20_full_glyphs
hpclang_font_tamzen_powerline10x20_full_glyphs:
.word hpclang_font_tamzen_powerline10x20_cp57504
.word hpclang_font_tamzen_powerline10x20_cp57505
.word hpclang_font_tamzen_powerline10x20_cp57506
.word hpclang_font_tamzen_powerline10x20_cp57520
.word hpclang_font_tamzen_powerline10x20_cp57521
.word hpclang_font_tamzen_powerline10x20_cp57522
.word hpclang_font_tamzen_powerline10x20_cp57523

.section .rodata.hpclang.font.tamzen_powerline10x20.full.data,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline10x20_full_data
hpclang_font_tamzen_powerline10x20_full_data:
.word hpclang_font_tamzen_powerline10x20_full_glyphs
.word 7
.word 57504
.hword 14,6,20,10

.section .rodata.hpclang.font.tamzen_powerline10x20.full.font,"a",%progbits
.balign 4
.global hpclang_font_tamzen_powerline10x20_full
hpclang_font_tamzen_powerline10x20_full:
.word 1,1
.word hpclang_font_tamzen_powerline10x20_full_data
