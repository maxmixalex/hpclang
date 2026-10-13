cmake_minimum_required(VERSION 3.20)
project(hpclang_hpgcc_system_e2e LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")
include("${HPCLANG_ROOT}/cmake/fonts.cmake")

add_executable(hpclang_hpgcc_system_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c
    historical-system.c)

target_include_directories(
    hpclang_hpgcc_system_e2e PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
target_compile_features(hpclang_hpgcc_system_e2e PRIVATE c_std_11)
target_compile_options(hpclang_hpgcc_system_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)
target_link_options(hpclang_hpgcc_system_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(
    hpclang_hpgcc_system_e2e
    PROPERTIES OUTPUT_NAME hpclang-hpgcc-system-e2e SUFFIX ".elf")

# Benchmark output contains arbitrary decimal values and fixed ASCII labels.
# Declare that repertoire explicitly instead of packaging the full Unicode font
# into the HP39 application's limited shared code/heap arena.
set(benchmark_ascii)
foreach(codepoint RANGE 32 126)
    list(APPEND benchmark_ascii ${codepoint})
endforeach()
hpclang_target_font(hpclang_hpgcc_system_e2e
    NAME hpclang_benchmark_font FONT @mini MODE STATIC_EXPLICIT
    CODEPOINTS ${benchmark_ascii} FALLBACK NONE)
