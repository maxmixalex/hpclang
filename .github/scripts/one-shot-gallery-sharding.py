#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected one patch anchor, found {text.count(old)}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1. Deterministically shard the generated catalog without baking the current
# face count into CMake. Round-robin assignment keeps each shard representative
# while preserving sorted order inside each shard.
cmake = ROOT / "tests/e2e/font-gallery/CMakeLists.txt"
replace_once(
    cmake,
    '''set(GALLERY_FONT_IDS ${HPCLANG_FONT_IDS})
list(SORT GALLERY_FONT_IDS)
if(HPCLANG_GALLERY_PROFILE STREQUAL "stock-smoke")
    list(GET GALLERY_FONT_IDS 0 GALLERY_STOCK_FACE)
    set(GALLERY_FONT_IDS "${GALLERY_STOCK_FACE}")
elseif(NOT HPCLANG_GALLERY_PROFILE STREQUAL "full")
    message(FATAL_ERROR
        "HPCLANG_GALLERY_PROFILE must be full or stock-smoke")
endif()
''',
    '''set(GALLERY_FONT_IDS ${HPCLANG_FONT_IDS})
list(SORT GALLERY_FONT_IDS)
set(HPCLANG_GALLERY_SHARD_INDEX "0" CACHE STRING
    "Zero-based exhaustive Gallery shard index")
set(HPCLANG_GALLERY_SHARD_COUNT "1" CACHE STRING
    "Number of exhaustive Gallery shards")

if(HPCLANG_GALLERY_PROFILE STREQUAL "stock-smoke")
    list(GET GALLERY_FONT_IDS 0 GALLERY_STOCK_FACE)
    set(GALLERY_FONT_IDS "${GALLERY_STOCK_FACE}")
elseif(HPCLANG_GALLERY_PROFILE STREQUAL "full")
    if(NOT HPCLANG_GALLERY_SHARD_INDEX MATCHES "^[0-9]+$" OR
       NOT HPCLANG_GALLERY_SHARD_COUNT MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "Gallery shard index/count must be non-negative/positive integers")
    endif()
    if(HPCLANG_GALLERY_SHARD_INDEX GREATER_EQUAL HPCLANG_GALLERY_SHARD_COUNT)
        message(FATAL_ERROR
            "Gallery shard index must be smaller than shard count")
    endif()

    if(HPCLANG_GALLERY_SHARD_COUNT GREATER 1)
        set(GALLERY_SHARD_FONT_IDS)
        set(gallery_catalog_index 0)
        foreach(font_id IN LISTS GALLERY_FONT_IDS)
            math(EXPR gallery_owner
                "${gallery_catalog_index} % ${HPCLANG_GALLERY_SHARD_COUNT}")
            if(gallery_owner EQUAL HPCLANG_GALLERY_SHARD_INDEX)
                list(APPEND GALLERY_SHARD_FONT_IDS "${font_id}")
            endif()
            math(EXPR gallery_catalog_index "${gallery_catalog_index} + 1")
        endforeach()
        if(NOT GALLERY_SHARD_FONT_IDS)
            message(FATAL_ERROR
                "Gallery shard ${HPCLANG_GALLERY_SHARD_INDEX}/${HPCLANG_GALLERY_SHARD_COUNT} is empty")
        endif()
        set(GALLERY_FONT_IDS ${GALLERY_SHARD_FONT_IDS})
    endif()
else()
    message(FATAL_ERROR
        "HPCLANG_GALLERY_PROFILE must be full or stock-smoke")
endif()
''',
)
replace_once(
    cmake,
    '''message(STATUS
    "Font Gallery (${HPCLANG_GALLERY_PROFILE}): ${face_index} generated faces from ${family_count} families + 2 native faces")
''',
    '''message(STATUS
    "Font Gallery (${HPCLANG_GALLERY_PROFILE}, shard ${HPCLANG_GALLERY_SHARD_INDEX}/${HPCLANG_GALLERY_SHARD_COUNT}): ${face_index} generated faces from ${family_count} families + 2 native faces")
''',
)

# 2. Make the ABI code-window limit explicit at pack time. This catches any
# future over-sized HPC1 payload before a launcher can fail opaquely.
packer = ROOT / "tools/hpclang-pack.py"
replace_once(
    packer,
    '''CODE_BASE = 0x01220000
DATA_BASE = 0x01101000
''',
    '''CODE_BASE = 0x01220000
CODE_WINDOW_END = 0x01300000
CODE_CAPACITY = CODE_WINDOW_END - CODE_BASE
DATA_BASE = 0x01101000
''',
)
replace_once(
    packer,
    '''    image_end = max(s.paddr + s.filesz for s in initialized)
    image = bytearray(image_end - CODE_BASE)
''',
    '''    image_end = max(s.paddr + s.filesz for s in initialized)
    image_size = image_end - CODE_BASE
    if image_size > CODE_CAPACITY:
        raise ValueError(
            f"HPC1 initialized image is {image_size} bytes, exceeding the "
            f"{CODE_CAPACITY}-byte code window"
        )
    image = bytearray(image_size)
''',
)

# 3. Pass shard coordinates into CMake and gate the raw synthetic Direct
# launcher against the x50ng Gallery injection window (224 pages, with three
# pages reserved for L2/scratch/sentinel).
build = ROOT / "tools/build-e2e.sh"
replace_once(
    build,
    '''    font-gallery)
        E2E_SOURCE="$ROOT/tests/e2e/font-gallery"
        E2E_TARGET="hpclang_font_gallery"
        E2E_ELF="hpclang-font-gallery.elf"
        E2E_CMAKE_ARGS=(-DHPCLANG_GALLERY_PROFILE=full)
        WRAP_LAUNCHERS=0
        ;;
''',
    '''    font-gallery)
        E2E_SOURCE="$ROOT/tests/e2e/font-gallery"
        E2E_TARGET="hpclang_font_gallery"
        E2E_ELF="hpclang-font-gallery.elf"
        GALLERY_SHARD_INDEX="${HPCLANG_GALLERY_SHARD_INDEX:-0}"
        GALLERY_SHARD_COUNT="${HPCLANG_GALLERY_SHARD_COUNT:-1}"
        E2E_CMAKE_ARGS=(
            -DHPCLANG_GALLERY_PROFILE=full
            -DHPCLANG_GALLERY_SHARD_INDEX="$GALLERY_SHARD_INDEX"
            -DHPCLANG_GALLERY_SHARD_COUNT="$GALLERY_SHARD_COUNT"
        )
        WRAP_LAUNCHERS=0
        ;;
''',
)
replace_once(
    build,
    '''if (( WRAP_LAUNCHERS )); then
    printf 'PASS: %s E2E application and wrapped target launchers built.\\n' "$MODE"
else
    printf 'PASS: %s E2E application and raw Direct target launchers built.\\n' "$MODE"
fi
''',
    '''if [[ "$MODE" == font-gallery ]]; then
    # prepare-x50ng-gallery.py expands the synthetic injection mapping to 224
    # pages. The frontend reserves three of them around the stage0 image.
    gallery_launcher_limit=$(( (224 - 3) * 4096 ))
    for launcher in "$OUT"/launchers/*.bin; do
        launcher_size="$(stat -c '%s' "$launcher")"
        if (( launcher_size > gallery_launcher_limit )); then
            echo "Direct Gallery launcher $(basename "$launcher") is ${launcher_size} bytes; limit is ${gallery_launcher_limit}" >&2
            exit 1
        fi
    done
    printf 'index=%s\\ncount=%s\\n' \\
        "$GALLERY_SHARD_INDEX" "$GALLERY_SHARD_COUNT" \\
        > "$OUT/e2e.gallery-shard"
fi

if (( WRAP_LAUNCHERS )); then
    printf 'PASS: %s E2E application and wrapped target launchers built.\\n' "$MODE"
else
    printf 'PASS: %s E2E application and raw Direct target launchers built.\\n' "$MODE"
fi
''',
)

# 4. Avoid redownloading the same firmware/NOR image once per shard. Use a
# target-specific cache file so the two hp39 family processes do not race.
run_gallery = ROOT / "tests/e2e/harness/run-gallery.sh"
replace_once(
    run_gallery,
    '''    hp39)
        curl -fL --retry 3 \\
            "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \\
            -o "$ROOT/build/$HPCLANG_E2E_ARCHIVE"
        unzip -p \\
            "$ROOT/build/$HPCLANG_E2E_ARCHIVE" \\
            "$HPCLANG_E2E_MEMBER" \\
            > "$ROOT/build/$TARGET-firmware.bin"
''',
    '''    hp39)
        archive="$ROOT/build/$TARGET-$HPCLANG_E2E_ARCHIVE"
        if [[ ! -s "$archive" ]]; then
            curl -fL --retry 3 \\
                "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \\
                -o "$archive.tmp"
            mv "$archive.tmp" "$archive"
        fi
        unzip -p "$archive" "$HPCLANG_E2E_MEMBER" \\
            > "$ROOT/build/$TARGET-firmware.bin"
''',
)
replace_once(
    run_gallery,
    '''    hp48)
        curl -fL --retry 3 \\
            https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \\
            -o "$ROOT/build/hp48gii-nor.bin"
        printf '%s  %s\\n' \\
            4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \\
            "$ROOT/build/hp48gii-nor.bin" |
            sha256sum -c -
        python3 "$ROOT/tools/prepare-hp48gii-nor.py" \\
            "$ROOT/build/hp48gii-nor.bin" "$STATE"
''',
    '''    hp48)
        nor="$ROOT/build/$TARGET-nor.bin"
        if [[ ! -s "$nor" ]]; then
            curl -fL --retry 3 \\
                https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \\
                -o "$nor.tmp"
            mv "$nor.tmp" "$nor"
        fi
        printf '%s  %s\\n' \\
            4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \\
            "$nor" |
            sha256sum -c -
        python3 "$ROOT/tools/prepare-hp48gii-nor.py" "$nor" "$STATE"
''',
)

# 5. Family orchestration now runs a target across all shard payloads. Targets
# remain parallel, and the shard wrapper continues after a failed shard so one
# bad slice does not suppress diagnostics from later slices.
family = ROOT / "tests/e2e/harness/run-gallery-family.sh"
replace_once(
    family,
    '''for target in "${TARGETS[@]}"; do
    bash "$ROOT/tests/e2e/harness/run-gallery.sh" "$target" "$APP_DIR" &
    PIDS+=("$!")
done
''',
    '''for target in "${TARGETS[@]}"; do
    bash "$ROOT/tests/e2e/harness/run-gallery-shards.sh" "$target" "$APP_DIR" &
    PIDS+=("$!")
done
''',
)

# 6. Add the shard aggregator. Each payload still contains the two native
# entries because that is part of the runtime Gallery contract; only shard 0
# contributes those duplicate frames/counts to the aggregate evidence.
shards = ROOT / "tests/e2e/harness/run-gallery-shards.sh"
if shards.exists():
    raise SystemExit(f"{shards}: already exists")
shards.write_text(r'''#!/usr/bin/env bash
set -u -o pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <target> <gallery-build-root>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_ROOT="$(realpath "$2")"
export TARGET

mapfile -t SHARDS < <(find "$APP_ROOT" -mindepth 1 -maxdepth 1 -type d -name 'shard-*' | sort)
if (( ${#SHARDS[@]} == 0 )); then
    if [[ -f "$APP_ROOT/e2e.hpc" ]]; then
        exec bash "$ROOT/tests/e2e/harness/run-gallery.sh" "$TARGET" "$APP_ROOT"
    fi
    echo "no Gallery shard payloads below: $APP_ROOT" >&2
    exit 2
fi

ARCHIVE_ROOT="$ROOT/build/$TARGET-font-gallery-shards"
AGG_FRAMES="$ROOT/build/$TARGET-font-gallery-frames-aggregate"
rm -rf "$ARCHIVE_ROOT" "$AGG_FRAMES"
mkdir -p "$ARCHIVE_ROOT" "$AGG_FRAMES"

status=0
passed=0
for shard_dir in "${SHARDS[@]}"; do
    shard_name="$(basename "$shard_dir")"
    meta="$shard_dir/e2e.gallery-shard"
    if [[ ! -f "$meta" ]]; then
        echo "missing shard metadata: $meta" >&2
        status=1
        continue
    fi
    shard_index="$(awk -F= '$1 == "index" {print $2}' "$meta")"
    shard_count="$(awk -F= '$1 == "count" {print $2}' "$meta")"
    printf 'Gallery %s %s (%s/%s)\n' "$TARGET" "$shard_name" "$shard_index" "$shard_count"

    if bash "$ROOT/tests/e2e/harness/run-gallery.sh" "$TARGET" "$shard_dir"; then
        passed=$((passed + 1))
    else
        rc=$?
        echo "FAIL $TARGET $shard_name (exit $rc)" >&2
        status=1
    fi

    dst="$ARCHIVE_ROOT/$shard_name"
    mkdir -p "$dst"
    for suffix in before.pgm before.pgm.png pass.pgm pass.pgm.png return.pgm return.pgm.png e2e.json font-gallery.pgm font-gallery.pgm.png; do
        src="$ROOT/build/$TARGET-$suffix"
        [[ -f "$src" ]] && cp "$src" "$dst/$suffix"
    done
    frames="$ROOT/build/$TARGET-font-gallery-frames"
    if [[ -d "$frames" ]]; then
        mkdir -p "$dst/frames"
        cp -a "$frames"/. "$dst/frames/"
        while IFS= read -r frame; do
            base="$(basename "$frame")"
            face_index="${base%%-*}"
            # Native mini/main are local face 0/1 in every shard. Keep them in
            # shard 0 only so aggregate evidence represents unique entries.
            if (( shard_index > 0 && 10#$face_index < 2 )); then
                continue
            fi
            cp "$frame" "$AGG_FRAMES/${shard_name}-${base}"
        done < <(find "$frames" -maxdepth 1 -type f -name '*.pgm' | sort)
    fi
done

if (( passed == 0 )); then
    echo "no Gallery shard completed for $TARGET" >&2
    exit 1
fi

mapfile -t aggregate_frames < <(find "$AGG_FRAMES" -maxdepth 1 -type f -name '*.pgm' | sort)
if (( ${#aggregate_frames[@]} == 0 )); then
    echo "no aggregate Gallery frames for $TARGET" >&2
    exit 1
fi
python3 "$ROOT/tools/merge-pgm-grid.py" \
    "$ROOT/build/$TARGET-font-gallery.pgm" "${aggregate_frames[@]}"
python3 "$ROOT/tools/pgm-to-png.py" \
    "$ROOT/build/$TARGET-font-gallery.pgm" \
    "$ROOT/build/$TARGET-font-gallery.pgm.png"

python3 - "$ARCHIVE_ROOT" "$ROOT/build/$TARGET-e2e.json" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
out = Path(sys.argv[2])
records = []
for shard in sorted(root.glob("shard-*")):
    path = shard / "e2e.json"
    if path.is_file():
        records.append((shard.name, json.loads(path.read_text(encoding="utf-8"))))
if not records:
    raise SystemExit("no completed shard JSON records")

unique_faces = 0
unique_frames = 0
for pos, (_, record) in enumerate(records):
    duplicate_native = 0 if pos == 0 else 2
    unique_faces += int(record.get("gallery_count", 0)) - duplicate_native
    unique_frames += int(record.get("gallery_frames", 0)) - 2 * duplicate_native

summary = {
    "status": "pass" if all(r.get("status") == "pass" for _, r in records) else "partial",
    "result": records[-1][1].get("result", "0x00000000"),
    "gallery_count": unique_faces,
    "gallery_frames": unique_frames,
    "saw_pass": all(bool(r.get("saw_pass")) for _, r in records),
    "returned": all(bool(r.get("returned")) for _, r in records),
    "return_settled": all(bool(r.get("return_settled")) for _, r in records),
    "shard_count": len(records),
    "shards": [{"name": name, **record} for name, record in records],
}
out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(
    f"Font Gallery aggregate: {unique_faces} unique faces / "
    f"{unique_frames} captures across {len(records)} shards"
)
PY

if (( status != 0 )); then
    exit "$status"
fi
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery.json"
''', encoding="utf-8")
shards.chmod(0o755)

print("Gallery sharding patch staged")
