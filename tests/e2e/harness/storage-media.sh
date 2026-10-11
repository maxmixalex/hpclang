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
}

verify_storage_media() {
    cmp -s <(printf 'SAFE') <(mtype -i "$HPCLANG_E2E_REPLACEMENT_CARD" ::/STALE.BIN)
    cmp -s <(printf 'NEW') <(mtype -i "$HPCLANG_E2E_REPLACEMENT_CARD" ::/NEW.BIN)
    printf 'PASS: replacement card retains its sentinel and newly written file\n'
}
