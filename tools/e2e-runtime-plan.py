#!/usr/bin/env python3
"""Validate Runtime suites and expand their build and execution matrices."""

import json
from pathlib import Path
import re


def plan(suites, profiles):
    if not isinstance(suites, list) or not suites:
        raise ValueError("Expected a nonempty Runtime suite list")
    families = {profile["family"] for profile in profiles}
    result = {"builds": [], "direct": [], "stock": []}
    modes = set()
    for suite in suites:
        if not isinstance(suite, dict) or set(suite) - {
            "mode",
            "keyboard",
            "verifier",
            "contexts",
            "families",
            "build",
        }:
            raise ValueError("Unknown Runtime suite fields")
        mode = suite.get("mode")
        if not isinstance(mode, str) or not re.fullmatch(r"[a-z][a-z0-9-]*", mode):
            raise ValueError("Invalid suite mode")
        if mode in modes or type(suite.get("keyboard")) is not bool:
            raise ValueError("Duplicate mode or missing boolean keyboard policy")
        modes.add(mode)
        contexts = suite.get("contexts", ["direct", "stock"])
        selected_families = suite.get("families", sorted(families))
        for values, allowed in ((contexts, {"direct", "stock"}), (selected_families, families)):
            if (
                not isinstance(values, list)
                or not values
                or any(not isinstance(value, str) for value in values)
                or len(set(values)) != len(values)
                or not set(values) <= allowed
            ):
                raise ValueError("Invalid or duplicate contexts/families")
        build = suite.get("build", "universal")
        if build not in ("universal", "model"):
            raise ValueError("Build must be universal or model")
        verifier = suite.get("verifier", "")
        if not isinstance(verifier, str) or (
            verifier and not re.fullmatch(r"tests/e2e/[a-z0-9_-]+/verify\.py", verifier)
        ):
            raise ValueError("Invalid suite verifier path")
        selected = [
            p for p in profiles if p["family"] in selected_families and p.get("automated_e2e", True)
        ]
        jobs = []
        for context in contexts:
            seen = set()
            for profile in selected:
                if context == "stock" and not profile.get("automated_stock_e2e", True):
                    continue
                target = profile["target"] if context == "stock" or build == "model" else ""
                key = (profile["family"], target)
                if key in seen:
                    continue
                seen.add(key)
                artifact = f"{mode}-{profile['target']}" if build == "model" else mode
                job = {
                    "suite": {
                        "mode": mode,
                        "keyboard": suite["keyboard"],
                        "verifier": verifier,
                        "artifact": artifact,
                    },
                    "model": {"family": profile["family"], "target": target},
                    "family": profile["family"],
                    "label": target or profile["family"],
                }
                result[context].append(job)
                jobs.append((context, job))
            if not seen:
                raise ValueError(f"{mode}: no eligible {context} targets")
        artifacts = {}
        for context, job in jobs:
            artifact = job["suite"]["artifact"]
            if artifact not in artifacts:
                artifacts[artifact] = {
                    "mode": mode,
                    "artifact": artifact,
                    "family": job["family"] if build == "model" else "all",
                    "model": job["model"]["target"] if build == "model" else "universal",
                    "stock": False,
                }
            artifacts[artifact]["stock"] |= context == "stock"
        if set(artifacts) & {item["artifact"] for item in result["builds"]}:
            raise ValueError("Runtime suites produce colliding artifact names")
        result["builds"].extend(artifacts.values())
    return result


def main():
    root = Path(__file__).resolve().parents[1]
    suites = json.loads((root / "tests/e2e/runtime-suites.json").read_text())
    profiles = [
        json.loads(path.read_text()) for path in sorted((root / "tests/e2e/targets").glob("*.json"))
    ]
    print(json.dumps(plan(suites, profiles)))


if __name__ == "__main__":
    main()
