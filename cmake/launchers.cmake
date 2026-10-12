# Package an application through the production ARMToolbox bootstrap.
function(hpclang_target_launchers target)
    cmake_parse_arguments(PARSE_ARGV 1 launcher "" "" "MODELS")
    if(launcher_UNPARSED_ARGUMENTS OR launcher_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "hpclang_target_launchers expects MODELS followed by model names")
    endif()
    if(NOT TARGET ${target}_hpc)
        message(FATAL_ERROR "Create ${target} with hpclang_add_application first")
    endif()
    if(NOT launcher_MODELS)
        set(launcher_MODELS hp39gplus hp39gs hp40gs hp48gii hp49gplus hp50g)
    endif()
    set(supported hp39gplus hp39gs hp40gs hp48gii hp49gplus hp50g)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${target}-launchers")
    set(outputs)
    foreach(model IN LISTS launcher_MODELS)
        if(NOT model IN_LIST supported)
            message(FATAL_ERROR "Unsupported calculator model: ${model}")
        endif()
        list(APPEND outputs "${output}/${model}.bin" "${output}/${model}.hp")
    endforeach()
    get_filename_component(tool_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    file(GLOB bootstrap_sources CONFIGURE_DEPENDS
        "${HPCLANG_ROOT}/runtime/bootstrap/*"
        "${HPCLANG_ROOT}/platform/armtoolbox/*"
        "${HPCLANG_ROOT}/include/hpclang/*.h")
    add_custom_command(OUTPUT ${outputs}
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/build-launchers.py"
            "${CMAKE_CURRENT_BINARY_DIR}/${target}.hpc" "${output}"
            --tool-bin "${tool_bin}" --models ${launcher_MODELS}
        DEPENDS ${target}_hpc "${CMAKE_CURRENT_BINARY_DIR}/${target}.hpc"
            "${HPCLANG_ROOT}/tools/build-launchers.py"
            "${HPCLANG_ROOT}/tools/armtoolbox-wrap.py" ${bootstrap_sources}
        VERBATIM)
    add_custom_target(${target}_launchers ALL DEPENDS ${outputs})
endfunction()
