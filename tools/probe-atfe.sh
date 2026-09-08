#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/atfe-probe}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="$ATFE/bin/clang"
CXX="$ATFE/bin/clang++"
READELF="$ATFE/bin/llvm-readelf"

rm -rf "$OUT"
mkdir -p "$OUT"

TARGET=(
    --target=arm-none-eabi
    -march=armv4t
    -mthumb
    -mfpu=none
    -mfloat-abi=soft
    -mabi=aapcs
    -fno-exceptions
    -fno-rtti
)

COMMON=(
    "${TARGET[@]}"
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-pic
    -fno-pie
    -ffunction-sections
    -fdata-sections
    -I"$ROOT/include"
)

VISIBLE_TEST_DEFINES=()
if [[ "${HPCLANG_E2E_WAIT_FOR_ENTER:-0}" == "1" ]]; then
    VISIBLE_TEST_DEFINES+=(-DHPCLANG_E2E_WAIT_FOR_ENTER=1)
fi
if [[ "${HPCLANG_E2E_KEYBOARD:-0}" == "1" ]]; then
    VISIBLE_TEST_DEFINES+=(-DHPCLANG_E2E_KEYBOARD=1)
fi

echo "== host generic soft-float self-test =="
cc -std=c11 -O2 -Wall -Wextra -Werror \
    "$ROOT/runtime/core/armv4t-softfloat.c" \
    "$ROOT/tests/armv4t-softfloat-host.c" \
    -lm -o "$OUT/armv4t-softfloat-host"
"$OUT/armv4t-softfloat-host"
echo "PASS: generic double soft-float matches host IEEE-754 reference cases."
echo

echo "== host monotonic-time inference self-test =="
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$ROOT/runtime/core" \
    "$ROOT/runtime/core/time-math.c" \
    "$ROOT/tests/time-math-host.c" \
    -o "$OUT/time-math-host"
"$OUT/time-math-host"
echo

echo "== host keyboard event self-test =="
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    -I"$ROOT/runtime/core" \
    "$ROOT/runtime/core/keyboard-event-engine.c" \
    "$ROOT/tests/keyboard-events-host.c" \
    -o "$OUT/keyboard-events-host"
"$OUT/keyboard-events-host"
echo

echo "== host display surface self-test =="
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$ROOT/include" \
    -I"$ROOT/runtime/core" \
    "$ROOT/runtime/core/display-surface.c" \
    "$ROOT/tests/display-surface-host.c" \
    -o "$OUT/display-surface-host"
"$OUT/display-surface-host"
echo

echo "== selected ATfE multilib =="
"$CC" "${TARGET[@]}" -print-multi-directory
echo
"$CC" --target=arm-none-eabi -print-multi-lib | grep armv4t || true

"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/start.S" -o "$OUT/start.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/armv4t-eabi-div.S" -o "$OUT/armv4t-eabi-div.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/armv4t-softfloat-entry.S" -o "$OUT/armv4t-softfloat-entry.o"
"$CC" "${COMMON[@]}" -marm -O2 -fno-builtin     -c "$ROOT/runtime/core/armv4t-softfloat.c" -o "$OUT/armv4t-softfloat.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/runtime.c" -o "$OUT/runtime.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/device.c" -o "$OUT/device.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/display.c" -o "$OUT/display.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/display-surface.c" -o "$OUT/display-surface.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/keyboard.c" -o "$OUT/keyboard.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/keyboard-event-engine.c" -o "$OUT/keyboard-event-engine.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/keyboard-events.c" -o "$OUT/keyboard-events.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/time-math.c" -o "$OUT/time-math.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/time.c" -o "$OUT/time.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/libc/sbrk.c" -o "$OUT/sbrk.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-runtime-smoke.c" -o "$OUT/smoke.o"

"$CC" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/armv4t-eabi-div.o" "$OUT/armv4t-softfloat-entry.o" "$OUT/armv4t-softfloat.o" "$OUT/runtime.o" "$OUT/device.o" "$OUT/display.o" "$OUT/display-surface.o" "$OUT/keyboard.o" "$OUT/keyboard-event-engine.o" "$OUT/keyboard-events.o" "$OUT/time-math.o" "$OUT/time.o" "$OUT/sbrk.o" "$OUT/smoke.o"     -o "$OUT/hpclang-atfe.elf"

echo
echo "== undefined global symbols =="
if "$READELF" -Ws "$OUT/hpclang-atfe.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global symbols" >&2
    exit 1
fi
echo "none"

echo
echo "== ARMv4T EABI divmod wrapper check =="
"$ATFE/bin/llvm-objdump" -d "$OUT/hpclang-atfe.elf" |
    sed -n '/<__aeabi_uldivmod>:/,/^$/p'
"$ATFE/bin/llvm-objdump" -d "$OUT/hpclang-atfe.elf" |
    sed -n '/<__aeabi_ldivmod>:/,/^$/p' || true

echo
echo "== ARMv4T soft-float entry check =="
for sym in __aeabi_dmul __aeabi_dadd __aeabi_dcmpgt; do
    "$ATFE/bin/llvm-objdump" -d "$OUT/hpclang-atfe.elf" |
        sed -n "/<$sym>:/,/^$/p"
done

echo
echo "== resolved runtime helpers =="
"$READELF" -Ws "$OUT/hpclang-atfe.elf" |
    grep -E '(__aeabi_|memcpy|strlen|snprintf|malloc|sbrk)' |
    head -80

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe.elf"     "$OUT/hpclang-atfe.hpc"

echo
echo "PASS: ATfE Armv4T compiler-rt + picolibc link into HPC1."


echo
echo "== libc++ Armv4T smoke =="

"$CXX" "${COMMON[@]}" -std=c++23 -Os     -c "$ROOT/tests/atfe-cxx-smoke.cpp" -o "$OUT/cxx-smoke.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-platform-stubs.c" -o "$OUT/platform-stubs.o"

"$CXX" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/armv4t-eabi-div.o" "$OUT/armv4t-softfloat-entry.o" "$OUT/armv4t-softfloat.o" "$OUT/runtime.o" "$OUT/device.o" "$OUT/display.o" "$OUT/display-surface.o" "$OUT/keyboard.o" "$OUT/keyboard-event-engine.o" "$OUT/keyboard-events.o" "$OUT/time-math.o" "$OUT/time.o" "$OUT/sbrk.o" "$OUT/platform-stubs.o" "$OUT/cxx-smoke.o"     -o "$OUT/hpclang-atfe-cxx.elf"

if "$READELF" -Ws "$OUT/hpclang-atfe-cxx.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global C++ symbols" >&2
    exit 1
fi

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe-cxx.elf"     "$OUT/hpclang-atfe-cxx.hpc"

echo "PASS: ATfE Armv4T C++23 + libc++ links into HPC1."

echo
echo "== common bootstrap ARMv4T link =="

"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/runtime/bootstrap/mmu.c" -o "$OUT/bootstrap-mmu.o"
"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/runtime/bootstrap/run.c" -o "$OUT/bootstrap-run.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/bootstrap/sync.S" -o "$OUT/bootstrap-sync.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/bootstrap/enter.S" -o "$OUT/bootstrap-enter.o"

"$ATFE/bin/ld.lld" -m armelf -r     "$OUT/bootstrap-mmu.o"     "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o"     "$OUT/bootstrap-enter.o"     -o "$OUT/bootstrap.o"

if "$ATFE/bin/llvm-nm" -u "$OUT/bootstrap.o" | grep .; then
    echo "unexpected undefined bootstrap symbols" >&2
    exit 1
fi

echo "PASS: common ARMv4T bootstrap links without platform/runtime dependencies."

echo
echo "== stock ARMToolbox fixed stage1 =="

"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/platform/armtoolbox/stage1.c" -o "$OUT/stage1.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/platform/armtoolbox/stage1-entry.S" -o "$OUT/stage1-entry.o"

"$ATFE/bin/ld.lld" -m armelf     -T "$ROOT/platform/armtoolbox/stage1.ld"     "$OUT/stage1-entry.o"     "$OUT/stage1.o"     "$OUT/bootstrap-mmu.o"     "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o"     "$OUT/bootstrap-enter.o"     -o "$OUT/armtoolbox-stage1.elf"

if "$ATFE/bin/llvm-nm" -u "$OUT/armtoolbox-stage1.elf" | grep .; then
    echo "unexpected undefined stage1 symbols" >&2
    exit 1
fi

"$ATFE/bin/llvm-objcopy" -O binary     "$OUT/armtoolbox-stage1.elf"     "$OUT/armtoolbox-stage1.bin"

"$READELF" -h "$OUT/armtoolbox-stage1.elf" |
    grep -E 'Entry point address|Machine'
"$READELF" -l "$OUT/armtoolbox-stage1.elf" |
    grep -E 'LOAD|Entry point'

echo "PASS: stock ARMToolbox stage1 is fixed-address, non-PIC and self-contained."

echo
echo "== HP39 hardware runtime validation =="

"$CC" "${COMMON[@]}" "${VISIBLE_TEST_DEFINES[@]}" -Os     -c "$ROOT/tests/hp39-visible-smoke.c" -o "$OUT/hp39-visible-smoke.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/hp39-runtime-diag.c" -o "$OUT/hp39-runtime-diag.o"

"$CC" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/armv4t-eabi-div.o" "$OUT/armv4t-softfloat-entry.o" "$OUT/armv4t-softfloat.o" "$OUT/hp39-runtime-diag.o" "$OUT/device.o" "$OUT/display.o" "$OUT/display-surface.o" "$OUT/keyboard.o" "$OUT/keyboard-event-engine.o" "$OUT/keyboard-events.o" "$OUT/time-math.o" "$OUT/time.o" "$OUT/sbrk.o"     "$OUT/hp39-visible-smoke.o"     -o "$OUT/hp39-visible.elf"

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hp39-visible.elf"     "$OUT/hp39-visible.hpc"

python3 - "$OUT/hp39-visible.hpc" <<'PY'
from pathlib import Path
import struct
import sys

magic, image_size, entry_offset, rw_static = struct.unpack(
    "<4sIII", Path(sys.argv[1]).read_bytes()[:16]
)
assert magic == b"HPC1"
assert image_size > 0
assert entry_offset == 0
assert rw_static >= 4, "pure-BSS RW footprint was lost by the packer"
print(f"PASS: visible HPC1 RW footprint is {rw_static} bytes")
PY

result_addr="$("$ATFE/bin/llvm-nm" -n "$OUT/hp39-visible.elf" |
    awk '$3 == "hpclang_e2e_result" {print "0x"$1}')"
test -n "$result_addr"
echo "E2E result address: $result_addr"
echo "$result_addr" > "$OUT/hp39-visible.result-address"

echo
echo "== stock ARMToolbox stage0 bundle =="

cat > "$OUT/stage0-bundle.S" <<EOF
.section .rodata.hpclang.bundle,"a",%progbits
.align 2

.global __stage1_blob_start
.global __stage1_blob_end
.global __stage1_blob_size
.global __hpc1_blob_start
.global __hpc1_blob_end

__stage1_blob_start:
.incbin "$OUT/armtoolbox-stage1.bin"
__stage1_blob_end:
.set __stage1_blob_size, __stage1_blob_end - __stage1_blob_start

.align 2
__hpc1_blob_start:
.incbin "$OUT/hp39-visible.hpc"
__hpc1_blob_end:
EOF

"$CC" "${COMMON[@]}" -marm -DHPCLANG_STAGE0_MODEL=2     -c "$ROOT/platform/armtoolbox/stage0.S" -o "$OUT/stage0.o"
"$CC" "${COMMON[@]}" -marm     -c "$OUT/stage0-bundle.S" -o "$OUT/stage0-bundle.o"

"$ATFE/bin/ld.lld" -m armelf     -T "$ROOT/platform/armtoolbox/stage0.ld"     "$OUT/stage0.o" "$OUT/stage0-bundle.o"     -o "$OUT/armtoolbox-stage0.elf"

entry="$("$READELF" -h "$OUT/armtoolbox-stage0.elf" |
    awk '/Entry point address:/ {print $4}')"
test "$entry" = "0x0"

if "$ATFE/bin/llvm-nm" -u "$OUT/armtoolbox-stage0.elf" | grep .; then
    echo "unexpected undefined stage0 symbols" >&2
    exit 1
fi

"$ATFE/bin/llvm-objcopy" -O binary     "$OUT/armtoolbox-stage0.elf"     "$OUT/armtoolbox-stage0.bin"

python3 "$ROOT/tools/armtoolbox-wrap.py"     "$OUT/armtoolbox-stage0.bin"     "$OUT/hp39gs-hpc1.hp"

python3 - "$OUT/hp39gs-hpc1.hp" <<'PY'
from pathlib import Path
import struct
import sys

p = Path(sys.argv[1]).read_bytes()
assert p[:8] == b"HPHP49-C"
assert p[13:20] == b"\x4c\xb3\x80A>CP"
assert p[-4:] == struct.pack("<I", 0x3176B34C)
assert p[-20:-16] == struct.pack("<I", 0)
print(f"PASS: single-form stock ARMToolbox envelope, {len(p)} bytes")
PY

echo "PASS: PIC stage0 + fixed stage1 + HPC1 bundle builds without legacy runtime objects."
