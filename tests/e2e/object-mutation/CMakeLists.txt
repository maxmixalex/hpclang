cmake_minimum_required(VERSION 3.20)
project(hpclang_object_mutation_e2e LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")

add_executable(hpclang_object_mutation_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c)

# The default compiles both paths for source analysis. CI builds one family
# per artifact so HP39 Aplets do not consume RAM for classic-only probes.
set(HPCLANG_E2E_FAMILY "all" CACHE STRING "Object mutation target family")
set_property(CACHE HPCLANG_E2E_FAMILY PROPERTY STRINGS all hp39 hp48 hp4950)
if(NOT HPCLANG_E2E_FAMILY MATCHES "^(all|hp39|hp48|hp4950)$")
    message(FATAL_ERROR "Unsupported object mutation family: ${HPCLANG_E2E_FAMILY}")
endif()
if(HPCLANG_E2E_FAMILY MATCHES "^(all|hp39)$")
    target_sources(hpclang_object_mutation_e2e PRIVATE hp39.c)
    target_compile_definitions(hpclang_object_mutation_e2e PRIVATE HPCLANG_E2E_HP39=1)
endif()
if(NOT HPCLANG_E2E_FAMILY STREQUAL "hp39")
    target_sources(hpclang_object_mutation_e2e PRIVATE classic.c nested.c ports.c)
    target_compile_definitions(hpclang_object_mutation_e2e PRIVATE HPCLANG_E2E_CLASSIC=1)
endif()

target_include_directories(
    hpclang_object_mutation_e2e PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
target_compile_features(hpclang_object_mutation_e2e PRIVATE c_std_11)
target_compile_options(hpclang_object_mutation_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)
target_link_options(hpclang_object_mutation_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(
    hpclang_object_mutation_e2e
    PROPERTIES OUTPUT_NAME hpclang-object-mutation-e2e SUFFIX ".elf")
