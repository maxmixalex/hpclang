cmake_minimum_required(VERSION 3.20)
project(hpclang_hpgcc_window_e2e LANGUAGES C ASM)
set(HPCLANG_WINDOW_GROUP 0 CACHE STRING "Window checks: 0 all, 1 foundation, 2 dialogs")
if(NOT HPCLANG_WINDOW_GROUP MATCHES "^[012]$")
    message(FATAL_ERROR "HPCLANG_WINDOW_GROUP must be 0, 1 or 2")
endif()
if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")
add_executable(hpclang_hpgcc_window_e2e
    ${HPCLANG_RUNTIME_START} ${HPCLANG_RUNTIME} ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT} ${HPCLANG_LIBC} main.c)
target_include_directories(hpclang_hpgcc_window_e2e PRIVATE "${HPCLANG_ROOT}/include")
target_compile_features(hpclang_hpgcc_window_e2e PRIVATE c_std_11)
target_compile_definitions(hpclang_hpgcc_window_e2e PRIVATE
    HPCLANG_WINDOW_GROUP=${HPCLANG_WINDOW_GROUP})
target_compile_options(hpclang_hpgcc_window_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)
target_link_options(hpclang_hpgcc_window_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_hpgcc_window_e2e
    PROPERTIES OUTPUT_NAME hpclang-hpgcc-window-e2e SUFFIX ".elf")
