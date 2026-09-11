include_guard(GLOBAL)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(HPCLANG_FONT_PACKAGE
    "${HPCLANG_ROOT}/sdk/fonts/fonts.zip"
    CACHE FILEPATH "Canonical hpclang source-font package")

function(hpclang_target_font target)
    set(options)
    set(oneValueArgs NAME FONT MODE)
    set(multiValueArgs TEXT CHARSET_FILES)
    cmake_parse_arguments(HF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HF_NAME OR NOT HF_FONT)
        message(FATAL_ERROR "hpclang_target_font requires NAME and FONT")
    endif()
    if(NOT HF_MODE)
        set(HF_MODE STATIC)
    endif()
    if(NOT HF_MODE STREQUAL "STATIC" AND NOT HF_MODE STREQUAL "FULL")
        message(FATAL_ERROR "hpclang_target_font MODE must be STATIC or FULL")
    endif()

    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/hpclang-fonts")
    set(out_c "${out_dir}/${HF_NAME}.c")
    set(args
        --package "${HPCLANG_FONT_PACKAGE}"
        compile "${HF_FONT}"
        --mode "${HF_MODE}"
        --symbol "${HF_NAME}"
        --output "${out_c}")

    foreach(text IN LISTS HF_TEXT)
        list(APPEND args --text "${text}")
    endforeach()
    foreach(file IN LISTS HF_CHARSET_FILES)
        list(APPEND args --charset-file "${file}")
    endforeach()

    add_custom_command(
        OUTPUT "${out_c}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/hpclang-font.py" ${args}
        DEPENDS
            "${HPCLANG_ROOT}/tools/hpclang-font.py"
            "${HPCLANG_FONT_PACKAGE}"
            ${HF_CHARSET_FILES}
        VERBATIM
        COMMENT "Compiling ${HF_FONT} (${HF_MODE}) for ${target}")

    target_sources(${target} PRIVATE "${out_c}")
endfunction()
