# Optional compiler flags for probing binary interoperability with
# historical HPGCC 1.x/2.x GCC objects.
#
# This is NOT the default 39gs source-modern profile. It exists to make the
# ABI experiment explicit and independently testable.

add_compile_options(
    -mabi=apcs-gnu
    -mfloat-abi=soft
    -mlittle-endian
)

set(HPCLANG_LEGACY_ABI_APCS_GNU TRUE CACHE BOOL
    "Compile using the historical APCS GNU-compatible calling convention")
