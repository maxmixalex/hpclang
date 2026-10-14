cmake_minimum_required(VERSION 3.20)
project(hpclang_e2e LANGUAGES C CXX ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")
include("${HPCLANG_ROOT}/cmake/fonts.cmake")

option(HPCLANG_E2E_WAIT_FOR_ENTER "Wait for ENTER on the result screen" OFF)
option(HPCLANG_E2E_KEYBOARD "Exercise injected keyboard transitions" OFF)
option(HPCLANG_E2E_STORAGE "Exercise stock stream-storage stdio path" OFF)

add_executable(hpclang_e2e
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c
    divmod.S
    cpp.cpp)

target_include_directories(hpclang_e2e PRIVATE "${HPCLANG_ROOT}/include")
target_compile_features(hpclang_e2e PRIVATE c_std_11 cxx_std_23)
target_compile_options(hpclang_e2e PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)

# The generated mini font is both exercised off-screen by the C++ probe and
# rendered visibly in the normal PASS screenshot as the "FONT OK" label.
hpclang_target_font(hpclang_e2e
    NAME hpclang_e2e_ui_font
    FONT @mini
    MODE STATIC_AUTO
    FALLBACK NONE)

if(HPCLANG_E2E_WAIT_FOR_ENTER)
    target_compile_definitions(hpclang_e2e PRIVATE HPCLANG_E2E_WAIT_FOR_ENTER=1)
endif()
if(HPCLANG_E2E_KEYBOARD)
    target_compile_definitions(hpclang_e2e PRIVATE HPCLANG_E2E_KEYBOARD=1)
    target_sources(hpclang_e2e PRIVATE phase.c)
    target_link_options(hpclang_e2e PRIVATE
        -Wl,--wrap=hpclang_keyboard_read
        -Wl,--wrap=hpclang_keyboard_events_reset
        -Wl,--wrap=hpclang_keyboard_event_poll)
endif()
if(HPCLANG_E2E_STORAGE)
    target_compile_definitions(hpclang_e2e PRIVATE HPCLANG_E2E_STORAGE=1)
    target_sources(hpclang_e2e PRIVATE historical-files.c)
endif()

target_link_options(hpclang_e2e PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_e2e PROPERTIES OUTPUT_NAME hpclang-e2e SUFFIX ".elf")
