#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected one patch anchor, found {text.count(old)}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# Keep the public CI artifact shape stable: one build-e2e invocation produces
# all shard-specific raw launchers below launchers/*.bin. No workflow-level
# shard matrix or duplicated ELF/HPC artifacts are needed.
build = ROOT / "tools/build-e2e.sh"
replace_once(
    build,
    '''NM="$ATFE/bin/llvm-nm"

E2E_SOURCE="$ROOT/tests/e2e"
''',
    '''NM="$ATFE/bin/llvm-nm"

# The exhaustive Gallery exceeds the v1 code window when linked monolithically.
# If no explicit shard index is supplied, build a deterministic bundle of
# shards and collect their raw target launchers into the ordinary artifact.
# An explicit HPCLANG_GALLERY_SHARD_INDEX selects the inner single-shard path.
if [[ "$MODE" == font-gallery && -z "${HPCLANG_GALLERY_SHARD_INDEX+x}" ]]; then
    GALLERY_BUNDLE_COUNT="${HPCLANG_GALLERY_SHARD_COUNT:-20}"
    if [[ ! "$GALLERY_BUNDLE_COUNT" =~ ^[1-9][0-9]*$ ]]; then
        echo "HPCLANG_GALLERY_SHARD_COUNT must be a positive integer" >&2
        exit 2
    fi

    rm -rf "$OUT"
    mkdir -p "$OUT/launchers"
    representative_size=-1
    expected_result=""

    for (( shard_index = 0; shard_index < GALLERY_BUNDLE_COUNT; ++shard_index )); do
        printf -v shard_tag '%02d' "$shard_index"
        shard_out="$OUT/.shard-$shard_tag"
        echo "=== Direct Gallery shard $shard_index/$GALLERY_BUNDLE_COUNT ==="
        HPCLANG_GALLERY_SHARD_INDEX="$shard_index" \\
        HPCLANG_GALLERY_SHARD_COUNT="$GALLERY_BUNDLE_COUNT" \\
            "$0" font-gallery "$shard_out"

        shard_result="$(cat "$shard_out/e2e.result-address")"
        if [[ -z "$expected_result" ]]; then
            expected_result="$shard_result"
        elif [[ "$shard_result" != "$expected_result" ]]; then
            echo "Gallery shard result address changed: $shard_result != $expected_result" >&2
            exit 1
        fi

        for launcher in "$shard_out"/launchers/*.bin; do
            base="$(basename "$launcher" .bin)"
            cp "$launcher" "$OUT/launchers/$base-shard-$shard_tag.bin"
        done

        shard_size="$(stat -c '%s' "$shard_out/e2e.hpc")"
        if (( shard_size > representative_size )); then
            representative_size="$shard_size"
            cp "$shard_out/e2e.elf" "$OUT/e2e.elf"
            cp "$shard_out/e2e.hpc" "$OUT/e2e.hpc"
            cp "$shard_out/e2e.result-address" "$OUT/e2e.result-address"
        fi
        printf '%s %s %s\\n' "$shard_tag" "$shard_size" "$shard_result" \\
            >> "$OUT/e2e.gallery-shards.txt"
        rm -rf "$shard_out"
    done

    printf 'PASS: font-gallery bundle built with %s shards; largest HPC1 container %s bytes.\\n' \\
        "$GALLERY_BUNDLE_COUNT" "$representative_size"
    exit 0
fi

E2E_SOURCE="$ROOT/tests/e2e"
''',
)

# A shard runner selects the raw launcher directly from the shared build root.
run_gallery = ROOT / "tests/e2e/harness/run-gallery.sh"
replace_once(
    run_gallery,
    '''rm -rf "$STATE" "$FRAMES"
mkdir -p "$STATE" "$FRAMES"
''',
    '''rm -rf "$STATE" "$FRAMES"
rm -f \\
    "$ROOT/build/$TARGET-before.pgm" \\
    "$ROOT/build/$TARGET-before.pgm.png" \\
    "$ROOT/build/$TARGET-pass.pgm" \\
    "$ROOT/build/$TARGET-pass.pgm.png" \\
    "$ROOT/build/$TARGET-return.pgm" \\
    "$ROOT/build/$TARGET-return.pgm.png" \\
    "$ROOT/build/$TARGET-e2e.json" \\
    "$ROOT/build/$TARGET-font-gallery.pgm" \\
    "$ROOT/build/$TARGET-font-gallery.pgm.png"
mkdir -p "$STATE" "$FRAMES"
''',
)
replace_once(
    run_gallery,
    '''export HPCLANG_X50NG_STAGE0="$APP_DIR/launchers/$TARGET.bin"
''',
    '''export HPCLANG_X50NG_STAGE0="${HPCLANG_GALLERY_STAGE0:-$APP_DIR/launchers/$TARGET.bin}"
''',
)

shards = ROOT / "tests/e2e/harness/run-gallery-shards.sh"
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

mapfile -t SHARD_LAUNCHERS < <(
    find "$APP_ROOT/launchers" -maxdepth 1 -type f \
        -name "$TARGET-shard-*.bin" | sort
)
if (( ${#SHARD_LAUNCHERS[@]} == 0 )); then
    if [[ -f "$APP_ROOT/launchers/$TARGET.bin" ]]; then
        exec bash "$ROOT/tests/e2e/harness/run-gallery.sh" "$TARGET" "$APP_ROOT"
    fi
    echo "no Gallery shard launchers for $TARGET below: $APP_ROOT" >&2
    exit 2
fi

ARCHIVE_ROOT="$ROOT/build/$TARGET-font-gallery-shards"
AGG_FRAMES="$ROOT/build/$TARGET-font-gallery-frames-aggregate"
rm -rf "$ARCHIVE_ROOT" "$AGG_FRAMES"
mkdir -p "$ARCHIVE_ROOT" "$AGG_FRAMES"

status=0
passed=0
for launcher in "${SHARD_LAUNCHERS[@]}"; do
    filename="$(basename "$launcher" .bin)"
    shard_tag="${filename##*-shard-}"
    shard_name="shard-$shard_tag"
    shard_index=$((10#$shard_tag))
    printf 'Gallery %s %s (%s/%s)\n' \
        "$TARGET" "$shard_name" "$shard_index" "${#SHARD_LAUNCHERS[@]}"

    if HPCLANG_GALLERY_STAGE0="$launcher" \
        bash "$ROOT/tests/e2e/harness/run-gallery.sh" "$TARGET" "$APP_ROOT"; then
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
            # The runtime contract puts native mini/main at local face 0/1 in
            # every shard. Execute them every time, but retain them only once
            # in aggregate evidence so counts represent unique Gallery entries.
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

mapfile -t aggregate_frames < <(
    find "$AGG_FRAMES" -maxdepth 1 -type f -name '*.pgm' | sort
)
if (( ${#aggregate_frames[@]} == 0 )); then
    echo "no aggregate Gallery frames for $TARGET" >&2
    exit 1
fi
python3 "$ROOT/tools/merge-pgm-grid.py" \
    "$ROOT/build/$TARGET-font-gallery.pgm" "${aggregate_frames[@]}"
python3 "$ROOT/tools/pgm-to-png.py" \
    "$ROOT/build/$TARGET-font-gallery.pgm" \
    "$ROOT/build/$TARGET-font-gallery.pgm.png"

python3 - "$ARCHIVE_ROOT" "$ROOT/build/$TARGET-e2e.json" "${#SHARD_LAUNCHERS[@]}" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
out = Path(sys.argv[2])
expected_shards = int(sys.argv[3])
records = []
for shard in sorted(root.glob("shard-*")):
    path = shard / "e2e.json"
    if path.is_file():
        records.append((shard.name, json.loads(path.read_text(encoding="utf-8"))))
if not records:
    raise SystemExit("no completed shard JSON records")

unique_faces = 0
unique_frames = 0
for name, record in records:
    shard_index = int(name.rsplit("-", 1)[1])
    duplicate_native = 0 if shard_index == 0 else 2
    unique_faces += int(record.get("gallery_count", 0)) - duplicate_native
    unique_frames += int(record.get("gallery_frames", 0)) - 2 * duplicate_native

all_pass = (
    len(records) == expected_shards
    and all(r.get("status") == "pass" for _, r in records)
)
summary = {
    "status": "pass" if all_pass else "partial",
    "result": records[-1][1].get("result", "0x00000000"),
    "gallery_count": unique_faces,
    "gallery_frames": unique_frames,
    "saw_pass": all(bool(r.get("saw_pass")) for _, r in records),
    "returned": all(bool(r.get("returned")) for _, r in records),
    "return_settled": all(bool(r.get("return_settled")) for _, r in records),
    "shard_count": len(records),
    "expected_shards": expected_shards,
    "shards": [{"name": name, **record} for name, record in records],
}
out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(
    f"Font Gallery aggregate: {unique_faces} unique faces / "
    f"{unique_frames} captures across {len(records)}/{expected_shards} shards"
)
PY

if (( status != 0 )); then
    exit "$status"
fi
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery.json"
''', encoding="utf-8")
shards.chmod(0o755)

print("Gallery shard bundle patch staged")
