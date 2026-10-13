cmake_minimum_required(VERSION 3.20)
project(hpclang_declarative_ui LANGUAGES C ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(declarative_ui main.c app.c)
option(HPCLANG_DEMO_INVERTED "Use the alternate UI palette" OFF)
option(HPCLANG_DEMO_NATIVE_FONT "Prefer the calculator's native default font" OFF)
target_compile_definitions(declarative_ui PRIVATE
    HPCLANG_DEMO_INVERTED=$<BOOL:${HPCLANG_DEMO_INVERTED}>
    HPCLANG_DEMO_NATIVE_FONT=$<BOOL:${HPCLANG_DEMO_NATIVE_FONT}>)
hpclang_target_launchers(declarative_ui)
set(ui_ascii)
foreach(codepoint RANGE 32 126)
    list(APPEND ui_ascii ${codepoint})
endforeach()
hpclang_target_font(declarative_ui NAME demo_font FONT @mini MODE STATIC_EXPLICIT
    CODEPOINTS ${ui_ascii} FALLBACK 63)
