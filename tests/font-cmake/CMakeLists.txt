cmake_minimum_required(VERSION 3.20)
project(hpclang_font_cmake_modes LANGUAGES C ASM)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

include("${HPCLANG_ROOT}/cmake/fonts.cmake")

# Runtime sources share the application target in the execution profile and
# must therefore also be valid STATIC_AUTO scanner input. UI node names must
# not collide with the public string marker.
hpclang_collect_ui_text_codepoints(runtime_codepoints
    "${HPCLANG_ROOT}/runtime/core/ui.c"
    "${HPCLANG_ROOT}/runtime/core/ui-surface.c")
if(runtime_codepoints)
    message(FATAL_ERROR "UI runtime unexpectedly contributes application text")
endif()

add_executable(font_mode_probe EXCLUDE_FROM_ALL auto.c)
target_include_directories(font_mode_probe PRIVATE "${HPCLANG_ROOT}/include")

hpclang_target_font(font_mode_probe
    NAME auto_font
    FONT @normal
    MODE STATIC_AUTO
    FALLBACK NONE)

hpclang_target_font(font_mode_probe
    NAME explicit_font
    FONT @mini
    MODE STATIC_EXPLICIT
    CODEPOINTS 68 69
    FALLBACK NONE)

hpclang_target_font(font_mode_probe
    NAME full_font
    FONT @large
    MODE FULL)

function(require_contains path needle)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${path} does not contain expected text: ${needle}")
    endif()
endfunction()

function(require_not_contains path needle)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "${path} unexpectedly contains text: ${needle}")
    endif()
endfunction()

set(generated "${CMAKE_CURRENT_BINARY_DIR}/hpclang-fonts")

# STATIC_AUTO sees only the explicit marker. The unmarked Z, marker spelling
# inside a string, and markers in comments must not affect the repertoire.
require_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp65")
require_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp66")
require_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp67")
require_not_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp90")
require_not_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp81")
require_not_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp88")
require_not_contains("${generated}/auto_font.c" "hpclang_font_newrpl_8c_cp89")

# STATIC_EXPLICIT ignores source literals entirely and retains exactly the
# caller-specified D/E repertoire when fallback is disabled.
require_contains("${generated}/explicit_font.c" "hpclang_font_newrpl_6a_cp68")
require_contains("${generated}/explicit_font.c" "hpclang_font_newrpl_6a_cp69")
require_not_contains("${generated}/explicit_font.c" "hpclang_font_newrpl_6a_cp65")

# FULL binds the generated complete-face descriptor rather than synthesizing a
# subset table.
require_contains("${generated}/full_font.c" "hpclang_font_newrpl_10a_full_data")
