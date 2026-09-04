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

echo "== selected ATfE multilib =="
"$CC" "${TARGET[@]}" -print-multi-directory
echo
"$CC" --target=arm-none-eabi -print-multi-lib | grep armv4t || true

"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/start.S" -o "$OUT/start.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/runtime.c" -o "$OUT/runtime.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/libc/sbrk.c" -o "$OUT/sbrk.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-runtime-smoke.c" -o "$OUT/smoke.o"

"$CC" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o" "$OUT/smoke.o"     -o "$OUT/hpclang-atfe.elf"

echo
echo "== undefined global symbols =="
if "$READELF" -Ws "$OUT/hpclang-atfe.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global symbols" >&2
    exit 1
fi
echo "none"

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

"$CXX" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-cxx-smoke.cpp" -o "$OUT/cxx-smoke.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-platform-stubs.c" -o "$OUT/platform-stubs.o"

"$CXX" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o" "$OUT/platform-stubs.o" "$OUT/cxx-smoke.o"     -o "$OUT/hpclang-atfe-cxx.elf"

if "$READELF" -Ws "$OUT/hpclang-atfe-cxx.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global C++ symbols" >&2
    exit 1
fi

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe-cxx.elf"     "$OUT/hpclang-atfe-cxx.hpc"

echo "PASS: ATfE Armv4T libc++ links into HPC1."

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
echo "== visible HP39 HPC1 smoke =="

"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/hp39-visible-smoke.c" -o "$OUT/hp39-visible-smoke.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/tests/hp39-visible-smoke-irq.S" -o "$OUT/hp39-visible-smoke-irq.o"

"$CC" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o"     "$OUT/hp39-visible-smoke.o" "$OUT/hp39-visible-smoke-irq.o"     -o "$OUT/hp39-visible.elf"

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hp39-visible.elf"     "$OUT/hp39-visible.hpc"

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
assert p[-16:-12] == struct.pack("<I", 0)
print(f"PASS: single-form stock ARMToolbox envelope, {len(p)} bytes")
PY

echo "PASS: PIC stage0 + fixed stage1 + HPC1 bundle builds without legacy runtime objects."
