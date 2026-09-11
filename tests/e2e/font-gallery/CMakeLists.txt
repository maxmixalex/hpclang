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

# One compact representative face from every generated font family.  The
# configure-time family audit below intentionally fails when the generated
# catalog gains a new family until the Gallery gets an explicit representative.
set(GALLERY_FACES
    "Ark Pixel|ark_pixel_ark_pixel_10px_proportional_zh_cn"
    "Bitbuntu|bitbuntu_bitbuntu"
    "Bitocra|bitocra_bitocra"
    "Boutique 7x7|boutique_bitmap_7x7_boutiquebitmap7x7_1_71"
    "Boutique 9x9|boutique_bitmap_9x9_boutiquebitmap9x9_1_93"
    "Cherry|cherry_cherry_10_r"
    "Cozette|cozette_cozette"
    "Dina|dina_dina_r400_8"
    "Dylex|dylex_6x10"
    "Fusion Pixel|fusion_pixel_fusion_pixel_8px_proportional_zh_hans"
    "Galmuri|galmuri_galmuri11_condensed"
    "Gohufont|gohufont_gohufont_uni_11"
    "Hasan Kazan|hasankazan_nokiasmallplain"
    "JapanYoshi|japanyoshi_pixeltheatre"
    "newRPL|newrpl_8c"
    "PixelDroid|pixeldroid_pixeldroidconsoleregular"
    "ProFont|profont_profont10"
    "Progsole|progsole_progsole_10"
    "Retro Pixel|retro_pixel_retro_pixel_cute_prop"
    "Spleen|spleen_spleen_5x8"
    "SpleenTT|spleentt_spleenttmedium_8"
    "Squeezed|squeezed_u8g2_squeezed_regular_6"
    "Tamzen|tamzen_tamzen6x12r"
    "Terminus|terminus_ter_u12n"
    "Tiny5|tiny5_tiny5_regular"
    "Tom Thumb|tom_thumb_tom_thumb"
    "X11 Fixed|x11_fixed_6x10"
    "X11 Micro|x11_micro_micro"
)

# Printable ASCII is the common comparison repertoire.  The second group is a
# deliberately broad Unicode probe: Latin-1, Greek/math, arrows/blocks, CJK,
# Hangul, Powerline/private-use symbols, and the Unicode game-controller emoji.
# Each face receives only the intersection with its real generated mapping.
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
    57504 57520 57521 57522 57523
    127918
)
list(APPEND GALLERY_CODEPOINTS ${GALLERY_UNICODE_CODEPOINTS})

function(gallery_family_root out font_id)
    set(selector_var "HPCLANG_FONT_${font_id}_SELECTOR")
    if(NOT DEFINED ${selector_var})
        message(FATAL_ERROR "Font Gallery references unknown font id: ${font_id}")
    endif()
    string(REPLACE "/" ";" selector_parts "${${selector_var}}")
    list(GET selector_parts 0 family_root)
    set(${out} "${family_root}" PARENT_SCOPE)
endfunction()

set(all_family_roots)
foreach(font_id IN LISTS HPCLANG_FONT_IDS)
    gallery_family_root(family_root "${font_id}")
    list(APPEND all_family_roots "${family_root}")
endforeach()
list(REMOVE_DUPLICATES all_family_roots)
list(SORT all_family_roots)

set(selected_family_roots)
foreach(entry IN LISTS GALLERY_FACES)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 1 font_id)
    gallery_family_root(family_root "${font_id}")
    list(APPEND selected_family_roots "${family_root}")
endforeach()
list(REMOVE_DUPLICATES selected_family_roots)
list(SORT selected_family_roots)

set(missing_families)
foreach(family_root IN LISTS all_family_roots)
    list(FIND selected_family_roots "${family_root}" found)
    if(found EQUAL -1)
        list(APPEND missing_families "${family_root}")
    endif()
endforeach()
if(missing_families)
    message(FATAL_ERROR
        "Font Gallery is missing generated font families: ${missing_families}")
endif()

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
"};\n"
"extern const struct hpclang_gallery_face hpclang_gallery_faces[];\n"
"extern const uint32_t hpclang_gallery_face_count;\n"
"#endif\n")
file(WRITE "${catalog_c}"
"#include <stdint.h>\n"
"#include \"font-gallery-catalog.h\"\n\n")

set(catalog_rows "")
set(face_index 0)
foreach(entry IN LISTS GALLERY_FACES)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 family_label)
    list(GET parts 1 font_id)

    set(selector_var "HPCLANG_FONT_${font_id}_SELECTOR")
    set(codepoints_var "HPCLANG_FONT_${font_id}_CODEPOINTS")
    set(codepoints_file_var "HPCLANG_FONT_${font_id}_CODEPOINTS_FILE")
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

    string(APPEND catalog_rows
        "    {\"${family_label_c}\", \"${face_label_c}\", &${symbol}, ${unicode_pointer}, ${unicode_count}u},\n")
    math(EXPR face_index "${face_index} + 1")
endforeach()

file(APPEND "${catalog_c}"
"\nconst struct hpclang_gallery_face hpclang_gallery_faces[] = {\n"
"${catalog_rows}"
"};\n"
"const uint32_t hpclang_gallery_face_count =\n"
"    (uint32_t)(sizeof(hpclang_gallery_faces) / sizeof(hpclang_gallery_faces[0]));\n")
target_sources(hpclang_font_gallery PRIVATE "${catalog_c}")

message(STATUS
    "Font Gallery: ${face_index} generated family representatives + 2 native faces")

target_link_options(hpclang_font_gallery PRIVATE
    -nostartfiles -Wl,--gc-sections "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
set_target_properties(hpclang_font_gallery PROPERTIES
    OUTPUT_NAME hpclang-font-gallery
    SUFFIX ".elf")
