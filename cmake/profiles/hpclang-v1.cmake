# Unified hpclang v1 execution profile.
#
# The application ABI is independent of the calculator family:
# ARMv4T, AAPCS32, Thumb-1 C code, soft-float and fixed virtual addresses.

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:-mthumb>"
    "$<$<COMPILE_LANGUAGE:C>:-mabi=aapcs>"
    "$<$<COMPILE_LANGUAGE:C>:-mfloat-abi=soft>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-pic>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-pie>"
)

set(HPCLANG_RUNTIME_START
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/start.S")
set(HPCLANG_RUNTIME
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/runtime.c")
set(HPCLANG_RUNTIME_EABI_SHIMS
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-eabi-div.S")
set(HPCLANG_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/link.ld")
set(HPCLANG_PACKER
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/hpclang-pack.py")

set(HPCLANG_ABI "hpc1" CACHE STRING "hpclang executable ABI")
set(HPCLANG_CODE_MODEL "fixed-virtual" CACHE STRING "hpclang code model")
