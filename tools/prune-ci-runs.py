#!/usr/bin/env python3
"""Keep only the newest completed GitHub Actions runs per workflow."""

from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

api = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")
repo = os.environ["GITHUB_REPOSITORY"]
token = os.environ["GITHUB_TOKEN"]
current_run = int(os.environ.get("GITHUB_RUN_ID", "0"))
keep = int(os.environ.get("KEEP_RUNS_PER_WORKFLOW", "20"))
inactive_keep = int(os.environ.get("KEEP_INACTIVE_RUNS", "2"))


def request(url: str, method: str = "GET") -> dict | None:
    req = urllib.request.Request(
        url,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "hpclang-ci-maintenance",
        },
    )
    with urllib.request.urlopen(req) as response:
        data = response.read()
    return json.loads(data) if data else None


runs: list[dict] = []
for page in range(1, 101):
    data = request(f"{api}/repos/{repo}/actions/runs?per_page=100&page={page}")
    batch = data["workflow_runs"]
    runs.extend(batch)
    if len(batch) < 100:
        break

workflow_entries = request(
    f"{api}/repos/{repo}/contents/.github/workflows?ref=main"
)
current_paths = {
    item["path"]
    for item in workflow_entries
    if item.get("type") == "file"
}

by_workflow: dict[int, list[dict]] = defaultdict(list)
for run in runs:
    if run["status"] == "completed" and run["id"] != current_run:
        by_workflow[run["workflow_id"]].append(run)

victims: list[dict] = []
for workflow_id, items in sorted(by_workflow.items()):
    items.sort(
        key=lambda run: (run["created_at"], run["run_number"], run["id"]),
        reverse=True,
    )
    is_current = any(run["path"] in current_paths for run in items)
    limit = keep if is_current else inactive_keep
    victims.extend(items[limit:])


def delete_run(run: dict) -> int:
    try:
        request(f"{api}/repos/{repo}/actions/runs/{run['id']}", method="DELETE")
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return 0
        raise
    print(
        f"deleted run={run['id']} workflow={run['workflow_id']} "
        f"number={run['run_number']} conclusion={run['conclusion']}"
    )
    return 1


deleted = 0
with ThreadPoolExecutor(max_workers=8) as pool:
    futures = [pool.submit(delete_run, run) for run in victims]
    for future in as_completed(futures):
        deleted += future.result()

print(
    f"kept up to {keep} runs for current workflows and "
    f"{inactive_keep} for removed workflows; deleted {deleted}"
)
