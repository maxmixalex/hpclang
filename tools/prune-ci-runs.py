#!/usr/bin/env python3
"""Keep only the newest completed GitHub Actions runs per workflow."""

from __future__ import annotations

import json
import os
import urllib.request
from collections import defaultdict

api = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")
repo = os.environ["GITHUB_REPOSITORY"]
token = os.environ["GITHUB_TOKEN"]
current_run = int(os.environ.get("GITHUB_RUN_ID", "0"))
keep = int(os.environ.get("KEEP_RUNS_PER_WORKFLOW", "20"))


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

by_workflow: dict[int, list[dict]] = defaultdict(list)
for run in runs:
    if run["status"] == "completed" and run["id"] != current_run:
        by_workflow[run["workflow_id"]].append(run)

deleted = 0
for workflow_id, items in sorted(by_workflow.items()):
    items.sort(
        key=lambda run: (run["created_at"], run["run_number"], run["id"]),
        reverse=True,
    )
    for run in items[keep:]:
        print(
            f"delete workflow={workflow_id} run={run['id']} "
            f"number={run['run_number']} conclusion={run['conclusion']}"
        )
        request(f"{api}/repos/{repo}/actions/runs/{run['id']}", method="DELETE")
        deleted += 1

print(f"kept up to {keep} completed runs per workflow; deleted {deleted}")
