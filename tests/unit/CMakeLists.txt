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

hpclang_unit(mmu test_mmu.c "${HPCLANG_ROOT}/runtime/bootstrap/mmu.c")
target_include_directories(test_mmu PRIVATE "${HPCLANG_ROOT}/include")
