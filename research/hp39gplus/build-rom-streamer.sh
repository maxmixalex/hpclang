#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <output-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$(realpath -m "$1")"
BUILD="$ROOT/build/hp39gplus-romstream"
SDK="${HP39_APLET_HOME:-$ROOT/.cache/hp39-aplet/sdk}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="${ARM_CC:-$ATFE/bin/clang}"
LD="${ARM_LD:-$ATFE/bin/ld.lld}"
OBJCOPY="${ARM_OBJCOPY:-$ATFE/bin/llvm-objcopy}"
READELF="${ARM_READELF:-$ATFE/bin/llvm-readelf}"
NM="${ARM_NM:-$ATFE/bin/llvm-nm}"

rm -rf "$OUT" "$BUILD"
mkdir -p "$OUT/calculator" "$OUT/tools" "$BUILD"

for tool in "$CC" "$LD" "$OBJCOPY" "$READELF" "$NM" python3 cc go; do
    [[ -x "$tool" ]] || command -v "$tool" >/dev/null 2>&1 || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

if [[ ! -f "$SDK/.hpclang-prepared" ]]; then
    echo "HPGCC39 SDK is not prepared; run tools/bootstrap-hp39-aplet.sh first" >&2
    exit 1
fi

cc -O2 "$SDK/src/hp2aplet.c" -o "$BUILD/hp2aplet"

CFLAGS=(
    --target=arm-none-eabi -mcpu=arm920t -marm -msoft-float
    -ffreestanding -fno-builtin -fno-stack-protector
    -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-jump-tables
    -Os -Wall -Wextra -Werror
)

"$CC" "${CFLAGS[@]}" \
    -c "$ROOT/research/hp39gplus/rom-stream.c" \
    -o "$BUILD/rom-stream.o"

"$CC" --target=arm-none-eabi -mcpu=arm920t -marm \
    -ffreestanding -fno-pic \
    -c "$ROOT/research/hp39gplus/rom-stream-entry.S" \
    -o "$BUILD/rom-stream-entry.o"

"$LD" \
    -T "$ROOT/research/hp39gplus/rom-stream.ld" \
    --gc-sections \
    "$BUILD/rom-stream-entry.o" "$BUILD/rom-stream.o" \
    -o "$BUILD/rom-stream.elf"

"$OBJCOPY" -O binary "$BUILD/rom-stream.elf" "$BUILD/rom-stream.bin"

if "$READELF" -r "$BUILD/rom-stream.elf" | grep -Eq '[0-9a-fA-F]+[[:space:]]+[0-9a-fA-F]+[[:space:]]+R_ARM_'; then
    echo "ROM streamer contains load-time relocations" >&2
    "$READELF" -r "$BUILD/rom-stream.elf" >&2
    exit 1
fi

if [[ -n "$("$NM" -u "$BUILD/rom-stream.elf")" ]]; then
    echo "ROM streamer has undefined symbols" >&2
    "$NM" -u "$BUILD/rom-stream.elf" >&2
    exit 1
fi

python3 -m py_compile \
    "$ROOT/research/hp39gplus/rom-stream-wrap.py" \
    "$ROOT/research/hp39gplus/receive-rom-stream.py"

python3 "$ROOT/research/hp39gplus/rom-stream-wrap.py" \
    "$BUILD/rom-stream.bin" "$BUILD/rom-stream.hp"

cat > "$BUILD/hp39prj.dat" <<'PROJECT'
HP39 Aplet Project
H39 ROM USB
hpclang read-only HP39g+ USB ROM streamer
N
PROJECT

(
    cd "$BUILD"
    HPGCC="$SDK/data" "$BUILD/hp2aplet" "$BUILD/rom-stream.hp"
)

cp "$BUILD/HPGCC000.000" "$OUT/calculator/HPGCC000.000"
cp "$BUILD/hp39dir.000" "$OUT/calculator/HP39DIR.000"
cp "$BUILD/hp39dir.cur" "$OUT/calculator/HP39DIR.CUR"
cp "$BUILD/LIB275L.000" "$OUT/calculator/LIB275L.000"
cp "$ROOT/research/hp39gplus/receive-rom-stream.py" "$OUT/tools/"
cp "$ROOT/research/hp39gplus/ROM-STREAM-README.md" "$OUT/README.txt"
printf 'hidapi>=0.14\n' > "$OUT/requirements.txt"

python3 - "$OUT" "$BUILD/rom-stream.bin" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
payload = Path(sys.argv[2])
files = {}
for rel in (
    "calculator/HPGCC000.000",
    "calculator/HP39DIR.000",
    "calculator/HP39DIR.CUR",
    "calculator/LIB275L.000",
    "hp39gplus-rom-receiver.exe",
    "tools/receive-rom-stream.py",
):
    p = root / rel
    files[rel] = {
        "bytes": p.stat().st_size,
        "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
    }
manifest = {
    "mode": "one-launch-raw-hid",
    "flash_bytes": 0x100000,
    "validation_block_bytes": 0x4000,
    "hid_data_bytes": 56,
    "arm_payload_bytes": payload.stat().st_size,
    "arm_payload_sha256": hashlib.sha256(payload.read_bytes()).hexdigest(),
    "files": files,
}
(root / "PACKAGE-MANIFEST.json").write_text(
    json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
)
PY

echo "Built one-launch read-only HP39g+ Raw HID ROM streamer in $OUT"
