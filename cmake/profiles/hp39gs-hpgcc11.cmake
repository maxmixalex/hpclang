# hp39gs-hpgcc11 calculator profile
#
# This profile intentionally reuses the proven HPGCC 1.1-p1 39G runtime
# objects rather than pretending stock Clang already reproduces the complete
# historical r10 single-PIC-base ABI.
#
# Expected normalized SDK layout:
#   $HPGCC39_HOME/lib/MMUloader.o
#   $HPGCC39_HOME/lib/MMUglobals.o
#   $HPGCC39_HOME/lib/MMUld.script
#   $HPGCC39_HOME/hp2aplet/...

if(NOT DEFINED ENV{HPGCC39_HOME})
    message(FATAL_ERROR "HPGCC39_HOME must point at a normalized HPGCC 1.1-p1 39G SDK")
endif()

set(HPGCC39_HOME "$ENV{HPGCC39_HOME}" CACHE PATH "Normalized HPGCC 39G SDK")
set(HPGCC39_MMU_LOADER "${HPGCC39_HOME}/lib/MMUloader.o")
set(HPGCC39_MMU_GLOBALS "${HPGCC39_HOME}/lib/MMUglobals.o")
set(HPGCC39_LINKER_SCRIPT "${HPGCC39_HOME}/lib/MMUld.script")

foreach(_required
    "${HPGCC39_MMU_LOADER}"
    "${HPGCC39_MMU_GLOBALS}"
    "${HPGCC39_LINKER_SCRIPT}"
)
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Missing HPGCC39 SDK input: ${_required}")
    endif()
endforeach()
