cmake_minimum_required(VERSION 3.20)
project(hpclang_font_gallery LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${HPCLANG_ROOT}/cmake/profiles/hpclang-v1.cmake")
include("${HPCLANG_ROOT}/cmake/fonts.cmake")

add_executable(hpclang_font_gallery
    ${HPCLANG_RUNTIME_START}
    ${HPCLANG_RUNTIME}
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    ${HPCLANG_LIBC}
    main.c)

target_include_directories(hpclang_font_gallery PRIVATE "${HPCLANG_ROOT}/include")
target_compile_features(hpclang_font_gallery PRIVATE c_std_11)
target_compile_options(hpclang_font_gallery PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)

# Three convenience aliases exercise the intended generated-font policy layer.
# They share an explicit STATIC_AUTO repertoire declared in main.c.
hpclang_target_font(hpclang_font_gallery
    NAME hpclang_gallery_mini
    FONT @mini
    MODE STATIC_AUTO
    FALLBACK NONE)
hpclang_target_font(hpclang_font_gallery
    NAME hpclang_gallery_normal
    FONT @normal
    MODE STATIC_AUTO
    FALLBACK NONE)
hpclang_target_font(hpclang_font_gallery
    NAME hpclang_gallery_large
    FONT @large
    MODE STATIC_AUTO
    FALLBACK NONE)

# One arbitrary catalog face is intentionally FULL so the gallery proves that
# arbitrary face selection and complete mapped repertoire remain independent
# from the three convenience aliases.
hpclang_target_font(hpclang_font_gallery
    NAME hpclang_gallery_spleen
    FONT spleen_spleen_5x8
    MODE FULL)

target_link_options(hpclang_font_gallery PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_font_gallery PROPERTIES
    OUTPUT_NAME hpclang-font-gallery
    SUFFIX ".elf")
