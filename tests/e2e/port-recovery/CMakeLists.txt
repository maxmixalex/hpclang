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
function(configure_recovery_image target)
    target_include_directories(${target} PRIVATE "${CORE}" "${HPCLANG_ROOT}/include")
    target_compile_features(${target} PRIVATE c_std_11)
    target_compile_options(${target} PRIVATE -Oz -ffreestanding -fno-builtin -fpie
        -fvisibility=hidden -ffunction-sections -fdata-sections)
    target_link_options(${target} PRIVATE -nostdlib -Wl,--gc-sections -Wl,--emit-relocs
        -Wl,--pic-veneer -Wl,--orphan-handling=error "-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/link.ld")
    target_link_libraries(${target} PRIVATE "${BUILTINS}")
    set_target_properties(${target} PROPERTIES SUFFIX ".elf"
        LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/link.ld")
endfunction()
add_executable(recovery
    entry.S main.c "${CORE}/flash-mapping.S"
    "${HPCLANG_ROOT}/runtime/bootstrap/sync.S")
foreach(module IN ITEMS rpl-flash-record rpl-flash-write rpl-flash-replay
        rpl-backup rpl-port rpl-object-span time time-math rtc rtc-decode datetime sst-flash)
    target_sources(recovery PRIVATE "${CORE}/${module}.c")
endforeach()
configure_recovery_image(recovery)
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

if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "")
    add_executable(admission admission-entry.S admission.c predicate.c "${CORE}/flash-mapping.S"
        "${HPCLANG_ROOT}/runtime/bootstrap/sync.S")
    foreach(module IN ITEMS rpl-stock-memory rpl-mutation rpl-stack-view rpl-flash-record
            rpl-flash-replay rpl-port rpl-backup rpl-object-span rpl-flash-write
            rpl-recovery-installation rpl-recovery-identity rpl-recovery-reservation rpl-object-store)
        target_sources(admission PRIVATE "${CORE}/${module}.c")
    endforeach()
    configure_recovery_image(admission)
    target_compile_definitions(admission PRIVATE
        HPCLANG_PREDICATE_STACK_BYTES=4096
        HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT=${HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT})
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/admission.hp"
            "${CMAKE_CURRENT_BINARY_DIR}/admission.bin"
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-recovery.py"
            --elf --model hp50g --binary-output "${CMAKE_CURRENT_BINARY_DIR}/admission.bin"
            --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
            "$<TARGET_FILE:admission>" "${CMAKE_CURRENT_BINARY_DIR}/admission.hp"
        DEPENDS admission "${HPCLANG_ROOT}/tools/package-recovery.py"
        VERBATIM)
    add_custom_target(admission-package ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/admission.hp"
        "${CMAKE_CURRENT_BINARY_DIR}/admission.bin")
endif()

# Generate at configuration time so Clang analysis can read the fixture even
# when this optional execution target has not been built.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/installation.py"
    "${CMAKE_CURRENT_LIST_DIR}/reservation.py"
    "${CMAKE_CURRENT_LIST_DIR}/verify.py"
    "${HPCLANG_ROOT}/tools/package-recovery.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/installation.py"
    "${CMAKE_CURRENT_BINARY_DIR}" COMMAND_ERROR_IS_FATAL ANY)
set(installation_witnesses installation installation-transaction installation-publication
        installation-cancellation installation-identity installation-reservation-failure
        installation-store installation-cleanup installation-staging installation-abort
        installation-activation-reject installation-creation installation-creation-admission)
if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "")
    math(EXPR TEMPLATE_AUTHORITY "${HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT} + 4124")
    foreach(template IN ITEMS template template-creation)
        set(creation_arguments)
        if(template STREQUAL "template-creation")
            list(APPEND creation_arguments --creation-claim-slot 0x1e0105)
        endif()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${template}.hp"
                "${CMAKE_CURRENT_BINARY_DIR}/${template}-layout.json"
                "${CMAKE_CURRENT_BINARY_DIR}/${template}-data.c"
            COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-recovery.py"
                --elf --model hp50g --flash-copy --library-id 1701
                --journal-slot "${HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT}"
                --authority-slot "${TEMPLATE_AUTHORITY}"
                --reservation-name HPCRES --reservation-bytes 32768 --reservation-prefix HPCMEM2
                --admission-elf "$<TARGET_FILE:admission>"
                ${creation_arguments}
                --layout-output "${CMAKE_CURRENT_BINARY_DIR}/${template}-layout.json"
                --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
                "$<TARGET_FILE:recovery>" "${CMAKE_CURRENT_BINARY_DIR}/${template}.hp"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/template.py"
                "${CMAKE_CURRENT_BINARY_DIR}/${template}.hp"
                "${CMAKE_CURRENT_BINARY_DIR}/${template}-layout.json"
                "${CMAKE_CURRENT_BINARY_DIR}/${template}-data.c"
            DEPENDS recovery admission "${HPCLANG_ROOT}/tools/package-recovery.py"
                "${CMAKE_CURRENT_LIST_DIR}/template.py"
                "${CMAKE_CURRENT_LIST_DIR}/reservation.py" "${CMAKE_CURRENT_LIST_DIR}/verify.py"
            VERBATIM)
        add_custom_target(recovery-${template}-data
            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${template}-data.c")
    endforeach()
    list(APPEND installation_witnesses installation-template installation-template-store
        installation-template-creation installation-template-creation-store)
endif()
foreach(witness IN LISTS installation_witnesses)
    add_executable(${witness} EXCLUDE_FROM_ALL admission-entry.S installation.c predicate.c memory.c
        "${CORE}/flash-mapping.S" "${HPCLANG_ROOT}/runtime/bootstrap/sync.S")
    foreach(module IN ITEMS rpl-stock-memory rpl-mutation rpl-stack-view rpl-flash-record
            rpl-flash-write rpl-port rpl-backup rpl-object-span)
        target_sources(${witness} PRIVATE "${CORE}/${module}.c")
    endforeach()
    if(witness MATCHES "^installation-template")
        set(template template)
        if(witness MATCHES "^installation-template-creation")
            set(template template-creation)
            target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_TEMPLATE_CREATION)
            target_sources(${witness} PRIVATE "${CORE}/rpl-recovery-reservation.c")
        endif()
        add_dependencies(${witness} recovery-${template}-data)
        target_sources(${witness} PRIVATE template.c "${CMAKE_CURRENT_BINARY_DIR}/${template}-data.c"
            "${CORE}/rpl-object-store.c")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_TEMPLATE)
    endif()
    if(witness STREQUAL "installation-store" OR witness STREQUAL "installation-cleanup" OR
            witness MATCHES "^installation-template.*-store$")
        target_sources(${witness} PRIVATE store.c predicate-tls.S)
        foreach(module IN ITEMS time time-math rtc rtc-decode datetime sst-flash)
            target_sources(${witness} PRIVATE "${CORE}/${module}.c")
        endforeach()
    endif()
    if(witness STREQUAL "installation" OR witness STREQUAL "installation-store" OR
            witness MATCHES "^installation-template")
        target_sources(${witness} PRIVATE startup.c "${CORE}/rpl-recovery-installation.c"
            "${CORE}/rpl-recovery-identity.c")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_STARTUP)
        if(witness STREQUAL "installation-store" OR witness MATCHES "^installation-template.*-store$")
            target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_STORE)
        endif()
    elseif(witness STREQUAL "installation-cleanup")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_CLEANUP)
    elseif(witness STREQUAL "installation-creation-admission")
        target_sources(${witness} PRIVATE "${CORE}/rpl-recovery-installation.c"
            "${CORE}/rpl-recovery-identity.c" "${CORE}/rpl-recovery-reservation.c")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_CREATION_ADMISSION)
    elseif(witness STREQUAL "installation-identity")
        target_sources(${witness} PRIVATE "${CORE}/rpl-recovery-identity.c"
            "${CORE}/rpl-recovery-installation.c")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_IDENTITY)
    elseif(witness STREQUAL "installation-transaction")
        target_sources(${witness} PRIVATE "${CORE}/rpl-flash-replay.c")
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_TRANSACTION)
    elseif(witness STREQUAL "installation-publication" OR
            witness STREQUAL "installation-reservation-failure" OR
            witness STREQUAL "installation-staging" OR witness STREQUAL "installation-abort" OR
            witness STREQUAL "installation-activation-reject" OR
            witness STREQUAL "installation-creation")
        target_sources(${witness} PRIVATE publication.c predicate-tls.S)
        foreach(module IN ITEMS rpl-flash-replay rpl-object-store object-snapshot rpl-recovery-reservation
                time time-math rtc rtc-decode datetime sst-flash)
            target_sources(${witness} PRIVATE "${CORE}/${module}.c")
        endforeach()
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_PUBLICATION)
        if(witness STREQUAL "installation-creation")
            target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_CREATION)
        elseif(witness STREQUAL "installation-activation-reject")
            target_compile_definitions(${witness} PRIVATE HPCLANG_ACTIVATION_REJECT)
        elseif(witness STREQUAL "installation-reservation-failure")
            target_compile_definitions(${witness} PRIVATE HPCLANG_RESERVATION_FAIL_WRITE=1)
        elseif(witness STREQUAL "installation-staging")
            target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_STAGING)
        elseif(witness STREQUAL "installation-abort")
            target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_ABORT)
        endif()
    elseif(witness STREQUAL "installation-cancellation")
        target_sources(${witness} PRIVATE cancellation.c predicate-tls.S)
        foreach(module IN ITEMS rpl-flash-replay time time-math rtc rtc-decode datetime sst-flash)
            target_sources(${witness} PRIVATE "${CORE}/${module}.c")
        endforeach()
        target_compile_definitions(${witness} PRIVATE HPCLANG_INSTALLATION_CANCELLATION)
    endif()
    configure_recovery_image(${witness})
    target_include_directories(${witness} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${witness} PRIVATE HPCLANG_PREDICATE_STACK_BYTES=4096)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${witness}.hp"
            "${CMAKE_CURRENT_BINARY_DIR}/${witness}.bin"
        COMMAND "${Python3_EXECUTABLE}" "${HPCLANG_ROOT}/tools/package-recovery.py"
            --elf --model hp50g --binary-output "${CMAKE_CURRENT_BINARY_DIR}/${witness}.bin"
            --nm "${LLVM_NM}" --readelf "${LLVM_READELF}" --objcopy "${LLVM_OBJCOPY}"
            "$<TARGET_FILE:${witness}>" "${CMAKE_CURRENT_BINARY_DIR}/${witness}.hp"
        DEPENDS ${witness} "${HPCLANG_ROOT}/tools/package-recovery.py"
        VERBATIM)
    if(NOT HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT STREQUAL "")
        add_custom_target(${witness}-package ALL
            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${witness}.hp"
                "${CMAKE_CURRENT_BINARY_DIR}/${witness}.bin")
    endif()
endforeach()
