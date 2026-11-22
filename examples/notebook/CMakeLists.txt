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
