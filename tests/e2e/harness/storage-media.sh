#!/usr/bin/env bash
# Raw FAT cards for persistence across removal; directory-backed VVFAT may
# discard an uncommitted overlay when an application has changed its tree.

prepare_storage_media() {
    export HPCLANG_X50NG_SD_DIR="$1"
    export HPCLANG_E2E_STORAGE_CARD="$1-media-a.img"
    export HPCLANG_E2E_REPLACEMENT_CARD="$1-media-b.img"
    for card in "$HPCLANG_E2E_STORAGE_CARD" "$HPCLANG_E2E_REPLACEMENT_CARD"; do
        truncate -s 0 "$card"
        truncate -s 16M "$card"
        mkfs.fat -F 16 "$card" >/dev/null
    done
    mcopy -m -i "$HPCLANG_E2E_STORAGE_CARD" "$1/HPCLANG.TST" "$1/HPCLANG.OUT" ::/
    mmd -i "$HPCLANG_E2E_STORAGE_CARD" ::/ZZHPDIR
    printf 'SAFE' | mcopy -i "$HPCLANG_E2E_REPLACEMENT_CARD" - ::/STALE.BIN
    # Two independent FAT volumes in primary slots 1 and 3. The hole makes
    # dense clean-API indexes and historical partition-slot aliases differ.
    python3 - "$HPCLANG_E2E_STORAGE_CARD" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
volume = path.read_bytes()
sectors = len(volume) // 512
starts = (2048, 2048 + sectors)
mbr = bytearray(512)
for slot, start in zip((0, 2), starts):
    entry = 446 + slot * 16
    mbr[entry + 4] = 6  # FAT16
    struct.pack_into("<II", mbr, entry + 8, start, sectors)
mbr[510:512] = b"\x55\xaa"
with path.open("wb") as card:
    card.write(mbr)
    for start in starts:
        card.seek(start * 512)
        card.write(volume)
PY
}

verify_storage_media() {
    cmp -s <(printf 'SAFE') <(mtype -i "$HPCLANG_E2E_REPLACEMENT_CARD" ::/STALE.BIN)
    cmp -s <(printf 'NEW') <(mtype -i "$HPCLANG_E2E_REPLACEMENT_CARD" ::/NEW.BIN)
    printf 'PASS: replacement card retains its sentinel and newly written file\n'
}
