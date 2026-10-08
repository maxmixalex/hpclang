#!/usr/bin/env bash
set -u -o pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <target> <gallery-build-root>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_ROOT="$(realpath "$2")"
export TARGET

case "$TARGET" in
    hp39gplus|hp39gs|hp40gs) NATIVE_FACE_COUNT=3 ;;
    *) NATIVE_FACE_COUNT=2 ;;
esac
export NATIVE_FACE_COUNT

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
            # Every shard repeats the target's concrete native ROM-font
            # prefix. Execute it every time, but retain it only once in
            # aggregate evidence so counts represent unique Gallery entries.
            if (( shard_index > 0 && 10#$face_index < NATIVE_FACE_COUNT )); then
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
import os
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
native_face_count = int(os.environ["NATIVE_FACE_COUNT"])
for name, record in records:
    shard_index = int(name.rsplit("-", 1)[1])
    duplicate_native = 0 if shard_index == 0 else native_face_count
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
