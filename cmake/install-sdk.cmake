# Preserve relative source/font paths so the installed SDK can be relocated.
set(sdk_root "share/hpclang")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/runtime/fonts/fonts.zip" font_source_header LIMIT 128)
if(font_source_header MATCHES "^version https://git-lfs.github.com/spec/")
    message(FATAL_ERROR "SDK installation requires the font source/license bundle: run git lfs pull")
endif()
install(DIRECTORY include runtime platform DESTINATION "${sdk_root}")
install(DIRECTORY third_party/fatfs DESTINATION "${sdk_root}/third_party")
install(DIRECTORY third_party/tinf DESTINATION "${sdk_root}/third_party")
install(DIRECTORY cmake/profiles cmake/toolchains DESTINATION "${sdk_root}/cmake")
install(FILES cmake/fonts.cmake cmake/ui-text.cmake cmake/hpclangConfig.cmake cmake/launchers.cmake
    DESTINATION "${sdk_root}/cmake")
install(FILES tools/hpclang-pack.py tools/build-launchers.py tools/armtoolbox-wrap.py
    tools/package-aplet.py tools/prepare-hp39-aplet.py tools/check-migration.py
    DESTINATION "${sdk_root}/tools")
# The experimental Notebook builder retains the layout used to find the SDK bootstrap.
install(FILES tests/e2e/independent-entry/build.py tests/e2e/independent-entry/build-hp39-aplet.py
    tests/e2e/independent-entry/entry.S tests/e2e/independent-entry/link.ld
    tests/e2e/independent-entry/probe.c DESTINATION "${sdk_root}/tools/experimental/independent-entry")
install(FILES tests/e2e/independent-pool/hp39-bitmap-probe.c
    DESTINATION "${sdk_root}/tools/experimental/independent-pool")
install(FILES LICENSE DESTINATION "${sdk_root}")
install(DIRECTORY examples DESTINATION "${sdk_root}")
install(FILES docs/sdk/getting-started.md DESTINATION "${sdk_root}/docs")
install(FILES docs/sdk/declarative-ui.md DESTINATION "${sdk_root}/docs")
install(FILES docs/sdk/migration.md DESTINATION "${sdk_root}/docs")
install(FILES docs/sdk/api-conventions.md DESTINATION "${sdk_root}/docs")
install(FILES docs/sdk/memory.md DESTINATION "${sdk_root}/docs")
install(FILES docs/development/licensing.md DESTINATION "${sdk_root}/docs")
