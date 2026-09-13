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

# Full Gallery is the exhaustive font-validation payload used by Direct E2E.
# Stock E2E validates the real calculator UI -> ARMToolbox -> HPC1 launch path,
# so it uses one generated face in addition to the two native faces. Keeping
# those responsibilities separate avoids making the stock launcher carry
# hundreds of font assets that do not add launcher coverage.
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
    list(GET GALLERY_FONT_IDS 0 GALLERY_STOCK_FACE)
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

# Keep full printable ASCII so the Gallery can use diagnostic type-specimen
# strings (Il1|, 0OQ, rn/m/w, punctuation) without silently falling back.
set(GALLERY_CODEPOINTS
    32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47
    48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63
    64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79
    80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95
    96 97 98 99 100 101 102 103 104 105 106 107 108 109 110 111
    112 113 114 115 116 117 118 119 120 121 122 123 124 125 126
)

# Common, visually diagnostic Unicode probes. Prefer recognizable characters
# over arbitrary early codepoints from a script block.
set(GALLERY_UNICODE_CODEPOINTS
    # Accented Latin and common currency/symbols.
    192 193 196 199 201 209 214 220 223 230 248 339
    163 165 169 176 177 181 215 247 8364
    # Kana and Hangul.
    12354 12356 12358 12360 12362 12450 12452 12454 12456 12458
    44032 45208 45796 54620 44544
    # Common Han, including simplified/traditional shape contrasts.
    22825 22320 20154 20013 25991 23383 20307 39636
    38376 38272 39118 39080 40857 40845 35821 35486 35328
    22269 22283 21488 33274 28286 28771
    # Greek, mathematics, arrows, and box drawing.
    937 945 946 955 960
    8730 8734 8776 8800 8804 8805 8592 8593 8594 8595 8596
    9472 9474 9484 9488 9492 9496
)
list(APPEND GALLERY_CODEPOINTS ${GALLERY_UNICODE_CODEPOINTS})

# Keep the historically useful Powerline sample globally. Controller glyphs
# are sampled only from BoutiqueBitmap9x9, whose ordinary mapping uses the
# E8xx/E9xx/EAxx PUA pages and whose HK remap uses EFxx/F0xx/F1xx. PUA glyphs
# remain secondary feature repertoire for text-capable faces instead of
# replacing their typography specimen.
set(GALLERY_FIXED_PRIVATE_CODEPOINTS
    57504 57520 57521 57522 57523
)
set(GALLERY_CONTROLLER_PUA_PAGES
    232 233 234 239 240 241
)
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
"    uint32_t text_capable;\n"
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

    # A face is typography-capable when its real cmap has whitespace, a digit,
    # and at least two alphabetic positions. This deliberately uses repertoire
    # rather than family names: text fonts that also ship Powerline/controller
    # PUA glyphs must still behave like text fonts in the Gallery.
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

    set(selected_private)

    # Retain real PUA mappings in the linked face. Unlike the old catalog, keep
    # those codepoints visible to the Gallery even when the same face is also a
    # usable text font: controller and patched-icon repertoires are precisely
    # the features the Gallery is meant to expose.
    foreach(cp IN LISTS GALLERY_FIXED_PRIVATE_CODEPOINTS)
        list(FIND available "${cp}" private_found)
        if(NOT private_found EQUAL -1)
            list(APPEND selected "${cp}")
            list(APPEND selected_private "${cp}")
        endif()
    endforeach()

    # Controller/button repertoires are not Unicode emoji. BoutiqueBitmap9x9
    # is the font family that defines them, so only its faces probe these PUA
    # pages. Take one actual mapped glyph per page instead of hard-coding a
    # particular button codepoint such as E866/E966/EA66.
    if(font_id MATCHES "^boutique_bitmap_9x9_")
        foreach(wanted_page IN LISTS GALLERY_CONTROLLER_PUA_PAGES)
            foreach(cp IN LISTS available)
                if(cp GREATER_EQUAL 57344 AND cp LESS_EQUAL 63743)
                    math(EXPR private_page "${cp} / 256")
                    if(private_page EQUAL wanted_page)
                        list(FIND selected_private "${cp}" private_found)
                        if(private_found EQUAL -1)
                            list(APPEND selected "${cp}")
                            list(APPEND selected_private "${cp}")
                        endif()
                        break()
                    endif()
                endif()
            endforeach()
        endforeach()
    endif()

    list(REMOVE_DUPLICATES selected)
    list(REMOVE_DUPLICATES selected_private)
    list(LENGTH selected_private private_count)
    if(private_count GREATER GALLERY_PRIVATE_MAX)
        list(SUBLIST selected_private 0 ${GALLERY_PRIVATE_MAX} selected_private)
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
    string(REPLACE "-" " " face_label "${face_label}")
    string(REPLACE "_" " " face_label "${face_label}")

    # Avoid spending the entire second chrome row repeating the family name.
    # This is particularly important for Ark Pixel where the distinguishing
    # locale suffix used to be clipped off-screen.
    set(face_prefix "${family_label} ")
    string(LENGTH "${face_prefix}" face_prefix_len)
    string(SUBSTRING "${face_label}" 0 ${face_prefix_len} face_prefix_actual)
    if(face_prefix_actual STREQUAL face_prefix)
        string(SUBSTRING "${face_label}" ${face_prefix_len} -1 face_label)
    endif()

    # Ark Pixel ships seven locale-specific glyph designs at every size/mode.
    # Keep the runtime strings ASCII-only for the calculator chrome, but make
    # size, width mode, and region visible before the label can be clipped.
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
        "    {\"${family_label_c}\", \"${face_label_c}\", &${symbol}, ${unicode_pointer}, ${unicode_count}u, ${private_pointer}, ${private_count}u, ${text_capable}u},\n")
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

# Generated font assets are immutable lookup data and the Gallery never relies
# on glyph/bitmap object address identity. Allow lld to fold equal read-only
# sections here so repeated language/style variants share identical glyphs and
# bitmaps. Keep this Gallery-only: normal SDK applications retain ordinary C
# address-identity semantics.
target_link_options(hpclang_font_gallery PRIVATE
    -nostartfiles
    -Wl,--gc-sections
    -Wl,--icf=all
    -Wl,--ignore-data-address-equality
    "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_font_gallery PROPERTIES
    OUTPUT_NAME hpclang-font-gallery
    SUFFIX ".elf")
