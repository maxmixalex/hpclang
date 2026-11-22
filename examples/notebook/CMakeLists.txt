cmake_minimum_required(VERSION 3.20)
project(hpclang_notebook LANGUAGES C ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(notebook main.c app.c store.c export.c)
# This application prints dates, port indices and integer error codes only.
target_link_options(notebook PRIVATE "LINKER:--defsym=vfprintf=__i_vfprintf")
hpclang_target_launchers(notebook)
set(notebook_ascii)
foreach(codepoint RANGE 32 126)
    list(APPEND notebook_ascii ${codepoint})
endforeach()
hpclang_target_font(notebook NAME notebook_font FONT @mini MODE STATIC_EXPLICIT
    CODEPOINTS ${notebook_ascii} FALLBACK 63)

set(HPCLANG_NOTEBOOK_APLET_ASSETS "" CACHE PATH "Stock assets for the experimental independent HP39/40 Notebook")
if(HPCLANG_NOTEBOOK_APLET_ASSETS)
    if(NOT HPCLANG_BUILD_MODEL MATCHES "^(hp39gplus|hp39gs|hp40gs)$")
        message(FATAL_ERROR "The independent Notebook requires an HP39/40 model-specific build")
    endif()
    set(entry "${HPCLANG_ROOT}/tools/experimental/independent-entry")
    set(stage "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/notebook-independent")
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/independent-aplet")
    set(image "${CMAKE_CURRENT_BINARY_DIR}/notebook.hpc")
    get_filename_component(tool_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    set(outputs)
    foreach(name IN ITEMS HP39DIR.CUR HP39DIR.000 HPGCC000.000 HPCRESET.000)
        list(APPEND outputs "${bundle}/${name}")
    endforeach()
    set(inputs)
    foreach(name IN ITEMS apletcode.bin apletepilog.bin blank.gro HPCRESET.000)
        list(APPEND inputs "${HPCLANG_NOTEBOOK_APLET_ASSETS}/${name}")
    endforeach()
    file(GLOB headers CONFIGURE_DEPENDS "${HPCLANG_ROOT}/include/hpclang/*.h")
    add_custom_command(OUTPUT ${outputs} "${stage}/entry/probe.bin"
            "${stage}/entry/metadata.json" "${stage}/entry/build.json"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${stage}"
        COMMAND "${Python3_EXECUTABLE}" -B "${entry}/build.py" --toolchain "${tool_bin}"
            --model "${HPCLANG_BUILD_MODEL}" --hp39-notebook-editor --aplet-writeback
            --bitmap-image "${image}" --output "${stage}/entry"
        COMMAND "${Python3_EXECUTABLE}" -B "${entry}/build-hp39-aplet.py"
            --assets "${HPCLANG_NOTEBOOK_APLET_ASSETS}" --bitmap-pool "${stage}/entry/probe.bin"
            --bitmap-image "${image}" --initial-note Seed42 --aplet-writeback --output "${stage}/bundle"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${stage}/bundle" "${bundle}"
        DEPENDS notebook_hpc "${image}" ${inputs} ${headers} "${entry}/entry.S" "${entry}/link.ld"
            "${entry}/build.py" "${entry}/build-hp39-aplet.py"
            "${entry}/../independent-pool/hp39-bitmap-probe.c" "${HPCLANG_ROOT}/tools/package-aplet.py"
            "${HPCLANG_ROOT}/runtime/bootstrap/sync.S" "${HPCLANG_ROOT}/runtime/bootstrap/enter.S"
            "${HPCLANG_ROOT}/runtime/bootstrap/mmu.c" "${HPCLANG_ROOT}/runtime/bootstrap/run.c"
        VERBATIM)
    add_custom_target(notebook_independent_aplet DEPENDS ${outputs}
        "${stage}/entry/probe.bin" "${stage}/entry/metadata.json" "${stage}/entry/build.json")
endif()

if(HPCLANG_BUILD_MODEL STREQUAL "hp48gii")
    set(builder "${HPCLANG_ROOT}/tools/experimental/independent-hpc1")
    set(entry "${HPCLANG_ROOT}/tools/experimental/independent-entry")
    set(image "${CMAKE_CURRENT_BINARY_DIR}/notebook.hpc")
    get_filename_component(tool_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    file(GLOB headers CONFIGURE_DEPENDS "${HPCLANG_ROOT}/include/hpclang/*.h")
    function(add_independent_program target filename)
        set(stage "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}")
        set(program "${CMAKE_CURRENT_BINARY_DIR}/${filename}")
        add_custom_command(OUTPUT "${program}"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${stage}"
            COMMAND "${Python3_EXECUTABLE}" -B "${builder}/build.py" --toolchain "${tool_bin}"
                --model hp48gii --notebook "${image}" --preserve-stack ${ARGN} --output "${stage}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${stage}/program.HP" "${program}"
            DEPENDS notebook_hpc "${image}" ${headers} "${builder}/build.py" "${builder}/loader.c"
                "${builder}/app.c" "${builder}/link.ld" "${entry}/entry.S" "${entry}/build.py"
                "${HPCLANG_ROOT}/runtime/bootstrap/sync.S" "${HPCLANG_ROOT}/runtime/bootstrap/enter.S"
                "${HPCLANG_ROOT}/runtime/bootstrap/mmu.c" "${HPCLANG_ROOT}/runtime/bootstrap/run.c"
                "${HPCLANG_ROOT}/platform/armtoolbox/stage-image.c"
                "${HPCLANG_ROOT}/platform/armtoolbox/stage-image.h" "${HPCLANG_ROOT}/third_party/tinf/tinf.h"
            VERBATIM)
        add_custom_target(${target} DEPENDS "${program}")
    endfunction()
    add_independent_program(notebook_independent_program notebook-independent.hp)
    add_independent_program(notebook_independent_port0_program notebook-independent-port0.hp --port0)
elseif(HPCLANG_BUILD_MODEL MATCHES "^(hp49gplus|hp50g)$")
    set(HPCLANG_NOTEBOOK_PORT1_OFFSET 0 CACHE STRING "Port 1 append offset in nibbles")
    set(builder "${HPCLANG_ROOT}/tools/experimental/independent-notebook")
    set(entry "${builder}/../independent-entry")
    set(image "${CMAKE_CURRENT_BINARY_DIR}/notebook.hpc")
    set(stage "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/notebook-independent")
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/notebook-independent")
    get_filename_component(tool_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    set(outputs "${bundle}/USER.HP" "${bundle}/DATA.HP" "${bundle}/COPY.HP")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/notebook-port1-offset.txt"
        CONTENT "@HPCLANG_NOTEBOOK_PORT1_OFFSET@")
    add_custom_command(OUTPUT ${outputs}
        COMMAND "${Python3_EXECUTABLE}" -B "${builder}/build.py" --model "${HPCLANG_BUILD_MODEL}"
            --toolchain "${tool_bin}" --notebook "${image}" --port1-offset "${HPCLANG_NOTEBOOK_PORT1_OFFSET}"
            --output "${stage}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${bundle}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${stage}/USER.HP" "${stage}/DATA.HP" "${stage}/COPY.HP" "${bundle}"
        DEPENDS notebook_hpc "${image}" "${builder}/build.py" "${builder}/acquire.c" "${builder}/loader.h"
            "${entry}/build.py" "${entry}/entry.S" "${entry}/link.ld"
            "${HPCLANG_ROOT}/platform/armtoolbox/stage-stack.c" "${HPCLANG_ROOT}/platform/armtoolbox/stage-stack.h"
            "${HPCLANG_ROOT}/runtime/bootstrap/sync.S" "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/notebook-port1-offset.txt"
        VERBATIM)
    add_custom_target(notebook_independent_program DEPENDS ${outputs})
endif()
