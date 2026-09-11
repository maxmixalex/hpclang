include_guard(GLOBAL)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

include("${HPCLANG_ROOT}/sdk/fonts/generated/index.cmake")

function(_hpclang_font_resolve out_id selector)
    if(selector STREQUAL "@mini")
        set(id newrpl_6a)
    elseif(selector STREQUAL "@normal")
        set(id newrpl_8c)
    elseif(selector STREQUAL "@large")
        set(id newrpl_10a)
    else()
        set(id "${selector}")
    endif()
    string(MAKE_C_IDENTIFIER "${id}" id)
    set(${out_id} "${id}" PARENT_SCOPE)
endfunction()

function(_hpclang_font_codepoint out_value input)
    if("${input}" MATCHES "^U\\+([0-9A-Fa-f]+)$")
        set(expr "0x${CMAKE_MATCH_1}")
    else()
        set(expr "${input}")
    endif()
    math(EXPR value "${expr}")
    if(value LESS 0 OR value GREATER 1114111)
        message(FATAL_ERROR "font code point out of Unicode range: ${input}")
    endif()
    set(${out_value} "${value}" PARENT_SCOPE)
endfunction()

function(hpclang_target_font target)
    set(options)
    set(oneValueArgs NAME FONT MODE FALLBACK)
    set(multiValueArgs CODEPOINTS)
    cmake_parse_arguments(HF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HF_NAME OR NOT HF_FONT)
        message(FATAL_ERROR "hpclang_target_font requires NAME and FONT")
    endif()
    if(NOT HF_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "hpclang_target_font NAME must be a C identifier")
    endif()

    if(NOT HF_MODE)
        set(HF_MODE STATIC)
    endif()
    if(NOT HF_MODE STREQUAL "STATIC" AND NOT HF_MODE STREQUAL "FULL")
        message(FATAL_ERROR "hpclang_target_font MODE must be STATIC or FULL")
    endif()

    _hpclang_font_resolve(font_id "${HF_FONT}")
    set(source_var "HPCLANG_FONT_${font_id}_SOURCE")
    if(NOT DEFINED ${source_var})
        message(FATAL_ERROR "unknown pre-generated hpclang font: ${HF_FONT}")
    endif()
    set(font_source "${${source_var}}")

    set(ascent_var "HPCLANG_FONT_${font_id}_ASCENT")
    set(descent_var "HPCLANG_FONT_${font_id}_DESCENT")
    set(line_var "HPCLANG_FONT_${font_id}_LINE_HEIGHT")
    set(advance_var "HPCLANG_FONT_${font_id}_NOMINAL_ADVANCE")
    set(codepoints_var "HPCLANG_FONT_${font_id}_CODEPOINTS")
    set(available "${${codepoints_var}}")

    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/hpclang-fonts")
    file(MAKE_DIRECTORY "${out_dir}")
    set(out_c "${out_dir}/${HF_NAME}.c")

    target_sources(${target} PRIVATE "${font_source}")

    if(HF_MODE STREQUAL "FULL")
        if(HF_CODEPOINTS)
            message(FATAL_ERROR "FULL font selection does not accept CODEPOINTS")
        endif()
        file(WRITE "${out_c}"
"#include <hpclang/font-package.h>\n"
"extern const struct hpclang_bitmap_font_data hpclang_font_${font_id}_full_data;\n"
"const struct hpclang_font ${HF_NAME} = HPCLANG_BITMAP_FONT(hpclang_font_${font_id}_full_data);\n")
        target_sources(${target} PRIVATE "${out_c}")
        return()
    endif()

    if(NOT HF_CODEPOINTS)
        message(FATAL_ERROR "STATIC font selection requires CODEPOINTS")
    endif()

    set(selected)
    foreach(raw IN LISTS HF_CODEPOINTS)
        _hpclang_font_codepoint(cp "${raw}")
        list(FIND available "${cp}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${HF_FONT} has no mapping for code point ${raw}")
        endif()
        list(APPEND selected "${cp}")
    endforeach()

    if(NOT DEFINED HF_FALLBACK OR HF_FALLBACK STREQUAL "AUTO")
        list(FIND available "63" question_mark)
        if(NOT question_mark EQUAL -1)
            set(fallback 63)
            list(APPEND selected 63)
        else()
            set(fallback 4294967295)
        endif()
    elseif(HF_FALLBACK STREQUAL "NONE")
        set(fallback 4294967295)
    else()
        _hpclang_font_codepoint(fallback "${HF_FALLBACK}")
        list(FIND available "${fallback}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${HF_FONT} has no fallback mapping for ${HF_FALLBACK}")
        endif()
        list(APPEND selected "${fallback}")
    endif()

    list(REMOVE_DUPLICATES selected)
    list(SORT selected COMPARE NATURAL ORDER ASCENDING)

    file(WRITE "${out_c}" "#include <hpclang/font-package.h>\n\n")
    foreach(cp IN LISTS selected)
        file(APPEND "${out_c}"
"extern const struct hpclang_bitmap_glyph hpclang_font_${font_id}_cp${cp};\n")
    endforeach()

    file(APPEND "${out_c}"
"\nstatic const struct hpclang_bitmap_glyph *const ${HF_NAME}_glyphs[] = {\n")
    foreach(cp IN LISTS selected)
        file(APPEND "${out_c}" "    &hpclang_font_${font_id}_cp${cp},\n")
    endforeach()

    list(LENGTH selected glyph_count)
    file(APPEND "${out_c}"
"};\n\n"
"static const struct hpclang_bitmap_font_data ${HF_NAME}_data = {\n"
"    ${HF_NAME}_glyphs, ${glyph_count}u, ${fallback}u,\n"
"    ${${ascent_var}}, ${${descent_var}}, ${${line_var}}u, ${${advance_var}}u\n"
"};\n"
"const struct hpclang_font ${HF_NAME} = HPCLANG_BITMAP_FONT(${HF_NAME}_data);\n")

    target_sources(${target} PRIVATE "${out_c}")
endfunction()
