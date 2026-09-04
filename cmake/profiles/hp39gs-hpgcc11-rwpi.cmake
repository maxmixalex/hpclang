# Experimental 39g+/39gs/40gs legacy-object interoperability profile.
#
# This does NOT replace hp39gs-hpgcc11. The normal 39gs profile follows the
# hardware-proven 39gsX source-modern path.
#
# This profile is for C objects entered through experiments/hybrid-rwpi/start.S:
#   historical runtime keeps r10/GOT
#   bridge mirrors r10 -> r9
#   modern C uses ARM RWPI through r9/sb

add_compile_options(
    -mabi=apcs-gnu
    -mfloat-abi=soft
    -frwpi
)

add_compile_definitions(HPCLANG_EXPERIMENTAL_DUAL_BASE_RWPI=1)

set(HPCLANG_RWPI_STATIC_BASE "0x09000400" CACHE STRING
    "Expected historical HPGCC RAM/static-base address")

set(HPCLANG_RWPI_REQUIRES_SINGLE_RAM_PHDR TRUE CACHE BOOL
    "The linker must keep the complete HPGCC RAM image in one RW PT_LOAD")
