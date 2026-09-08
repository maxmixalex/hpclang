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
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/runtime.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/device.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display-surface.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard-event-engine.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard-events.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/time-math.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/time.c")
set(HPCLANG_RUNTIME_EABI_SHIMS
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-eabi-div.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-softfloat-entry.S")
set(HPCLANG_RUNTIME_SOFTFLOAT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-softfloat.c")

# These implementation objects deliberately stay in ARM state. Application C/C++
# remains Thumb-1; the EABI veneers provide the one explicit interworking edge.
set_source_files_properties(
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    PROPERTIES COMPILE_OPTIONS "-marm")
set(HPCLANG_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/link.ld")
set(HPCLANG_PACKER
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/hpclang-pack.py")

set(HPCLANG_ABI "hpc1" CACHE STRING "hpclang executable ABI")
set(HPCLANG_CODE_MODEL "fixed-virtual" CACHE STRING "hpclang code model")
