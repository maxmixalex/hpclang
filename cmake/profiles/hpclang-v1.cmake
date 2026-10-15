# Unified hpclang v1 execution profile.
#
# The application ABI is independent of the calculator family:
# ARMv4T, AAPCS32, Thumb-1 C code, soft-float and fixed virtual addresses.

set(HPCLANG_BUILD_MODEL "universal" CACHE STRING "Calculator model to optimize for, or universal")
set(hpclang_build_models universal hp39gplus hp39gs hp40gs hp48gii hp49gplus hp50g)
set_property(CACHE HPCLANG_BUILD_MODEL PROPERTY STRINGS ${hpclang_build_models})
list(FIND hpclang_build_models "${HPCLANG_BUILD_MODEL}" hpclang_build_model_id)
if(hpclang_build_model_id LESS 0)
    message(FATAL_ERROR "Unsupported HPCLANG_BUILD_MODEL: ${HPCLANG_BUILD_MODEL}")
endif()
if(hpclang_build_model_id GREATER 0)
    # Whole-program optimization removes services absent on the selected model.
    # Runtime startup also checks the model before invoking application code.
    add_compile_definitions(HPCLANG_COMPILED_MODEL=${hpclang_build_model_id})
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-flto>")
    add_link_options(-flto)
endif()

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:-mthumb>"
    "$<$<COMPILE_LANGUAGE:C>:-mabi=aapcs>"
    "$<$<COMPILE_LANGUAGE:C>:-mfloat-abi=soft>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-pic>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-pie>"
)

set(HPCLANG_RUNTIME_START
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/start.S")
set(HPCLANG_RUNTIME
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-layout.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-focus.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-input.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-paint.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-effect.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/ui-surface.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/runtime.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/exit-hooks.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/device.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/power.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/power-clock.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/power-profile.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display-session.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display-hardware.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/dma-buffer.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/dma-span.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/lcd-engine.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/bootstrap/sync.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/contrast.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/display-surface.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/font.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/saturn-view.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-object-span.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-reference.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-backup.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-flash-record.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-flash-write.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-flash-replay.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-port.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/sst-flash.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/stock-flash.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/flash-mapping.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/port.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/port-write.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-port-mutation.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-stack-view.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-snapshot.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-construct.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-composite.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-decimal.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-object-store.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hp39-variable-schema.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hp39-native-directory.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hp39-portal.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hp39-aplet-frame.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-store.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/object-catalog.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpgcc-satdir.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpg-context.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpg-draw.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpg-font.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpg-xpm.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/fonts/generated/newrpl_font6_menus.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/fonts/generated/newrpl_font8_styleb.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpgcc-saturn.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-mutation.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rpl-stock-memory.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpgcc-keyboard.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/hpgcc-system.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input-session.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input-alpha.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input-keymap.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input-keyboard.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/input-mode.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/console.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/stdio-routing.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard-event-engine.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard-key-map.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/keyboard-events.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/datetime.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rtc-decode.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/rtc.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/timer-engine.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/timer.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/sound-engine.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/sound.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/time-math.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/time.c")
set(HPCLANG_RUNTIME_EABI_SHIMS
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-eabi-div.S"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-softfloat-entry.S")
set(HPCLANG_RUNTIME_SOFTFLOAT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/armv4t-softfloat.c")
set(HPCLANG_LIBC
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/sbrk.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/platform.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/stdio-lifetime.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage-media.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage-volumes.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage-media-events.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/hpgcc-fsystem.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage-namespace.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/storage-namespace-lease.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/s3c2410-sdio-core.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/s3c2410-sdio-card.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/hp4950-fatfs-diskio.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../third_party/fatfs/ff.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../third_party/fatfs/ffunicode.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/libc/kos-call.S")

# These implementation objects deliberately stay in ARM state. Application C/C++
# remains Thumb-1; the EABI veneers provide the one explicit interworking edge.
set_source_files_properties(
    ${HPCLANG_RUNTIME_EABI_SHIMS}
    ${HPCLANG_RUNTIME_SOFTFLOAT}
    PROPERTIES COMPILE_OPTIONS "-marm")
set(HPCLANG_LINKER_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/core/link.ld")
set(HPCLANG_PACKER
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/hpclang-pack.py")

set(HPCLANG_ABI "hpc1" CACHE STRING "hpclang executable ABI")
add_link_options(
    "LINKER:--wrap=fdopen,--wrap=freopen,--wrap=fclose,--wrap=fflush"
    "LINKER:--wrap=fmemopen,--wrap=funopen,--wrap=fdevopen")
set(HPCLANG_CODE_MODEL "fixed-virtual" CACHE STRING "hpclang code model")
