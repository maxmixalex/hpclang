cmake_minimum_required(VERSION 3.20)
project(hpclang_unit_tests LANGUAGES C)
enable_testing()
get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(hpclang_unit name)
    add_executable(test_${name} ${ARGN})
    target_compile_features(test_${name} PRIVATE c_std_11)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

hpclang_unit(softfloat test_softfloat.c "${HPCLANG_ROOT}/runtime/core/armv4t-softfloat.c")
target_link_libraries(test_softfloat PRIVATE m)

hpclang_unit(time test_time.c "${HPCLANG_ROOT}/runtime/core/time-math.c")
target_include_directories(test_time PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(keyboard_events test_keyboard_events.c "${HPCLANG_ROOT}/runtime/core/keyboard-event-engine.c")
target_include_directories(test_keyboard_events PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(keyboard_keys test_keyboard_keys.c "${HPCLANG_ROOT}/runtime/core/keyboard-key-map.c")
target_include_directories(test_keyboard_keys PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(display test_display.c "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_display PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(font
    test_font.c
    "${HPCLANG_ROOT}/runtime/core/font.c"
    "${HPCLANG_ROOT}/runtime/core/saturn-view.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_font PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

# Configure-only test of the SDK font build API. It verifies that STATIC_AUTO,
# STATIC_EXPLICIT and FULL remain semantically distinct without assembling the
# ARM font sources on the host.
add_test(
    NAME font_cmake_modes
    COMMAND "${CMAKE_COMMAND}"
        "-DHPCLANG_ROOT=${HPCLANG_ROOT}"
        -S "${HPCLANG_ROOT}/tests/font-cmake"
        -B "${CMAKE_CURRENT_BINARY_DIR}/font-cmake")

hpclang_unit(rpl_object_span
    test_rpl_object_span.c
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_object_span PRIVATE
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_stack_view
    test_rpl_stack_view.c
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c")
target_include_directories(test_rpl_stack_view PRIVATE
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_object_store
    test_rpl_object_store.c
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c")
target_include_directories(test_rpl_object_store PRIVATE
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_transfer_directory
    test_hp39_transfer_directory.c
    "${HPCLANG_ROOT}/runtime/core/hp39-transfer-directory.c")
target_include_directories(test_hp39_transfer_directory PRIVATE
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_transfer_container
    test_hp39_transfer_container.c
    "${HPCLANG_ROOT}/runtime/core/hp39-transfer-container.c")
target_include_directories(test_hp39_transfer_container PRIVATE
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_catalog
    test_hp39_catalog.c
    "${HPCLANG_ROOT}/runtime/core/hp39-catalog.c")
target_include_directories(test_hp39_catalog PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_variable_schema
    test_hp39_variable_schema.c
    "${HPCLANG_ROOT}/runtime/core/hp39-variable-schema.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_hp39_variable_schema PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_native_directory
    test_hp39_native_directory.c
    "${HPCLANG_ROOT}/runtime/core/hp39-native-directory.c"
    "${HPCLANG_ROOT}/runtime/core/hp39-catalog.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_hp39_native_directory PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hp39_function_fields
    test_hp39_function_fields.c
    "${HPCLANG_ROOT}/runtime/core/hp39-function-fields.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_hp39_function_fields PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_snapshot
    test_object_snapshot.c
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_snapshot PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_construct
    test_object_construct.c
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_construct PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_store
    test_object_store.c
    "${HPCLANG_ROOT}/runtime/core/object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_store PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hpgcc_satdir
    test_hpgcc_satdir.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-satdir.c")
target_include_directories(test_hpgcc_satdir PRIVATE
    "${HPCLANG_ROOT}/include")

hpclang_unit(hpgcc_saturn
    test_hpgcc_saturn.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-saturn.c"
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_hpgcc_saturn PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object
    test_object.c
    "${HPCLANG_ROOT}/runtime/core/object.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c")
target_include_directories(test_object PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_mutation
    test_rpl_mutation.c
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_mutation PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(mmu test_mmu.c "${HPCLANG_ROOT}/runtime/bootstrap/mmu.c")
target_include_directories(test_mmu PRIVATE "${HPCLANG_ROOT}/include")

# Build the same pinned FatFs core on the host as a fast configuration/vendor
# check. The calculator runtime also links this core above the hp4950 SDI
# block-device backend.
add_library(hpclang_fatfs_vendor STATIC
    "${HPCLANG_ROOT}/third_party/fatfs/ff.c"
    "${HPCLANG_ROOT}/third_party/fatfs/ffunicode.c")
target_compile_features(hpclang_fatfs_vendor PRIVATE c_std_11)
target_include_directories(hpclang_fatfs_vendor PUBLIC
    "${HPCLANG_ROOT}/third_party/fatfs")

hpclang_unit(fatfs_config test_fatfs_config.c)
target_include_directories(test_fatfs_config PRIVATE
    "${HPCLANG_ROOT}/third_party/fatfs")
target_link_libraries(test_fatfs_config PRIVATE hpclang_fatfs_vendor)

hpclang_unit(namespace_lease
    test_namespace_lease.c
    "${HPCLANG_ROOT}/runtime/libc/storage-namespace-lease.c")
target_include_directories(test_namespace_lease PRIVATE
    "${HPCLANG_ROOT}/runtime/libc")

hpclang_unit(s3c2410_sdio_core
    test_s3c2410_sdio_core.c
    "${HPCLANG_ROOT}/runtime/libc/s3c2410-sdio-core.c")
target_include_directories(test_s3c2410_sdio_core PRIVATE
    "${HPCLANG_ROOT}/runtime/libc")

hpclang_unit(s3c2410_sdio_card
    test_s3c2410_sdio_card.c
    "${HPCLANG_ROOT}/runtime/libc/s3c2410-sdio-card.c"
    "${HPCLANG_ROOT}/runtime/libc/s3c2410-sdio-core.c")
target_include_directories(test_s3c2410_sdio_card PRIVATE
    "${HPCLANG_ROOT}/runtime/libc")

hpclang_unit(usbdev_host
    test_usbdev_host.c
    "${HPCLANG_ROOT}/tests/e2e/harness/x50ng/hpclang-usbdev-host.c")
target_include_directories(test_usbdev_host PRIVATE
    "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
