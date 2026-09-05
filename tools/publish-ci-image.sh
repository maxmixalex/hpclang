#!/usr/bin/env bash
set -euo pipefail

image="$1"
destination="$2"
branch="\${3:-ci-evidence}"

test -f "$image"

worktree="$(mktemp -d)"
trap 'git worktree remove --force "$worktree" >/dev/null 2>&1 || true; rm -rf "$worktree"' EXIT

git fetch origin "$branch"
git worktree add --detach "$worktree" "origin/$branch" >/dev/null

mkdir -p "$worktree/$(dirname "$destination")"
cp "$image" "$worktree/$destination"

git -C "$worktree" config user.name "github-actions[bot]"
git -C "$worktree" config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git -C "$worktree" add "$destination"

if git -C "$worktree" diff --cached --quiet; then
  exit 0
fi

git -C "$worktree" commit -m "ci: publish $destination" >/dev/null

while ! git -C "$worktree" push origin "HEAD:$branch"; do
  git -C "$worktree" fetch origin "$branch"
  git -C "$worktree" rebase "origin/$branch"
done
