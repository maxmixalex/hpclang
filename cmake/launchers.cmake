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
    set_property(TARGET ${target} PROPERTY HPCLANG_LAUNCHER_MODELS "${launcher_MODELS}")
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

# Historical code and calculator assets are caller-supplied dependencies.
function(hpclang_target_aplet target)
    cmake_parse_arguments(PARSE_ARGV 1 aplet "" "MODEL;NAME;AUTHOR;TOOL;ASSETS" "")
    if(aplet_UNPARSED_ARGUMENTS OR aplet_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid hpclang_target_aplet arguments")
    endif()
    foreach(required IN ITEMS MODEL NAME TOOL ASSETS)
        if(NOT aplet_${required})
            message(FATAL_ERROR "hpclang_target_aplet requires ${required}")
        endif()
    endforeach()
    set(supported hp39gplus hp39gs hp40gs)
    get_target_property(models ${target} HPCLANG_LAUNCHER_MODELS)
    if(NOT aplet_MODEL IN_LIST supported OR NOT aplet_MODEL IN_LIST models)
        message(FATAL_ERROR "Enable the ${aplet_MODEL} launcher before packaging its Aplet")
    endif()
    if(NOT aplet_AUTHOR)
        set(aplet_AUTHOR hpclang)
    endif()
    get_filename_component(aplet_TOOL "${aplet_TOOL}" ABSOLUTE)
    get_filename_component(aplet_ASSETS "${aplet_ASSETS}" ABSOLUTE)
    set(dependencies "${aplet_TOOL}")
    foreach(asset IN ITEMS apletcode.bin apletepilog.bin blank.gro LIB275L.L39 HPCRESET.000)
        list(APPEND dependencies "${aplet_ASSETS}/${asset}")
    endforeach()
    foreach(dependency IN LISTS dependencies)
        if(NOT EXISTS "${dependency}")
            message(FATAL_ERROR "Missing Aplet packaging dependency: ${dependency}")
        endif()
    endforeach()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${target}-aplet/${aplet_MODEL}")
    set(outputs)
    foreach(filename IN ITEMS HPGCC000.000 LIB275L.000 HPCRESET.000 HP39DIR.000 HP39DIR.CUR)
        list(APPEND outputs "${output}/${filename}")
    endforeach()
    set(payload "${CMAKE_CURRENT_BINARY_DIR}/${target}-launchers/${aplet_MODEL}.hp")
    add_custom_command(OUTPUT ${outputs}
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-aplet.py"
            "${payload}" "${output}" --tool "${aplet_TOOL}" --assets "${aplet_ASSETS}"
            --name "${aplet_NAME}" --author "${aplet_AUTHOR}"
        DEPENDS ${target}_launchers "${payload}" ${dependencies}
            "${HPCLANG_ROOT}/tools/package-aplet.py"
        VERBATIM)
    add_custom_target(${target}_${aplet_MODEL}_aplet ALL DEPENDS ${outputs})
endfunction()
