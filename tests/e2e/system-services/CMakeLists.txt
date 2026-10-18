cmake_minimum_required(VERSION 3.20)
project(hpclang_system_services_e2e LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")
include("${HPCLANG_ROOT}/cmake/fonts.cmake")

add_executable(hpclang_system_services_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c
    checks.c
    irq.c
    irq.S
    power.c
    console.c)

target_include_directories(
    hpclang_system_services_e2e PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
target_compile_features(hpclang_system_services_e2e PRIVATE c_std_11)
target_compile_options(hpclang_system_services_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)
target_link_options(hpclang_system_services_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(
    hpclang_system_services_e2e
    PROPERTIES OUTPUT_NAME hpclang-system-services-e2e SUFFIX ".elf")

hpclang_target_font(hpclang_system_services_e2e
    NAME hpclang_system_font FONT @mini MODE STATIC_AUTO FALLBACK NONE)
