include_guard(GLOBAL)

if(NOT DEFINED HPCLANG_ROOT)
    get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

include("${HPCLANG_ROOT}/cmake/ui-text.cmake")
include("${HPCLANG_ROOT}/runtime/fonts/generated/index.cmake")

function(_hpclang_font_resolve out_id selector)
    if(selector STREQUAL "@mini")
        set(id newrpl_6a)
    elseif(selector STREQUAL "@text" OR selector STREQUAL "@normal")
        set(id newrpl_8c)
    elseif(selector STREQUAL "@edit-large" OR selector STREQUAL "@large")
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

function(_hpclang_font_target_sources out target)
    get_target_property(_sources ${target} SOURCES)
    get_target_property(_source_dir ${target} SOURCE_DIR)
    if(NOT _sources)
        set(${out} "" PARENT_SCOPE)
        return()
    endif()

    set(_scan)
    foreach(_source IN LISTS _sources)
        if(_source MATCHES "^\\$<")
            continue()
        endif()
        get_filename_component(_ext "${_source}" EXT)
        string(TOLOWER "${_ext}" _ext)
        if(NOT _ext MATCHES "^\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$")
            continue()
        endif()
        if(IS_ABSOLUTE "${_source}")
            set(_absolute "${_source}")
        else()
            get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${_source_dir}")
        endif()
        if(EXISTS "${_absolute}")
            list(APPEND _scan "${_absolute}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _scan)
    set(${out} "${_scan}" PARENT_SCOPE)
endfunction()

function(hpclang_target_font target)
    set(options)
    set(oneValueArgs NAME FONT MODE FALLBACK)
    set(multiValueArgs CODEPOINTS)
    cmake_parse_arguments(HF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "hpclang_target_font target does not exist: ${target}")
    endif()
    if(NOT HF_NAME OR NOT HF_FONT)
        message(FATAL_ERROR "hpclang_target_font requires NAME and FONT")
    endif()
    if(NOT HF_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "hpclang_target_font NAME must be a C identifier")
    endif()

    if(NOT HF_MODE)
        set(HF_MODE STATIC_AUTO)
    endif()
    if(NOT HF_MODE STREQUAL "STATIC_AUTO" AND
       NOT HF_MODE STREQUAL "STATIC_EXPLICIT" AND
       NOT HF_MODE STREQUAL "FULL")
        message(FATAL_ERROR
            "hpclang_target_font MODE must be STATIC_AUTO, STATIC_EXPLICIT, or FULL")
    endif()

    _hpclang_font_resolve(font_id "${HF_FONT}")

    # Large generated faces may be split over several assembly translation
    # units. Older in-flight catalogs used a singular SOURCE variable, which is
    # still accepted while this branch is being consolidated.
    set(sources_var "HPCLANG_FONT_${font_id}_SOURCES")
    set(source_var "HPCLANG_FONT_${font_id}_SOURCE")
    if(DEFINED ${sources_var})
        set(font_sources "${${sources_var}}")
    elseif(DEFINED ${source_var})
        set(font_sources "${${source_var}}")
    else()
        message(FATAL_ERROR "unknown pre-generated hpclang font: ${HF_FONT}")
    endif()

    set(ascent_var "HPCLANG_FONT_${font_id}_ASCENT")
    set(descent_var "HPCLANG_FONT_${font_id}_DESCENT")
    set(line_var "HPCLANG_FONT_${font_id}_LINE_HEIGHT")
    set(advance_var "HPCLANG_FONT_${font_id}_NOMINAL_ADVANCE")
    set(codepoints_var "HPCLANG_FONT_${font_id}_CODEPOINTS")
    set(codepoints_file_var "HPCLANG_FONT_${font_id}_CODEPOINTS_FILE")

    if(DEFINED ${codepoints_file_var})
        include("${${codepoints_file_var}}")
    endif()
    if(NOT DEFINED ${codepoints_var})
        message(FATAL_ERROR "pre-generated hpclang font has no codepoint catalog: ${HF_FONT}")
    endif()
    set(available "${${codepoints_var}}")

    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/hpclang-fonts")
    file(MAKE_DIRECTORY "${out_dir}")
    set(out_c "${out_dir}/${HF_NAME}.c")

    target_sources(${target} PRIVATE ${font_sources})

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

    set(selected)
    if(HF_MODE STREQUAL "STATIC_EXPLICIT")
        if(NOT HF_CODEPOINTS)
            message(FATAL_ERROR "STATIC_EXPLICIT font selection requires CODEPOINTS")
        endif()
        foreach(raw IN LISTS HF_CODEPOINTS)
            _hpclang_font_codepoint(cp "${raw}")
            list(APPEND selected "${cp}")
        endforeach()
    else()
        if(HF_CODEPOINTS)
            message(FATAL_ERROR
                "STATIC_AUTO derives its repertoire from HPCLANG_UI_TEXT and does not accept CODEPOINTS")
        endif()
        _hpclang_font_target_sources(ui_sources ${target})
        if(NOT ui_sources)
            message(FATAL_ERROR "STATIC_AUTO target ${target} has no scannable C/C++ sources")
        endif()

        # Source-content changes must refresh the configure-time repertoire.
        # This preserves a normal incremental-build workflow without requiring
        # Python or another host-side font scanner.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ui_sources})
        hpclang_collect_ui_text_codepoints(selected ${ui_sources})
        if(NOT selected)
            message(FATAL_ERROR
                "STATIC_AUTO found no HPCLANG_UI_TEXT(\"...\") literals in target ${target}")
        endif()
    endif()

    list(REMOVE_DUPLICATES selected)
    list(SORT selected COMPARE NATURAL ORDER ASCENDING)
    foreach(cp IN LISTS selected)
        list(FIND available "${cp}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${HF_FONT} has no mapping for code point ${cp}")
        endif()
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
