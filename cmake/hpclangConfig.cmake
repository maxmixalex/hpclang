if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "armv4t" OR
   NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Use the installed hpclang-armv4t.cmake toolchain with Arm Toolchain for Embedded")
endif()
get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/profiles/hpclang-v1.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/fonts.cmake")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Compile once per downstream build, using exactly its selected toolchain.
# The source SDK does not ship compiler-version-dependent prebuilt archives.
if(NOT TARGET hpclang::runtime)
    add_library(hpclang_runtime STATIC
        ${HPCLANG_RUNTIME} ${HPCLANG_RUNTIME_EABI_SHIMS}
        ${HPCLANG_RUNTIME_SOFTFLOAT} ${HPCLANG_LIBC})
    add_library(hpclang::runtime ALIAS hpclang_runtime)
    target_include_directories(hpclang_runtime PUBLIC "${HPCLANG_ROOT}/include"
        PRIVATE "${HPCLANG_ROOT}/runtime/core")
    target_compile_features(hpclang_runtime PUBLIC c_std_11)
    target_compile_options(hpclang_runtime PRIVATE -ffunction-sections -fdata-sections)
endif()

function(hpclang_add_application target)
    add_executable(${target} ${ARGN} "${HPCLANG_RUNTIME_START}")
    target_link_libraries(${target} PRIVATE hpclang::runtime)
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${target} PRIVATE -nostartfiles -Wl,--gc-sections
        "-Wl,-T,${HPCLANG_LINKER_SCRIPT}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${HPCLANG_LINKER_SCRIPT}")
    set_target_properties(${target} PROPERTIES SUFFIX ".elf")
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}.hpc"
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_PACKER}"
            "$<TARGET_FILE:${target}>" "${CMAKE_CURRENT_BINARY_DIR}/${target}.hpc"
        DEPENDS ${target} "${HPCLANG_PACKER}"
        VERBATIM)
    add_custom_target(${target}_hpc ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${target}.hpc")
endfunction()
