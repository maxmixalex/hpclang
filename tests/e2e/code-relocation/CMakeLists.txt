cmake_minimum_required(VERSION 3.20)
project(hpclang_code_relocation_e2e LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cases.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures.py"
            "${CMAKE_CURRENT_BINARY_DIR}/cases.h"
    DEPENDS fixtures.py VERBATIM)

add_executable(hpclang_code_relocation_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c "${CMAKE_CURRENT_BINARY_DIR}/cases.h")
target_include_directories(hpclang_code_relocation_e2e PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(hpclang_code_relocation_e2e PRIVATE c_std_11)
target_compile_options(hpclang_code_relocation_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)
target_link_options(hpclang_code_relocation_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_code_relocation_e2e
    PROPERTIES OUTPUT_NAME hpclang-code-relocation-e2e SUFFIX ".elf")
