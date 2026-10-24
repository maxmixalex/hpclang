cmake_minimum_required(VERSION 3.20)
project(hpclang_unit_tests LANGUAGES C CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME python_tools
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_LIST_DIR}" -p "test_*.py")
get_filename_component(HPCLANG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(hpclang_unit name)
    add_executable(test_${name} ${ARGN})
    target_compile_features(test_${name} PRIVATE c_std_11)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

hpclang_unit(flash_busy test_flash_busy.c)
target_include_directories(test_flash_busy PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
hpclang_unit(watchdog_clock test_watchdog_clock.c)
target_include_directories(test_watchdog_clock PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
hpclang_unit(watchdog_model test_watchdog_model.c)
target_include_directories(test_watchdog_model PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(memory_checkpoint test_memory_checkpoint.c)
target_include_directories(test_memory_checkpoint PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

if(UNIX)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        hpclang_unit(time_state test_time_state.c
            "${HPCLANG_ROOT}/runtime/core/time.c"
            "${HPCLANG_ROOT}/runtime/core/time-math.c"
            "${HPCLANG_ROOT}/runtime/core/datetime.c")
        target_include_directories(test_time_state PRIVATE
            "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
    endif()
    hpclang_unit(pool_checkpoint test_pool_checkpoint.c)
    target_include_directories(test_pool_checkpoint PRIVATE
        "${HPCLANG_ROOT}/tests/e2e/harness/x50ng" "${HPCLANG_ROOT}/platform/armtoolbox")
    hpclang_unit(reset_checkpoint test_reset_checkpoint.c)
    target_include_directories(test_reset_checkpoint PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
endif()

hpclang_unit(libc_sbrk test_libc_sbrk.c)
target_include_directories(test_libc_sbrk PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(sst_flash test_sst_flash.c "${HPCLANG_ROOT}/runtime/core/sst-flash.c")
target_include_directories(test_sst_flash PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rtc_tick test_rtc_tick.c "${HPCLANG_ROOT}/runtime/core/rtc-tick.c")
target_include_directories(test_rtc_tick PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(win_key_state test_win_key_state.c
    "${HPCLANG_ROOT}/runtime/core/win-key-state.c")
target_include_directories(test_win_key_state PRIVATE "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/include")
hpclang_unit(win_key_state_reference "${HPCLANG_ROOT}/tests/compat/win-key-state.c"
    "${HPCLANG_ROOT}/runtime/core/win-key-state.c")
target_include_directories(test_win_key_state_reference PRIVATE "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/include")

hpclang_unit(win_focus test_win_focus.c "${HPCLANG_ROOT}/runtime/core/win-focus.c")
target_include_directories(test_win_focus PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(win_events test_win_events.c "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_events PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(win_timers test_win_timers.c
    "${HPCLANG_ROOT}/runtime/core/win-timers.c" "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_timers PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(win_loop test_win_loop.c
    "${HPCLANG_ROOT}/runtime/core/win-loop.c"
    "${HPCLANG_ROOT}/runtime/core/win-timers.c" "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_loop PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(win_repaint test_win_repaint.c
    "${HPCLANG_ROOT}/runtime/core/win-repaint.c"
    "${HPCLANG_ROOT}/runtime/core/win-loop.c" "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_repaint PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(win_public test_win_public.c
    "${HPCLANG_ROOT}/runtime/core/win-key-state.c"
    "${HPCLANG_ROOT}/runtime/core/hpgcc-window.c"
    "${HPCLANG_ROOT}/runtime/core/win-drivers.c"
    "${HPCLANG_ROOT}/runtime/core/win-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/win-repaint.c"
    "${HPCLANG_ROOT}/runtime/core/win-timers.c"
    "${HPCLANG_ROOT}/runtime/core/win-loop.c" "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_public PRIVATE "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/include")
get_target_property(win_adapter_sources test_win_public SOURCES)
list(REMOVE_ITEM win_adapter_sources test_win_public.c)
hpclang_unit(win_widget test_win_widget.c ${win_adapter_sources}
    "${HPCLANG_ROOT}/runtime/core/hpgcc-widget.c"
    "${HPCLANG_ROOT}/runtime/core/win-focus.c")
target_include_directories(test_win_widget PRIVATE "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/include")
hpclang_unit(win_keyboard test_win_keyboard.c
    "${HPCLANG_ROOT}/runtime/core/win-drivers.c"
    "${HPCLANG_ROOT}/runtime/core/win-loop.c"
    "${HPCLANG_ROOT}/runtime/core/win-timers.c"
    "${HPCLANG_ROOT}/runtime/core/win-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/win-events.c"
    "${HPCLANG_ROOT}/runtime/core/hpgcc-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/keyboard-key-map.c")
target_include_directories(test_win_keyboard PRIVATE "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/include")
hpclang_unit(win_timer_reference "${HPCLANG_ROOT}/tests/compat/win-timers.c"
    "${HPCLANG_ROOT}/runtime/core/win-timers.c" "${HPCLANG_ROOT}/runtime/core/win-events.c")
target_include_directories(test_win_timer_reference PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_record test_rpl_flash_record.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_record PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(rpl_flash_stream test_rpl_flash_stream.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_stream PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_intent test_rpl_flash_intent.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_intent PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_replay test_rpl_flash_replay.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-replay.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_replay PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_authority test_rpl_flash_authority.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-replay.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_authority PRIVATE "${HPCLANG_ROOT}/runtime/core")

# Static analysis consumes the compilation database without building targets.
# Keep generated inputs available at configuration time, including regeneration
# when either the reference generator or its packager changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/recovery_identity_vectors.py"
    "${HPCLANG_ROOT}/tools/package-recovery.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/recovery_identity_vectors.py"
        "${CMAKE_CURRENT_BINARY_DIR}/recovery-identity-vectors.h"
    COMMAND_ERROR_IS_FATAL ANY)
hpclang_unit(rpl_recovery_identity test_rpl_recovery_identity.c
    "${CMAKE_CURRENT_BINARY_DIR}/recovery-identity-vectors.h"
    "${HPCLANG_ROOT}/runtime/core/rpl-recovery-identity.c")
target_include_directories(test_rpl_recovery_identity PRIVATE
    "${HPCLANG_ROOT}/runtime/core" "${CMAKE_CURRENT_BINARY_DIR}")
hpclang_unit(rpl_recovery_installation test_rpl_recovery_installation.c
    "${CMAKE_CURRENT_BINARY_DIR}/recovery-identity-vectors.h"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-recovery-installation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-recovery-identity.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-replay.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_recovery_installation PRIVATE
    "${HPCLANG_ROOT}/runtime/core" "${CMAKE_CURRENT_BINARY_DIR}")

hpclang_unit(rpl_flash_write test_rpl_flash_write.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_write PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_create test_rpl_flash_create.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_create PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_flash_plan test_rpl_flash_plan.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_flash_plan PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(stage_stack test_stage_stack.c "${HPCLANG_ROOT}/platform/armtoolbox/stage-stack.c")
target_include_directories(test_stage_stack PRIVATE "${HPCLANG_ROOT}/platform/armtoolbox")

add_executable(stage_image_driver stage_image_driver.c
    "${HPCLANG_ROOT}/platform/armtoolbox/stage-image.c"
    "${HPCLANG_ROOT}/third_party/tinf/tinflate.c")
target_include_directories(stage_image_driver PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/platform/armtoolbox")
target_compile_options(stage_image_driver PRIVATE -Wall -Wextra -Werror)
add_test(NAME stage_image COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/stage_image_cases.py" $<TARGET_FILE:stage_image_driver>)

hpclang_unit(notebook_store test_notebook_store.c
    "${HPCLANG_ROOT}/examples/notebook/store.c"
    "${HPCLANG_ROOT}/examples/notebook/export.c")
target_include_directories(test_notebook_store PRIVATE "${HPCLANG_ROOT}/include")
target_compile_definitions(test_notebook_store PRIVATE
    open=notebook_test_open write=notebook_test_write close=notebook_test_close)

hpclang_unit(hpgcc_system test_hpgcc_system.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-system.c"
    "${HPCLANG_ROOT}/runtime/core/datetime.c")
target_include_directories(test_hpgcc_system PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(hpgcc_string test_hpgcc_string.c "${HPCLANG_ROOT}/runtime/core/hpgcc-string.c")
target_include_directories(test_hpgcc_string PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(hpgcc_number test_hpgcc_number.c "${HPCLANG_ROOT}/runtime/core/hpgcc-number.c")
target_include_directories(test_hpgcc_number PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(hpgcc_line test_hpgcc_line.c "${HPCLANG_ROOT}/runtime/core/hpgcc-line.c")
target_include_directories(test_hpgcc_line PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hpgcc_input test_hpgcc_input.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-input.c"
    "${HPCLANG_ROOT}/runtime/core/input-mode.c"
    "${HPCLANG_ROOT}/runtime/core/input-keymap.c"
    "${HPCLANG_ROOT}/runtime/core/input-alpha.c")
target_include_directories(test_hpgcc_input PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hpgcc_rtc test_hpgcc_rtc.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-rtc.c"
    "${HPCLANG_ROOT}/runtime/core/rtc-tick.c"
    "${HPCLANG_ROOT}/runtime/core/exit-hooks.c")
target_include_directories(test_hpgcc_rtc PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hpgcc_keyboard test_hpgcc_keyboard.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/keyboard-key-map.c")
target_include_directories(test_hpgcc_keyboard PRIVATE "${HPCLANG_ROOT}/include")
hpclang_unit(hpg_context test_hpg_context.c
    "${HPCLANG_ROOT}/runtime/core/hpgcc-widget.c"
    "${HPCLANG_ROOT}/runtime/core/win-focus.c"
    "${HPCLANG_ROOT}/runtime/core/hpgcc-window.c"
    "${HPCLANG_ROOT}/runtime/core/win-key-state.c"
    "${HPCLANG_ROOT}/runtime/core/win-drivers.c"
    "${HPCLANG_ROOT}/runtime/core/win-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/win-timers.c"
    "${HPCLANG_ROOT}/runtime/core/win-repaint.c"
    "${HPCLANG_ROOT}/runtime/core/win-loop.c"
    "${HPCLANG_ROOT}/runtime/core/win-events.c"
    "${HPCLANG_ROOT}/runtime/core/hpgcc-console.c"
    "${HPCLANG_ROOT}/runtime/core/hpgcc-line.c"
    "${HPCLANG_ROOT}/runtime/core/console.c"
    "${HPCLANG_ROOT}/runtime/core/stdio-routing.c"
    "${HPCLANG_ROOT}/runtime/core/hpg-context.c"
    "${HPCLANG_ROOT}/runtime/core/hpg-draw.c"
    "${HPCLANG_ROOT}/runtime/core/hpg-font.c"
    "${HPCLANG_ROOT}/runtime/core/hpg-xpm.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_hpg_context PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")
target_compile_definitions(test_hpg_context PRIVATE getchar=hpg_test_getchar getc=hpg_test_getc
    putchar=hpg_test_putchar putc=hpg_test_putc fputs=hpg_test_fputs ferror=hpg_test_ferror)
set(HPCLANG_UI_TEST_SOURCES "${HPCLANG_ROOT}/runtime/core/ui.c"
    "${HPCLANG_ROOT}/runtime/core/ui-layout.c"
    "${HPCLANG_ROOT}/runtime/core/ui-focus.c"
    "${HPCLANG_ROOT}/runtime/core/ui-paint.c"
    "${HPCLANG_ROOT}/runtime/core/ui-effect.c"
    "${HPCLANG_ROOT}/runtime/core/ui-input.c"
    "${HPCLANG_ROOT}/runtime/core/input.c"
    "${HPCLANG_ROOT}/runtime/core/input-keyboard.c"
    "${HPCLANG_ROOT}/runtime/core/input-mode.c"
    "${HPCLANG_ROOT}/runtime/core/input-alpha.c"
    "${HPCLANG_ROOT}/runtime/core/input-keymap.c")
hpclang_unit(ui test_ui.c ${HPCLANG_UI_TEST_SOURCES})
hpclang_unit(ui_effect_exit test_ui_effect_exit.c ${HPCLANG_UI_TEST_SOURCES})
hpclang_unit(ui_cpp test_ui_cpp.cpp ${HPCLANG_UI_TEST_SOURCES})
target_compile_features(test_ui_cpp PRIVATE cxx_std_17)
target_compile_options(test_ui_cpp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
target_include_directories(test_ui_cpp PRIVATE "${HPCLANG_ROOT}/include")
hpclang_unit(surface_cpp test_surface_cpp.cpp "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_compile_features(test_surface_cpp PRIVATE cxx_std_20)
target_compile_options(test_surface_cpp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
target_include_directories(test_surface_cpp PRIVATE "${HPCLANG_ROOT}/include")
foreach(name ui ui_effect_exit)
    target_include_directories(test_${name} PRIVATE "${HPCLANG_ROOT}/include")
endforeach()

set(HPCLANG_UI_SURFACE_TEST_SOURCES ${HPCLANG_UI_TEST_SOURCES}
    "${HPCLANG_ROOT}/runtime/core/ui-surface.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c"
    "${HPCLANG_ROOT}/runtime/core/font.c"
    "${HPCLANG_ROOT}/runtime/core/saturn-view.c")
hpclang_unit(ui_surface test_ui_surface.c ${HPCLANG_UI_SURFACE_TEST_SOURCES})
target_include_directories(test_ui_surface PRIVATE "${HPCLANG_ROOT}/include")
hpclang_unit(ui_demo test_ui_demo.c ${HPCLANG_UI_SURFACE_TEST_SOURCES}
    "${HPCLANG_ROOT}/examples/declarative-ui/app.c")
target_include_directories(test_ui_demo PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(notebook_ui test_notebook_ui.c ${HPCLANG_UI_SURFACE_TEST_SOURCES}
    "${HPCLANG_ROOT}/examples/notebook/app.c")
target_include_directories(test_notebook_ui PRIVATE "${HPCLANG_ROOT}/include")

if(UNIX)
    hpclang_unit(ui_surface_alloc test_ui_surface_alloc.c ${HPCLANG_UI_SURFACE_TEST_SOURCES})
    target_include_directories(test_ui_surface_alloc PRIVATE "${HPCLANG_ROOT}/include")
    target_link_options(test_ui_surface_alloc PRIVATE "LINKER:--wrap=malloc,--wrap=calloc,--wrap=free")
    hpclang_unit(stdio_lifetime test_stdio_lifetime.c
        "${HPCLANG_ROOT}/runtime/core/exit-hooks.c"
        "${HPCLANG_ROOT}/runtime/libc/stdio-lifetime.c")
    target_link_options(test_stdio_lifetime PRIVATE
        "LINKER:--wrap=fdopen,--wrap=freopen,--wrap=fclose,--wrap=fflush,--wrap=fmemopen,--wrap=malloc")
    hpclang_unit(directory_order test_directory_order.c)
    target_include_directories(test_directory_order PRIVATE
        "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
    hpclang_unit(execution_clock test_execution_clock.c
        "${HPCLANG_ROOT}/tests/e2e/harness/x50ng/execution-clock.c")
    target_include_directories(test_execution_clock PRIVATE
        "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
    hpclang_unit(input_checkpoint test_input_checkpoint.c)
    target_include_directories(test_input_checkpoint PRIVATE
        "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")
endif()

hpclang_unit(softfloat test_softfloat.c "${HPCLANG_ROOT}/runtime/core/armv4t-softfloat.c")
target_link_libraries(test_softfloat PRIVATE m)

hpclang_unit(font_symbols test_font_symbols.c
    "${HPCLANG_ROOT}/runtime/core/font.c" "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_font_symbols PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(hpg_graphics test_hpg_graphics.c
    "${HPCLANG_ROOT}/tests/e2e/display/hpg-prototype.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_hpg_graphics PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(input test_input.c "${HPCLANG_ROOT}/runtime/core/input.c")
target_include_directories(test_input PRIVATE "${HPCLANG_ROOT}/include")
hpclang_unit(input_session test_input_session.c "${HPCLANG_ROOT}/runtime/core/input.c"
    "${HPCLANG_ROOT}/runtime/core/input-session.c"
    "${HPCLANG_ROOT}/runtime/core/input-mode.c"
    "${HPCLANG_ROOT}/runtime/core/input-alpha.c"
    "${HPCLANG_ROOT}/runtime/core/input-keymap.c"
    "${HPCLANG_ROOT}/runtime/core/input-keyboard.c")
target_include_directories(test_input_session PRIVATE "${HPCLANG_ROOT}/include")
hpclang_unit(input_alpha test_input_alpha.c "${HPCLANG_ROOT}/runtime/core/input-alpha.c" "${HPCLANG_ROOT}/runtime/core/keyboard-key-map.c")
target_include_directories(test_input_alpha PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(input_mode test_input_mode.c "${HPCLANG_ROOT}/runtime/core/input-mode.c")
hpclang_unit(input_keyboard test_input_keyboard.c
    "${HPCLANG_ROOT}/runtime/core/input.c"
    "${HPCLANG_ROOT}/runtime/core/input-session.c"
    "${HPCLANG_ROOT}/runtime/core/input-mode.c"
    "${HPCLANG_ROOT}/runtime/core/input-alpha.c"
    "${HPCLANG_ROOT}/runtime/core/input-keymap.c"
    "${HPCLANG_ROOT}/runtime/core/input-keyboard.c")
target_include_directories(test_input_keyboard PRIVATE "${HPCLANG_ROOT}/include")
target_include_directories(test_input_mode PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(numeric_input test_numeric_input.c "${HPCLANG_ROOT}/runtime/core/input.c")
target_include_directories(test_numeric_input PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(console test_console.c "${HPCLANG_ROOT}/runtime/core/console.c"
    "${HPCLANG_ROOT}/runtime/core/font.c" "${HPCLANG_ROOT}/runtime/core/saturn-view.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_console PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(stdio_routing test_stdio_routing.c "${HPCLANG_ROOT}/runtime/core/stdio-routing.c")
target_include_directories(test_stdio_routing PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(device test_device.c "${HPCLANG_ROOT}/runtime/core/device.c"
    "${HPCLANG_ROOT}/runtime/core/contrast.c" "${HPCLANG_ROOT}/runtime/core/power.c")
target_include_directories(test_device PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(display_session test_display_session.c
    "${HPCLANG_ROOT}/runtime/core/display-session.c"
    "${HPCLANG_ROOT}/runtime/core/display-surface.c"
    "${HPCLANG_ROOT}/runtime/core/lcd-engine.c"
    "${HPCLANG_ROOT}/runtime/core/dma-buffer.c")
target_include_directories(test_display_session PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(datetime test_datetime.c
    "${HPCLANG_ROOT}/runtime/core/datetime.c"
    "${HPCLANG_ROOT}/runtime/core/rtc-decode.c")
target_include_directories(test_datetime PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rtc_clock test_rtc_clock.c
    "${HPCLANG_ROOT}/tests/e2e/harness/x50ng/rtc-clock.c"
    "${HPCLANG_ROOT}/runtime/core/datetime.c"
    "${HPCLANG_ROOT}/runtime/core/rtc-decode.c")
target_include_directories(test_rtc_clock PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core" "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(timer test_timer.c
    "${HPCLANG_ROOT}/runtime/core/timer-engine.c"
    "${HPCLANG_ROOT}/runtime/core/timer.c")
target_include_directories(test_timer PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(sound_api test_sound_api.c "${HPCLANG_ROOT}/runtime/core/sound.c")
target_include_directories(test_sound_api PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(sound test_sound.c "${HPCLANG_ROOT}/runtime/core/sound-engine.c")
target_include_directories(test_sound PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(sound_alarm test_sound_alarm.c "${HPCLANG_ROOT}/runtime/core/sound-alarm.c")
target_include_directories(test_sound_alarm PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(watchdog_alarm test_watchdog_alarm.c "${HPCLANG_ROOT}/runtime/core/watchdog-alarm.c")
target_include_directories(test_watchdog_alarm PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(irq_owner test_irq_owner.c "${HPCLANG_ROOT}/runtime/core/irq-owner.c")
target_include_directories(test_irq_owner PRIVATE "${HPCLANG_ROOT}/runtime/core")
hpclang_unit(sound_session test_sound_session.c
    "${HPCLANG_ROOT}/runtime/core/sound-session.c"
    "${HPCLANG_ROOT}/runtime/core/sound-engine.c"
    "${HPCLANG_ROOT}/runtime/core/watchdog-alarm.c"
    "${HPCLANG_ROOT}/runtime/core/irq-owner.c")
target_include_directories(test_sound_session PRIVATE "${HPCLANG_ROOT}/runtime/core")
  hpclang_unit(hpgcc_sound test_hpgcc_sound.c "${HPCLANG_ROOT}/runtime/core/hpgcc-sound.c")
  target_include_directories(test_hpgcc_sound PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    hpclang_unit(sound_async test_sound_async.c "${HPCLANG_ROOT}/runtime/core/sound-async.c")
    target_include_directories(test_sound_async PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
endif()
hpclang_unit(sound_playback test_sound_playback.c
    "${HPCLANG_ROOT}/runtime/core/sound-playback.c"
    "${HPCLANG_ROOT}/runtime/core/sound-engine.c"
    "${HPCLANG_ROOT}/runtime/core/sound-alarm.c")
target_include_directories(test_sound_playback PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(time test_time.c "${HPCLANG_ROOT}/runtime/core/time-math.c")
target_include_directories(test_time PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(keyboard_events test_keyboard_events.c "${HPCLANG_ROOT}/runtime/core/keyboard-event-engine.c")
target_include_directories(test_keyboard_events PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(keyboard_keys test_keyboard_keys.c "${HPCLANG_ROOT}/runtime/core/keyboard-key-map.c")
target_include_directories(test_keyboard_keys PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(display test_display.c "${HPCLANG_ROOT}/runtime/core/display-surface.c")
target_include_directories(test_display PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(dma_span test_dma_span.c "${HPCLANG_ROOT}/runtime/core/dma-span.c")
target_include_directories(test_dma_span PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(lcd_engine test_lcd_engine.c "${HPCLANG_ROOT}/runtime/core/lcd-engine.c")
target_include_directories(test_lcd_engine PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(dma_buffer test_dma_buffer.c "${HPCLANG_ROOT}/runtime/core/dma-buffer.c"
    "${HPCLANG_ROOT}/runtime/core/dma-span.c")
target_include_directories(test_dma_buffer PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

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

hpclang_unit(rpl_backup test_rpl_backup.c
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_backup PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_port test_rpl_port.c
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_port PRIVATE "${HPCLANG_ROOT}/runtime/core")

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

hpclang_unit(rpl_code test_rpl_code.c "${HPCLANG_ROOT}/runtime/core/rpl-code.c")
target_include_directories(test_rpl_code PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_reference test_rpl_reference.c
    "${HPCLANG_ROOT}/runtime/core/rpl-code.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-reference.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_reference PRIVATE "${HPCLANG_ROOT}/runtime/core")

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

hpclang_unit(object_composite
    test_object_composite.c
    "${HPCLANG_ROOT}/runtime/core/object-decimal.c"
    "${HPCLANG_ROOT}/runtime/core/object-composite.c"
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_composite PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_construct
    test_object_construct.c
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")

hpclang_unit(object_cpp
    test_object_cpp.cpp
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_compile_features(test_object_cpp PRIVATE cxx_std_17)
target_compile_options(test_object_cpp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
target_include_directories(test_object_cpp PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_catalog
    test_object_catalog.c
    "${HPCLANG_ROOT}/runtime/core/object-catalog.c"
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_catalog PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")
target_include_directories(test_object_construct PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(object_store
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    test_object_store.c
    test_object_store_cpp.cpp
    rpl-stock-memory-stub.c
    "${HPCLANG_ROOT}/runtime/core/object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_object_store PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")
target_compile_features(test_object_store PRIVATE cxx_std_17)
target_compile_options(test_object_store PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")

hpclang_unit(port
    test_port.c
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/object-construct.c"
    "${HPCLANG_ROOT}/runtime/core/port-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_port PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

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
    rpl-stock-memory-stub.c
    "${HPCLANG_ROOT}/runtime/core/object.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c")
target_include_directories(test_object PRIVATE
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(rpl_mutation
    test_rpl_mutation.c
    "${HPCLANG_ROOT}/runtime/core/rpl-recovery-reservation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-write.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-flash-record.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-recovery-identity.c"
    "${HPCLANG_ROOT}/platform/armtoolbox/stage-stack.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-port.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-backup.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-mutation.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-store.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-stack-view.c"
    "${HPCLANG_ROOT}/runtime/core/object-snapshot.c"
    "${HPCLANG_ROOT}/runtime/core/rpl-object-span.c")
target_include_directories(test_rpl_mutation PRIVATE
    "${HPCLANG_ROOT}/platform/armtoolbox"
    "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core")
if(UNIX)
    target_compile_definitions(test_rpl_mutation PRIVATE HPCLANG_TEST_WRAP_ALLOC=1)
    target_link_options(test_rpl_mutation PRIVATE "LINKER:--wrap=malloc,--wrap=free")
endif()

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

hpclang_unit(fatfs_config test_fatfs_config.c
    "${HPCLANG_ROOT}/runtime/libc/storage-volumes.c")
target_include_directories(test_fatfs_config PRIVATE
    "${HPCLANG_ROOT}/third_party/fatfs")
target_link_libraries(test_fatfs_config PRIVATE hpclang_fatfs_vendor)

hpclang_unit(storage_metadata test_storage_metadata.c
    test_storage_cpp.cpp
    "${HPCLANG_ROOT}/runtime/libc/storage-volumes.c"
    "${HPCLANG_ROOT}/runtime/libc/storage-namespace.c"
    "${HPCLANG_ROOT}/runtime/libc/storage-namespace-lease.c")
target_include_directories(test_storage_metadata PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/libc")
target_link_libraries(test_storage_metadata PRIVATE hpclang_fatfs_vendor)
target_compile_features(test_storage_metadata PRIVATE cxx_std_17)
target_compile_options(test_storage_metadata PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
if(UNIX)
    hpclang_unit(stream_cpp test_stream_cpp.cpp)
    target_include_directories(test_stream_cpp PRIVATE "${HPCLANG_ROOT}/include")
    target_compile_features(test_stream_cpp PRIVATE cxx_std_17)
    target_compile_options(test_stream_cpp PRIVATE -fno-exceptions -fno-rtti)
endif()

hpclang_unit(storage_media test_storage_media.c "${HPCLANG_ROOT}/runtime/libc/storage-media.c")
target_include_directories(test_storage_media PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/libc")

hpclang_unit(storage_media_events test_storage_media_events.c
    "${HPCLANG_ROOT}/runtime/libc/storage-media-events.c")
target_include_directories(test_storage_media_events PRIVATE
    "${HPCLANG_ROOT}/runtime/libc" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(storage_diskio test_storage_diskio.c "${HPCLANG_ROOT}/runtime/libc/hp4950-fatfs-diskio.c")
target_include_directories(test_storage_diskio PRIVATE
    "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/libc" "${HPCLANG_ROOT}/runtime/core"
    "${HPCLANG_ROOT}/third_party/fatfs")

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

hpclang_unit(e2e_events test_e2e_events.c)
target_include_directories(test_e2e_events PRIVATE
    "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(e2e_phase test_e2e_phase.c "${HPCLANG_ROOT}/tests/e2e/phase.c")
target_include_directories(test_e2e_phase PRIVATE "${HPCLANG_ROOT}/include")

hpclang_unit(rpl_stock_memory test_rpl_stock_memory.c
    "${HPCLANG_ROOT}/runtime/core/rpl-stock-memory.c")
target_include_directories(test_rpl_stock_memory PRIVATE "${HPCLANG_ROOT}/include" "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(port_memory test_port_memory.c)
target_include_directories(test_port_memory PRIVATE "${HPCLANG_ROOT}/include"
    "${HPCLANG_ROOT}/runtime/core" "${HPCLANG_ROOT}/platform/armtoolbox")

hpclang_unit(hp39_portal_stack test_hp39_portal_stack.c)
target_include_directories(test_hp39_portal_stack PRIVATE "${HPCLANG_ROOT}/platform/armtoolbox")

hpclang_unit(hp39_aplet_frame test_hp39_aplet_frame.c
    "${HPCLANG_ROOT}/runtime/core/hp39-aplet-frame.c")
target_include_directories(test_hp39_aplet_frame PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(key_sample test_key_sample.c)
target_include_directories(test_key_sample PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(lcd_stability test_lcd_stability.c)
target_include_directories(test_lcd_stability PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(key_delivery test_key_delivery.c)
target_include_directories(test_key_delivery PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(view_tap test_view_tap.c)
target_include_directories(test_view_tap PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

hpclang_unit(power_clock test_power_clock.c "${HPCLANG_ROOT}/runtime/core/power-clock.c")
target_include_directories(test_power_clock PRIVATE "${HPCLANG_ROOT}/runtime/core")

hpclang_unit(timer_clock test_timer_clock.c)
target_include_directories(test_timer_clock PRIVATE "${HPCLANG_ROOT}/tests/e2e/harness/x50ng")

# Rename only the application/libc boundary, keeping the real runtime control
# transfer and cleanup logic while the unit executable uses the host libc.
add_library(runtime_exit_fixture OBJECT "${HPCLANG_ROOT}/runtime/core/runtime.c")
target_include_directories(runtime_exit_fixture PRIVATE "${HPCLANG_ROOT}/include")
target_compile_options(runtime_exit_fixture PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(runtime_exit_fixture PRIVATE
    main=fixture_main exit=fixture_exit _exit=fixture_immediate_exit)
hpclang_unit(runtime_exit test_runtime_exit.c $<TARGET_OBJECTS:runtime_exit_fixture>)
target_include_directories(test_runtime_exit PRIVATE "${HPCLANG_ROOT}/include")

add_library(runtime_exit_model_fixture OBJECT "${HPCLANG_ROOT}/runtime/core/runtime.c")
target_include_directories(runtime_exit_model_fixture PRIVATE "${HPCLANG_ROOT}/include")
target_compile_options(runtime_exit_model_fixture PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(runtime_exit_model_fixture PRIVATE
    HPCLANG_COMPILED_MODEL=1 main=fixture_main exit=fixture_exit _exit=fixture_immediate_exit)
hpclang_unit(runtime_exit_model test_runtime_exit.c $<TARGET_OBJECTS:runtime_exit_model_fixture>)
target_include_directories(test_runtime_exit_model PRIVATE "${HPCLANG_ROOT}/include")
target_compile_definitions(test_runtime_exit_model PRIVATE HPCLANG_COMPILED_MODEL=1)
