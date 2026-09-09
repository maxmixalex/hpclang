cmake_minimum_required(VERSION 3.20)
project(hpclang_e2e LANGUAGES C CXX ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")

option(HPCLANG_E2E_WAIT_FOR_ENTER "Wait for ENTER on the result screen" OFF)
option(HPCLANG_E2E_KEYBOARD "Exercise injected keyboard transitions" OFF)

add_executable(hpclang_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    "${HPCLANG_ROOT}/runtime/libc/sbrk.c"
    main.c
    cpp.cpp)

target_include_directories(hpclang_e2e PRIVATE "${HPCLANG_ROOT}/include")
target_compile_features(hpclang_e2e PRIVATE c_std_11 cxx_std_23)
target_compile_options(hpclang_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)

if(HPCLANG_E2E_WAIT_FOR_ENTER)
    target_compile_definitions(hpclang_e2e PRIVATE HPCLANG_E2E_WAIT_FOR_ENTER=1)
endif()
if(HPCLANG_E2E_KEYBOARD)
    target_compile_definitions(hpclang_e2e PRIVATE HPCLANG_E2E_KEYBOARD=1)
endif()

target_link_options(hpclang_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_e2e PROPERTIES OUTPUT_NAME hpclang-e2e SUFFIX ".elf")
