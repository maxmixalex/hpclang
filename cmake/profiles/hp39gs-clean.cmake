# First-class clean hp39gs profile.
#
# No HPGCC SDK is consulted by this profile. Target code is compiled using
# stock LLVM ROPI+RWPI and linked with runtime/hp39gs/loader.S + link.ld.
#
# The current calculator-side launcher contract is the hardware-proven HP39
# Launcher V3 M+R memory-packer contract. HP39 aplet packaging is a separate
# platform layer and is still being reconstructed from source.

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:-mabi=apcs-gnu>"
    "$<$<COMPILE_LANGUAGE:C>:-mfloat-abi=soft>"
    "$<$<COMPILE_LANGUAGE:C>:-fropi>"
    "$<$<COMPILE_LANGUAGE:C>:-frwpi>"
)

set(HPCLANG_HP39GS_LOADER
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/hp39gs/loader.S")
set(HPCLANG_HP39GS_RUNTIME
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/hp39gs/runtime.c")
set(HPCLANG_HP39GS_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/hp39gs/link.ld")
set(HPCLANG_HP39GS_ELF2L3
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/elf2l3.py")

set(HPCLANG_HP39GS_CODE_MODEL "ropi-rwpi" CACHE STRING
    "Clean hp39gs LLVM code model")
set(HPCLANG_HP39GS_LAUNCH_MODE "launcher-v3-mmu-mr" CACHE STRING
    "HP39 calculator-side launch contract")
