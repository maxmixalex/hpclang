cmake_minimum_required(VERSION 3.20)
project(hpclang_port_recovery_witness LANGUAGES C ASM)
option(HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY
    "Stop the witness before publishing a restored bank identity" OFF)

get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(TOOL_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(LLVM_NM llvm-nm HINTS "${TOOL_BIN}" REQUIRED)
find_program(LLVM_READELF llvm-readelf HINTS "${TOOL_BIN}" REQUIRED)
find_program(LLVM_OBJCOPY llvm-objcopy HINTS "${TOOL_BIN}" REQUIRED)
execute_process(
    COMMAND "${CMAKE_C_COMPILER}" --target=arm-none-eabi -march=armv4t -mthumb
        -print-libgcc-file-name
    OUTPUT_VARIABLE BUILTINS OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
if(NOT EXISTS "${BUILTINS}")
    message(FATAL_ERROR "The ARMv4T compiler runtime is unavailable: ${BUILTINS}")
endif()

set(CORE "${HPCLANG_ROOT}/runtime/core")
add_executable(recovery
    entry.S main.c "${CORE}/flash-mapping.S"
    "${HPCLANG_ROOT}/runtime/bootstrap/sync.S")
foreach(module IN ITEMS rpl-flash-record rpl-flash-write rpl-flash-replay
        rpl-backup rpl-port rpl-object-span time time-math rtc rtc-decode datetime sst-flash)
    target_sources(recovery PRIVATE "${CORE}/${module}.c")
endforeach()
target_include_directories(recovery PRIVATE "${CORE}" "${HPCLANG_ROOT}/include")
target_compile_features(recovery PRIVATE c_std_11)
if(HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY)
    target_compile_definitions(recovery PRIVATE HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY=1)
endif()
target_compile_options(recovery PRIVATE -Oz -ffreestanding -fno-builtin -fpie
    -fvisibility=hidden -ffunction-sections -fdata-sections)
target_link_options(recovery PRIVATE -nostdlib -Wl,--gc-sections -Wl,--emit-relocs
    -Wl,--pic-veneer -Wl,--orphan-handling=error "-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/link.ld")
target_link_libraries(recovery PRIVATE "${BUILTINS}")
set_target_properties(recovery PROPERTIES SUFFIX ".elf"
    LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/link.ld")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/package.py"
        --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
        "$<TARGET_FILE:recovery>" "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp"
    DEPENDS recovery package.py "${HPCLANG_ROOT}/tools/package-recovery.py"
    VERBATIM)
add_custom_target(package ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp")
