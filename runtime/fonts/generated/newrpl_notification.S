.syntax unified

.macro G cp,xo,yo,w,h,a,s,bytes:vararg
.section .rodata.hpclang.font.newrpl_notification.cp\cp\().bitmap,"a",%progbits
.global hpclang_font_newrpl_notification_cp\cp\()_bitmap
hpclang_font_newrpl_notification_cp\cp\()_bitmap:
.byte \bytes
.section .rodata.hpclang.font.newrpl_notification.cp\cp\().glyph,"a",%progbits
.balign 4
.global hpclang_font_newrpl_notification_cp\cp
hpclang_font_newrpl_notification_cp\cp:
.word \cp
.word hpclang_font_newrpl_notification_cp\cp\()_bitmap
.hword \xo,\yo,\w,\h,\a,\s
.endm

G 65,0,0,15,12,15,2,7,192,24,34,48,50,96,52,224,52,224,56,224,48,224,48,224,112,112,178,31,28,0,0
G 66,0,0,8,12,8,1,24,255,129,153,153,153,153,153,129,153,129,255
G 67,0,0,8,12,8,1,8,235,153,153,184,63,126,14,140,141,137,235
G 68,0,0,8,12,8,1,24,255,129,129,129,129,255,255,255,255,255,255
G 76,0,0,15,12,15,2,127,252,255,254,249,254,243,254,231,254,192,30,192,14,231,206,243,206,249,206,255,206,127,204
G 82,0,0,15,12,15,2,127,252,255,254,255,62,255,158,255,206,240,6,224,6,231,206,231,158,231,62,231,254,103,252
G 85,0,0,15,12,15,2,48,0,120,0,204,48,134,24,51,12,127,254,127,254,51,12,134,24,204,48,120,0,48,0
G 87,0,0,13,12,13,2,127,240,32,32,32,32,31,192,15,128,7,0,5,0,8,128,16,64,39,32,47,160,127,240
G 88,0,0,15,12,15,2,32,8,64,4,72,36,144,18,147,146,147,146,147,146,144,18,72,36,64,4,32,8,0,0

.purgem G

.syntax unified

.section .rodata.hpclang.font.newrpl_notification.full.map,"a",%progbits
.balign 4
.global hpclang_font_newrpl_notification_full_glyphs
hpclang_font_newrpl_notification_full_glyphs:
.word hpclang_font_newrpl_notification_cp65
.word hpclang_font_newrpl_notification_cp66
.word hpclang_font_newrpl_notification_cp67
.word hpclang_font_newrpl_notification_cp68
.word hpclang_font_newrpl_notification_cp76
.word hpclang_font_newrpl_notification_cp82
.word hpclang_font_newrpl_notification_cp85
.word hpclang_font_newrpl_notification_cp87
.word hpclang_font_newrpl_notification_cp88

.section .rodata.hpclang.font.newrpl_notification.full.data,"a",%progbits
.balign 4
.global hpclang_font_newrpl_notification_full_data
hpclang_font_newrpl_notification_full_data:
.word hpclang_font_newrpl_notification_full_glyphs
.word 9
.word 65
.hword 12,0,12,12

.section .rodata.hpclang.font.newrpl_notification.full.font,"a",%progbits
.balign 4
.global hpclang_font_newrpl_notification_full
hpclang_font_newrpl_notification_full:
.word 1,1
.word hpclang_font_newrpl_notification_full_data
