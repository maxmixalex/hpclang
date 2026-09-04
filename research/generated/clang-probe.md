# Modern Clang probe

## Version
~~~
Ubuntu clang version 18.1.3 (1ubuntu1)
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
~~~

## Old required flag: -ffixed-r10
~~~
clang: error: unknown argument '-ffixed-r10'; did you mean '-ffixed-r19'?
~~~

## APCS GNU ABI support
~~~
/tmp/apcs.o: ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV), not stripped
~~~

## PIC codegen
~~~asm
	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	arm7tdmi
	.eabi_attribute	6, 2	@ Tag_CPU_arch
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	9, 1	@ Tag_THUMB_ISA_use
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	15, 1	@ Tag_ABI_PCS_RW_data
	.eabi_attribute	16, 1	@ Tag_ABI_PCS_RO_data
	.eabi_attribute	17, 2	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 0	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 4	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 2	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.file	"pic.c"
	.globl	f                               @ -- Begin function f
	.p2align	2
	.type	f,%function
	.code	32                              @ @f
f:
	.fnstart
@ %bb.0:
	ldr	r1, .LCPI0_0
.LPC0_0:
	ldr	r1, [pc, r1]
	ldr	r1, [r1]
	add	r0, r1, r0
	ldr	r1, .LCPI0_1
.LPC0_1:
	ldr	r1, [pc, r1]
	ldr	r1, [r1]
	add	r0, r0, r1
	bx	lr
	.p2align	2
@ %bb.1:
.LCPI0_0:
.Ltmp0:
	.long	ext(GOT_PREL)-((.LPC0_0+8)-.Ltmp0)
.LCPI0_1:
.Ltmp1:
	.long	glob(GOT_PREL)-((.LPC0_1+8)-.Ltmp1)
.Lfunc_end0:
	.size	f, .Lfunc_end0-f
	.cantunwind
	.fnend
                                        @ -- End function
	.type	glob,%object                    @ @glob
	.data
	.globl	glob
	.p2align	2, 0x0
glob:
	.long	3                               @ 0x3
	.size	glob, 4

	.ident	"Ubuntu clang version 18.1.3 (1ubuntu1)"
	.section	".note.GNU-stack","",%progbits
	.addrsig
	.eabi_attribute	30, 1	@ Tag_ABI_optimization_goals
~~~
