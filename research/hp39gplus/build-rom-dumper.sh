#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <output-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$(realpath -m "$1")"
BUILD="$ROOT/build/hp39gplus-romdump"
SDK="${HP39_APLET_HOME:-$ROOT/.cache/hp39-aplet/sdk}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="${ARM_CC:-$ATFE/bin/clang}"
LD="${ARM_LD:-$ATFE/bin/ld.lld}"
OBJCOPY="${ARM_OBJCOPY:-$ATFE/bin/llvm-objcopy}"

rm -rf "$OUT" "$BUILD"
mkdir -p "$OUT/packages" "$OUT/tools" "$BUILD"

for tool in "$CC" "$LD" "$OBJCOPY" python3 cc; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

if [[ ! -f "$SDK/.hpclang-prepared" ]]; then
    echo "HPGCC39 SDK is not prepared; run tools/bootstrap-hp39-aplet.sh first" >&2
    exit 1
fi

cc -O2 "$SDK/src/hp2aplet.c" -o "$BUILD/hp2aplet"

for index in $(seq 0 63); do
    tag="$(printf '%02X' "$index")"
    offset=$(( index * 0x4000 ))
    work="$BUILD/$tag"
    package="$OUT/packages/$tag"
    mkdir -p "$work" "$package"

    "$CC" --target=arm-none-eabi -mcpu=arm920t -marm \
        -ffreestanding -fno-pic -x assembler-with-cpp \
        -DHP39GPLUS_DUMP_OFFSET="$offset" \
        -c "$ROOT/research/hp39gplus/rom-dump.S" \
        -o "$work/rom-dump.o"

    "$LD" \
        -T "$ROOT/research/hp39gplus/rom-dump.ld" \
        "$work/rom-dump.o" \
        -o "$work/rom-dump.elf"

    "$OBJCOPY" -O binary "$work/rom-dump.elf" "$work/rom-dump.bin"

    python3 "$ROOT/research/hp39gplus/boot-dump-wrap.py" \
        "$work/rom-dump.bin" "$work/rom-dump.hp"

    cat > "$work/hp39prj.dat" <<EOF
HP39 Aplet Project
H39 ROM $tag
hpclang read-only HP39g+ physical NOR dumper
N
EOF

    (
        cd "$work"
        HPGCC="$SDK/data" "$BUILD/hp2aplet" "$work/rom-dump.hp"
    )

    cp "$work/HPGCC000.000" "$package/HPGCC000.000"
    cp "$work/hp39dir.000" "$package/HP39DIR.000"
    cp "$work/hp39dir.cur" "$package/HP39DIR.CUR"
    cp "$work/LIB275L.000" "$package/LIB275L.000"
    printf 'chunk=%s\noffset=0x%05X\nbytes=0x4000\n' \
        "$tag" "$offset" > "$package/CHUNK.txt"
done

cp "$ROOT/research/hp39gplus/extract-rom-dump.py" "$OUT/tools/"
cp "$ROOT/research/hp39gplus/collect-rom-dump.py" "$OUT/tools/"
cp "$ROOT/research/hp39gplus/ROM-DUMP-README.md" "$OUT/README.txt"

python3 - "$OUT" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
items = []
for index in range(64):
    tag = f"{index:02X}"
    app = root / "packages" / tag / "HPGCC000.000"
    items.append({
        "index": index,
        "tag": tag,
        "offset": f"0x{index * 0x4000:05x}",
        "bytes": app.stat().st_size,
        "sha256": hashlib.sha256(app.read_bytes()).hexdigest(),
    })
(root / "PACKAGE-MANIFEST.json").write_text(
    json.dumps({"chunks": items}, indent=2) + "\n",
    encoding="utf-8",
)
PY

echo "Built 64 read-only HP39g+ ROM-dump packages in $OUT/packages"
