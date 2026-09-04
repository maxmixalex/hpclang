.syntax unified
.arm
.text

.global _start
.type _start,%function
_start:
    /* Historical MMUloader has already established r10 == 0x09000400.
       Mirror it into the standard ARM RWPI static-base register before any
       Clang-generated C instruction can access writable globals. */
    mov r9, r10
    b hpclang_start
.size _start, .-_start
