cmake_minimum_required(VERSION 3.20)
project(hpclang_port_recovery_witness LANGUAGES C ASM)
option(HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY
    "Stop the witness before publishing a restored bank identity" OFF)
option(HPCLANG_RECOVERY_RETIRE_JOURNAL
    "Retire the verified journal and check repeated retirement without writes" OFF)
option(HPCLANG_RECOVERY_RESTART_AFTER_REPAIR
    "Request Stock reinitialization after a successfully retired recovery" OFF)
set(HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT "" CACHE STRING
    "Optional injected journal address; its one-use authority immediately follows it")
if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "" AND
        NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT MATCHES "^(0[xX][0-9a-fA-F]+|[0-9]+)$")
    message(FATAL_ERROR "Bound journal slot must be an integer address")
endif()
if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "" AND
        (NOT HPCLANG_RECOVERY_RETIRE_JOURNAL OR NOT HPCLANG_RECOVERY_RESTART_AFTER_REPAIR))
    message(FATAL_ERROR "Bound configuration witness requires retirement and restart")
endif()
if(HPCLANG_RECOVERY_RESTART_AFTER_REPAIR AND
        (NOT HPCLANG_RECOVERY_RETIRE_JOURNAL OR HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY))
    message(FATAL_ERROR "Recovery restart requires retirement without an interrupted witness")
endif()

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
if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "")
    target_compile_definitions(recovery PRIVATE
        HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT=${HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT})
endif()
if(HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY)
    target_compile_definitions(recovery PRIVATE HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY=1)
endif()
if(HPCLANG_RECOVERY_RETIRE_JOURNAL)
    target_compile_definitions(recovery PRIVATE HPCLANG_RECOVERY_RETIRE_JOURNAL=1)
endif()
if(HPCLANG_RECOVERY_RESTART_AFTER_REPAIR)
    target_compile_definitions(recovery PRIVATE HPCLANG_RECOVERY_RESTART_AFTER_REPAIR=1)
endif()
target_compile_options(recovery PRIVATE -Oz -ffreestanding -fno-builtin -fpie
    -fvisibility=hidden -ffunction-sections -fdata-sections)
target_link_options(recovery PRIVATE -nostdlib -Wl,--gc-sections -Wl,--emit-relocs
    -Wl,--pic-veneer -Wl,--orphan-handling=error "-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/link.ld")
target_link_libraries(recovery PRIVATE "${BUILTINS}")
set_target_properties(recovery PROPERTIES SUFFIX ".elf"
    LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/link.ld")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp"
        "${CMAKE_CURRENT_BINARY_DIR}/recovery.bin"
    COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-recovery.py"
        --elf --model hp50g --binary-output "${CMAKE_CURRENT_BINARY_DIR}/recovery.bin"
        --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
        "$<TARGET_FILE:recovery>" "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp"
    DEPENDS recovery "${HPCLANG_ROOT}/tools/package-recovery.py"
    VERBATIM)
add_custom_target(package ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/recovery.hp"
    "${CMAKE_CURRENT_BINARY_DIR}/recovery.bin")
if(HPCLANG_RECOVERY_RESTART_AFTER_REPAIR)
    # This ID belongs to the fresh Stock fixture, not a production reservation.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/recovery-library.hp"
            "${CMAKE_CURRENT_BINARY_DIR}/recovery-configuration.hp"
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-recovery.py"
            --elf --model hp50g --library-id 1701 --flash-copy
            --program-output "${CMAKE_CURRENT_BINARY_DIR}/recovery-configuration.hp"
            --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
            "$<TARGET_FILE:recovery>" "${CMAKE_CURRENT_BINARY_DIR}/recovery-library.hp"
        DEPENDS recovery "${HPCLANG_ROOT}/tools/package-recovery.py"
        VERBATIM)
    add_custom_target(configuration-package ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/recovery-library.hp"
            "${CMAKE_CURRENT_BINARY_DIR}/recovery-configuration.hp")
endif()
