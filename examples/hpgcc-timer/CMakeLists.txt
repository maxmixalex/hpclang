cmake_minimum_required(VERSION 3.20)
project(hpclang_hpgcc_timer LANGUAGES C ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(hpgcc_timer main.c)
target_compile_features(hpgcc_timer PRIVATE c_std_11)
hpclang_target_launchers(hpgcc_timer)

# The formatted duration has a variable decimal representation.
set(report_ascii)
foreach(codepoint RANGE 32 126)
    list(APPEND report_ascii ${codepoint})
endforeach()
hpclang_target_font(hpgcc_timer
    NAME report_font FONT @mini MODE STATIC_EXPLICIT
    CODEPOINTS ${report_ascii} FALLBACK NONE)
