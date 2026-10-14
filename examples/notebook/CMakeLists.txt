cmake_minimum_required(VERSION 3.20)
project(hpclang_notebook LANGUAGES C ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(notebook main.c app.c store.c export.c)
# This application prints dates, port indices and integer error codes only.
target_link_options(notebook PRIVATE "LINKER:--defsym=vfprintf=__i_vfprintf")
hpclang_target_launchers(notebook)
set(notebook_ascii)
foreach(codepoint RANGE 32 126)
    list(APPEND notebook_ascii ${codepoint})
endforeach()
hpclang_target_font(notebook NAME notebook_font FONT @mini MODE STATIC_EXPLICIT
    CODEPOINTS ${notebook_ascii} FALLBACK 63)
