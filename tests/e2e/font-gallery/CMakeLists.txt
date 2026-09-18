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

# Direct Gallery is exhaustive. Stock Gallery is deliberately a compact launch
# smoke test: two native faces plus one explicit generated control face. Do not
# let list ordering silently change the control face.
set(HPCLANG_GALLERY_PROFILE "full" CACHE STRING
    "Font Gallery profile: full or stock-smoke")
set_property(CACHE HPCLANG_GALLERY_PROFILE PROPERTY STRINGS full stock-smoke)

set(GALLERY_FONT_IDS ${HPCLANG_FONT_IDS})
list(SORT GALLERY_FONT_IDS)
set(HPCLANG_GALLERY_SHARD_INDEX "0" CACHE STRING
    "Zero-based exhaustive Gallery shard index")
set(HPCLANG_GALLERY_SHARD_COUNT "1" CACHE STRING
    "Number of exhaustive Gallery shards")

if(HPCLANG_GALLERY_PROFILE STREQUAL "stock-smoke")
    set(GALLERY_STOCK_FACE "newrpl_8c")
    list(FIND GALLERY_FONT_IDS "${GALLERY_STOCK_FACE}" gallery_stock_index)
    if(gallery_stock_index EQUAL -1)
        message(FATAL_ERROR
            "Stock Gallery control face is missing: ${GALLERY_STOCK_FACE}")
    endif()
    set(GALLERY_FONT_IDS "${GALLERY_STOCK_FACE}")
elseif(HPCLANG_GALLERY_PROFILE STREQUAL "full")
    if(NOT HPCLANG_GALLERY_SHARD_INDEX MATCHES "^[0-9]+$" OR
       NOT HPCLANG_GALLERY_SHARD_COUNT MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "Gallery shard index/count must be non-negative/positive integers")
    endif()
    if(HPCLANG_GALLERY_SHARD_INDEX GREATER_EQUAL HPCLANG_GALLERY_SHARD_COUNT)
        message(FATAL_ERROR
            "Gallery shard index must be smaller than shard count")
    endif()

    if(HPCLANG_GALLERY_SHARD_COUNT GREATER 1)
        set(GALLERY_SHARD_FONT_IDS)
        set(gallery_catalog_index 0)
        foreach(font_id IN LISTS GALLERY_FONT_IDS)
            math(EXPR gallery_owner
                "${gallery_catalog_index} % ${HPCLANG_GALLERY_SHARD_COUNT}")
            if(gallery_owner EQUAL HPCLANG_GALLERY_SHARD_INDEX)
                list(APPEND GALLERY_SHARD_FONT_IDS "${font_id}")
            endif()
            math(EXPR gallery_catalog_index "${gallery_catalog_index} + 1")
        endforeach()
        if(NOT GALLERY_SHARD_FONT_IDS)
            message(FATAL_ERROR
                "Gallery shard ${HPCLANG_GALLERY_SHARD_INDEX}/${HPCLANG_GALLERY_SHARD_COUNT} is empty")
        endif()
        set(GALLERY_FONT_IDS ${GALLERY_SHARD_FONT_IDS})
    endif()
else()
    message(FATAL_ERROR
        "HPCLANG_GALLERY_PROFILE must be full or stock-smoke")
endif()

# Core diagnostic text. Every face only receives codepoints that its real cmap
# actually contains.
set(GALLERY_ASCII_CODEPOINTS
    32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47
    48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63
    64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79
    80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95
    96 97 98 99 100 101 102 103 104 105 106 107 108 109 110 111
    112 113 114 115 116 117 118 119 120 121 122 123 124 125 126)

# Locale-specific specimens come first so regional CJK faces do not all show
# the same generic Han row. The shared same-codepoint tail (骨/直/令/返/道)
# makes regional glyph-shape differences visible without relying only on
# simplified-vs-traditional codepoint differences.
set(GALLERY_SPECIMEN_JA
    26085 26412 35486 26481 20140 12354 12356 12450 12452
    39592 30452 20196 36820 36947)
set(GALLERY_SPECIMEN_KO
    54620 44544 44397 50612 44032 45208 45796
    39592 30452 20196 36820 36947)
set(GALLERY_SPECIMEN_ZH_CN
    31616 20307 20013 25991 22269 38376 39118 40857 35821
    39592 30452 20196 36820 36947)
set(GALLERY_SPECIMEN_ZH_HK
    39321 28207 20013 25991 22283 38272 39080 40845 35486
    39592 30452 20196 36820 36947)
set(GALLERY_SPECIMEN_ZH_TR
    32321 39636 20013 25991 22283 38272 39080 40845 35486
    39592 30452 20196 36820 36947)
# Locale rows are glyph probes, not prose. Keep the Taiwan-specific row
# semantically neutral so adjacent test glyphs do not read as a political claim.
set(GALLERY_SPECIMEN_ZH_TW
    32321 39636 20013 25991 23383 24418 33274 28771 39080 40845 35486
    39592 30452 20196 36820 36947)
set(GALLERY_SPECIMEN_CJK_GENERIC
    20013 25991 23383 20307 39636 26085 26412 20154 23665 27700
    39592 30452 20196)

# Common repertoire probes. Range sampling below supplements these when a face
# has useful symbols outside this preferred list.
set(GALLERY_SPECIMEN_FEATURES
    192 193 196 199 201 209 214 220 223 230 248 339
    163 165 169 176 177 181 215 247 8364
    937 945 946 955 960
    8592 8593 8594 8595 8596 8730 8734 8776 8800 8804 8805
    9472 9474 9484 9488 9492 9496
    9632 9633 9650 9660 9670 9675
    9733 9734 9745 9824 9829 9830 9831)

set(GALLERY_SPECIMEN_MAX 28)

macro(gallery_append_present list_name cp_value)
    list(FIND available "${cp_value}" _gallery_found)
    if(NOT _gallery_found EQUAL -1)
        list(APPEND ${list_name} "${cp_value}")
    endif()
endmacro()

macro(gallery_append_range list_name lo hi limit)
    set(_gallery_added 0)
    foreach(_gallery_cp IN LISTS available)
        if(_gallery_added GREATER_EQUAL ${limit})
            break()
        endif()
        if(_gallery_cp GREATER_EQUAL ${lo} AND _gallery_cp LESS_EQUAL ${hi})
            list(APPEND ${list_name} "${_gallery_cp}")
            math(EXPR _gallery_added "${_gallery_added} + 1")
        endif()
    endforeach()
endmacro()

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
"    const uint32_t *specimen_codepoints;\n"
"    uint32_t specimen_count;\n"
"    uint32_t text_capable;\n"
"    uint32_t specimen_profile;\n"
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

    list(FIND available "32" text_space)
    list(FIND available "48" text_zero)
    list(FIND available "65" text_upper_a)
    list(FIND available "97" text_lower_a)
    list(FIND available "66" text_upper_b)
    list(FIND available "98" text_lower_b)
    set(text_capable 0)
    if(NOT text_space EQUAL -1 AND NOT text_zero EQUAL -1)
        if((NOT text_upper_a EQUAL -1 OR NOT text_lower_a EQUAL -1) AND
           (NOT text_upper_b EQUAL -1 OR NOT text_lower_b EQUAL -1))
            set(text_capable 1)
        endif()
    endif()

    set(specimen_profile 0)
    set(locale_specimen)
    if(font_id MATCHES "_(ja|jp)$")
        set(specimen_profile 1)
        set(locale_specimen ${GALLERY_SPECIMEN_JA})
    elseif(font_id MATCHES "_ko$")
        set(specimen_profile 2)
        set(locale_specimen ${GALLERY_SPECIMEN_KO})
    elseif(font_id MATCHES "_(zh_cn|zh_hans)$")
        set(specimen_profile 3)
        set(locale_specimen ${GALLERY_SPECIMEN_ZH_CN})
    elseif(font_id MATCHES "_zh_hk$")
        set(specimen_profile 4)
        set(locale_specimen ${GALLERY_SPECIMEN_ZH_HK})
    elseif(font_id MATCHES "_(zh_tr|zh_hant)$")
        set(specimen_profile 5)
        set(locale_specimen ${GALLERY_SPECIMEN_ZH_TR})
    elseif(font_id MATCHES "_zh_tw$")
        set(specimen_profile 6)
        set(locale_specimen ${GALLERY_SPECIMEN_ZH_TW})
    endif()

    set(selected)
    foreach(cp IN LISTS GALLERY_ASCII_CODEPOINTS)
        gallery_append_present(selected "${cp}")
    endforeach()

    set(specimen)
    if(text_capable)
        if(NOT "${locale_specimen}" STREQUAL "")
            foreach(cp IN LISTS locale_specimen)
                gallery_append_present(specimen "${cp}")
            endforeach()
        else()
            foreach(cp IN LISTS GALLERY_SPECIMEN_CJK_GENERIC)
                gallery_append_present(specimen "${cp}")
            endforeach()
        endif()

        foreach(cp IN LISTS GALLERY_SPECIMEN_FEATURES)
            gallery_append_present(specimen "${cp}")
        endforeach()

        gallery_append_range(specimen 8592 8703 2)
        gallery_append_range(specimen 8704 8959 2)
        gallery_append_range(specimen 8960 9215 2)
        gallery_append_range(specimen 9472 9599 2)
        gallery_append_range(specimen 9600 9631 2)
        gallery_append_range(specimen 9632 9727 2)
        gallery_append_range(specimen 9728 9983 2)
        gallery_append_range(specimen 9984 10175 2)
        gallery_append_range(specimen 57344 63743 4)
    else()
        # Icon/status faces such as newRPL Notification often use ordinary
        # ASCII codepoints for pictograms. Sample the face's real cmap instead
        # of asking only for Unicode/PUA feature ranges.
        foreach(cp IN LISTS available)
            if(cp GREATER_EQUAL 32)
                list(APPEND specimen "${cp}")
            endif()
        endforeach()
        if("${specimen}" STREQUAL "")
            set(specimen ${available})
        endif()
    endif()

    list(REMOVE_DUPLICATES specimen)
    list(LENGTH specimen specimen_count)
    if(specimen_count GREATER GALLERY_SPECIMEN_MAX)
        list(SUBLIST specimen 0 ${GALLERY_SPECIMEN_MAX} specimen)
    endif()
    list(APPEND selected ${specimen})
    list(REMOVE_DUPLICATES selected)

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
    string(REPLACE "-" " " face_label "${face_label}")
    string(REPLACE "_" " " face_label "${face_label}")

    set(face_prefix "${family_label} ")
    string(LENGTH "${face_prefix}" face_prefix_len)
    string(SUBSTRING "${face_label}" 0 ${face_prefix_len} face_prefix_actual)
    if(face_prefix_actual STREQUAL face_prefix)
        string(SUBSTRING "${face_label}" ${face_prefix_len} -1 face_label)
    endif()

    if(font_id MATCHES "^ark_pixel_ark_pixel_(10|12|16)px_(monospaced|proportional)_(ja|ko|latin|zh_cn|zh_hk|zh_tr|zh_tw)$")
        set(ark_size "${CMAKE_MATCH_1}")
        set(ark_mode "${CMAKE_MATCH_2}")
        set(ark_locale "${CMAKE_MATCH_3}")
        if(ark_mode STREQUAL "monospaced")
            set(ark_mode_label "Mono")
        else()
            set(ark_mode_label "Prop")
        endif()
        if(ark_locale STREQUAL "ja")
            set(ark_locale_label "Japanese")
        elseif(ark_locale STREQUAL "ko")
            set(ark_locale_label "Korean")
        elseif(ark_locale STREQUAL "latin")
            set(ark_locale_label "Latin")
        elseif(ark_locale STREQUAL "zh_cn")
            set(ark_locale_label "CN Mainland")
        elseif(ark_locale STREQUAL "zh_hk")
            set(ark_locale_label "HK")
        elseif(ark_locale STREQUAL "zh_tw")
            set(ark_locale_label "TW")
        else()
            set(ark_locale_label "Traditional")
        endif()
        set(family_label "Ark Pixel")
        set(face_label "${ark_size}px ${ark_mode_label} ${ark_locale_label}")
    endif()

    string(REPLACE "\\" "\\\\" family_label_c "${family_label}")
    string(REPLACE "\"" "\\\"" family_label_c "${family_label_c}")
    string(REPLACE "\\" "\\\\" face_label_c "${face_label}")
    string(REPLACE "\"" "\\\"" face_label_c "${face_label_c}")

    file(APPEND "${catalog_c}"
        "extern const struct hpclang_font ${symbol};\n")
    if(NOT "${specimen}" STREQUAL "")
        file(APPEND "${catalog_c}"
            "static const uint32_t specimen_${face_index}[] = {")
        foreach(cp IN LISTS specimen)
            file(APPEND "${catalog_c}" "${cp}u,")
        endforeach()
        file(APPEND "${catalog_c}" "};\n")
        list(LENGTH specimen specimen_count)
        set(specimen_pointer "specimen_${face_index}")
    else()
        set(specimen_count 0)
        set(specimen_pointer "0")
    endif()

    string(APPEND catalog_rows
        "    {\"${family_label_c}\", \"${face_label_c}\", &${symbol}, ${specimen_pointer}, ${specimen_count}u, ${text_capable}u, ${specimen_profile}u},\n")
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
    "Font Gallery (${HPCLANG_GALLERY_PROFILE}, shard ${HPCLANG_GALLERY_SHARD_INDEX}/${HPCLANG_GALLERY_SHARD_COUNT}): ${face_index} generated faces from ${family_count} families + 2 native faces")

target_link_options(hpclang_font_gallery PRIVATE
    -nostartfiles
    -Wl,--gc-sections
    -Wl,--icf=all
    -Wl,--ignore-data-address-equality
    "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_font_gallery PROPERTIES
    OUTPUT_NAME hpclang-font-gallery
    SUFFIX ".elf")
