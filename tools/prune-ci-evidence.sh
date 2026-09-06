#!/usr/bin/env bash
set -euo pipefail

keep="${KEEP_EVIDENCE_RUNS:-20}"

git lfs install --local >/dev/null
git fetch origin ci-evidence
start_remote="$(git rev-parse origin/ci-evidence)"

# Keep only CI images and the attributes file.
find . -type f ! -path './.git/*' \
  ! -name .gitattributes \
  ! -iname '*.png' ! -iname '*.jpg' ! -iname '*.jpeg' \
  ! -iname '*.webp' ! -iname '*.gif' -delete

# Keep the newest numeric run directories in each evidence family.
for root in */; do
    [ -d "$root" ] || continue
    mapfile -t runs < <(
        find "$root" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' |
        awk '/^[0-9]+$/' |
        sort -nr
    )
    for run in "${runs[@]:$keep}"; do
        rm -rf -- "$root$run"
    done
done

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

test -f .gitattributes
cp .gitattributes "$tmp/"

while IFS= read -r -d '' image; do
    rel="${image#./}"
    mkdir -p "$tmp/$(dirname "$rel")"
    cp "$image" "$tmp/$rel"
done < <(
    find . -type f ! -path './.git/*' \
      \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.jpeg' \
         -o -iname '*.webp' -o -iname '*.gif' \) -print0
)

git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"

git checkout --orphan ci-evidence-pruned
git rm -rf --ignore-unmatch . >/dev/null 2>&1 || true
find . -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -a "$tmp"/. .

git add .
git commit --allow-empty -m "ci: prune evidence history"
git lfs ls-files

git fetch origin ci-evidence
current_remote="$(git rev-parse origin/ci-evidence)"
if [ "$current_remote" != "$start_remote" ]; then
    echo "ci-evidence changed during maintenance; skip history rewrite"
    exit 0
fi

git push --force-with-lease="ci-evidence:$start_remote" origin HEAD:ci-evidence
