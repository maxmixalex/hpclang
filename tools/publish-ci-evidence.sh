#!/usr/bin/env bash
set -euo pipefail

source_dir="$1"
branch="${2:-ci-evidence}"

test -d "$source_dir"

if ! find "$source_dir" -type f -print -quit | grep -q .; then
    echo "No CI evidence files to publish from $source_dir"
    exit 0
fi

if ! git lfs version >/dev/null 2>&1; then
    echo "git-lfs is required to publish CI evidence" >&2
    exit 1
fi
git lfs install --local >/dev/null

worktree="$(mktemp -d)"
trap 'git worktree remove --force "$worktree" >/dev/null 2>&1 || true; rm -rf "$worktree"' EXIT

git fetch origin "$branch"
# Existing evidence is immutable for this publication. Keep its LFS pointers
# instead of downloading every historical image; git add preserves untouched
# pointers and applies the LFS clean filter to newly copied images.
GIT_LFS_SKIP_SMUDGE=1 git worktree add --detach "$worktree" "origin/$branch" >/dev/null

cp -a "$source_dir"/. "$worktree"/

git -C "$worktree" config user.name "github-actions[bot]"
git -C "$worktree" config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git -C "$worktree" add --all

if git -C "$worktree" diff --cached --quiet; then
    echo "CI evidence is already up to date"
    exit 0
fi

run_label="${GITHUB_RUN_ID:-local}"
git -C "$worktree" commit -m "ci: publish evidence for run $run_label" >/dev/null

# Publishers are serialized by a shared Actions concurrency group. Keep a
# bounded fetch/rebase retry as a defensive fallback for manual writers or
# older workflow runs that still target the same branch.
for _ in 1 2 3; do
    if git -C "$worktree" push origin "HEAD:$branch"; then
        exit 0
    fi
    git -C "$worktree" fetch origin "$branch"
    GIT_LFS_SKIP_SMUDGE=1 git -C "$worktree" rebase "origin/$branch"
done

exit 1
