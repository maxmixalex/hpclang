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

target_include_directories(hpclang_font_gallery PRIVATE
    "${HPCLANG_ROOT}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(hpclang_font_gallery PRIVATE c_std_11)
target_compile_options(hpclang_font_gallery PRIVATE
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fno-pic -fno-pie -ffunction-sections -fdata-sections)

# The Gallery intentionally links every generated face in the catalog.  Each
# face retains only the comparison repertoire below, so large Unicode families
# remain practical while every size/weight/language variant stays browsable.
set(GALLERY_FONT_IDS ${HPCLANG_FONT_IDS})
list(SORT GALLERY_FONT_IDS)

# Printable ASCII is the common comparison repertoire.  The second group is a
# deliberately broad Unicode probe: Latin-1, Greek/math, arrows/blocks, CJK,
# and Hangul.  Private-use mappings are sampled separately from each face's
# real cmap so icon/controller fonts display their actual reserved glyphs.
set(GALLERY_CODEPOINTS)
foreach(cp RANGE 32 126)
    list(APPEND GALLERY_CODEPOINTS "${cp}")
endforeach()
set(GALLERY_UNICODE_CODEPOINTS
    163 165 169 176 177 181 215 247 8364
    916 931 937 945 946 955 960
    8592 8593 8594 8595 8596
    8730 8734 8776 8800 8804 8805
    9472 9474 9484 9488 9492 9496 9500 9508 9516 9524 9532
    9608 9617 9632 9679
    20320 22909 19990 30028 26085 26412 35486 54620 44544
)
list(APPEND GALLERY_CODEPOINTS ${GALLERY_UNICODE_CODEPOINTS})
set(GALLERY_PRIVATE_MAX 12)

set(catalog_h "${CMAKE_CURRENT_BINARY_DIR}/font-gallery-catalog.h")
set(catalog_c "${CMAKE_CURRENT_BINARY_DIR}/font-gallery-catalog.c")
file(WRITE "${catalog_h}"
"#ifndef HPCLANG_FONT_GALLERY_CATALOG_H\n"
"#define HPCLANG_FONT_GALLERY_CATALOG_H\n"
"#include <stdint.h>\n"
"#include <hpclang/font.h>\n"
"struct hpclang_gallery_face {\n"
"    const char *family;\n"
"    const char *face;\n"
"    const struct hpclang_font *font;\n"
"    const uint32_t *unicode_codepoints;\n"
"    uint32_t unicode_count;\n"
"    const uint32_t *private_codepoints;\n"
"    uint32_t private_count;\n"
"};\n"
"extern const struct hpclang_gallery_face hpclang_gallery_faces[];\n"
"extern const uint32_t hpclang_gallery_face_count;\n"
"#endif\n")
file(WRITE "${catalog_c}"
"#include <stdint.h>\n"
"#include \"font-gallery-catalog.h\"\n\n")

set(catalog_rows "")
set(face_index 0)
set(gallery_family_roots)
foreach(font_id IN LISTS GALLERY_FONT_IDS)
    set(selector_var "HPCLANG_FONT_${font_id}_SELECTOR")
    set(codepoints_var "HPCLANG_FONT_${font_id}_CODEPOINTS")
    set(codepoints_file_var "HPCLANG_FONT_${font_id}_CODEPOINTS_FILE")

    if(NOT DEFINED ${selector_var})
        message(FATAL_ERROR "Font Gallery references unknown font id: ${font_id}")
    endif()

    include("${${codepoints_file_var}}")
    set(available "${${codepoints_var}}")

    set(selected)
    set(selected_unicode)
    foreach(cp IN LISTS GALLERY_CODEPOINTS)
        list(FIND available "${cp}" found)
        if(NOT found EQUAL -1)
            list(APPEND selected "${cp}")
            list(FIND GALLERY_UNICODE_CODEPOINTS "${cp}" unicode_found)
            if(NOT unicode_found EQUAL -1)
                list(APPEND selected_unicode "${cp}")
            endif()
        endif()
    endforeach()

    # Preserve a small, representative private-use repertoire.  The first
    # pass takes one real mapped codepoint from each 0x100-codepoint page so
    # related icon sets (for example E8xx/E9xx/EAxx controller families) are
    # all visible.  The second pass fills the remaining slots with additional
    # real mappings without hard-coding any particular font's assignments.
    set(selected_private)
    set(private_pages)
    foreach(cp IN LISTS available)
        if(cp GREATER_EQUAL 57344 AND cp LESS_EQUAL 63743)
            math(EXPR private_page "${cp} / 256")
            list(FIND private_pages "${private_page}" private_page_found)
            if(private_page_found EQUAL -1)
                list(APPEND private_pages "${private_page}")
                list(APPEND selected_private "${cp}")
                list(LENGTH selected_private private_count)
                if(private_count GREATER_EQUAL GALLERY_PRIVATE_MAX)
                    break()
                endif()
            endif()
        endif()
    endforeach()
    list(LENGTH selected_private private_count)
    if(private_count LESS GALLERY_PRIVATE_MAX)
        foreach(cp IN LISTS available)
            if(cp GREATER_EQUAL 57344 AND cp LESS_EQUAL 63743)
                list(FIND selected_private "${cp}" private_found)
                if(private_found EQUAL -1)
                    list(APPEND selected_private "${cp}")
                    list(LENGTH selected_private private_count)
                    if(private_count GREATER_EQUAL GALLERY_PRIVATE_MAX)
                        break()
                    endif()
                endif()
            endif()
        endforeach()
    endif()
    if(selected_private)
        list(APPEND selected ${selected_private})
    endif()

    if(NOT selected)
        message(FATAL_ERROR "Font Gallery face has no display repertoire: ${font_id}")
    endif()

    set(symbol "hpclang_gallery_font_${face_index}")
    hpclang_target_font(hpclang_font_gallery
        NAME "${symbol}"
        FONT "${font_id}"
        MODE STATIC_EXPLICIT
        FALLBACK NONE
        CODEPOINTS ${selected})

    string(REPLACE "/" ";" selector_parts "${${selector_var}}")
    list(GET selector_parts 0 family_label)
    list(APPEND gallery_family_roots "${family_label}")
    string(REPLACE "-" " " family_label "${family_label}")
    get_filename_component(face_label "${${selector_var}}" NAME_WE)

    string(REPLACE "\\" "\\\\" family_label_c "${family_label}")
    string(REPLACE "\"" "\\\"" family_label_c "${family_label_c}")
    string(REPLACE "\\" "\\\\" face_label_c "${face_label}")
    string(REPLACE "\"" "\\\"" face_label_c "${face_label_c}")

    file(APPEND "${catalog_c}"
        "extern const struct hpclang_font ${symbol};\n")
    if(selected_unicode)
        file(APPEND "${catalog_c}"
            "static const uint32_t unicode_${face_index}[] = {")
        foreach(cp IN LISTS selected_unicode)
            file(APPEND "${catalog_c}" "${cp}u,")
        endforeach()
        file(APPEND "${catalog_c}" "};\n")
        list(LENGTH selected_unicode unicode_count)
        set(unicode_pointer "unicode_${face_index}")
    else()
        set(unicode_count 0)
        set(unicode_pointer "0")
    endif()

    if(selected_private)
        file(APPEND "${catalog_c}"
            "static const uint32_t private_${face_index}[] = {")
        foreach(cp IN LISTS selected_private)
            file(APPEND "${catalog_c}" "${cp}u,")
        endforeach()
        file(APPEND "${catalog_c}" "};\n")
        list(LENGTH selected_private private_count)
        set(private_pointer "private_${face_index}")
    else()
        set(private_count 0)
        set(private_pointer "0")
    endif()

    string(APPEND catalog_rows
        "    {\"${family_label_c}\", \"${face_label_c}\", &${symbol}, ${unicode_pointer}, ${unicode_count}u, ${private_pointer}, ${private_count}u},\n")
    math(EXPR face_index "${face_index} + 1")
endforeach()

list(REMOVE_DUPLICATES gallery_family_roots)
list(LENGTH gallery_family_roots family_count)

file(APPEND "${catalog_c}"
"\nconst struct hpclang_gallery_face hpclang_gallery_faces[] = {\n"
"${catalog_rows}"
"};\n"
"const uint32_t hpclang_gallery_face_count =\n"
"    (uint32_t)(sizeof(hpclang_gallery_faces) / sizeof(hpclang_gallery_faces[0]));\n")
target_sources(hpclang_font_gallery PRIVATE "${catalog_c}")

message(STATUS
    "Font Gallery: ${face_index} generated faces from ${family_count} families + 2 native faces")

target_link_options(hpclang_font_gallery PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_font_gallery PROPERTIES
    OUTPUT_NAME hpclang-font-gallery
    SUFFIX ".elf")
