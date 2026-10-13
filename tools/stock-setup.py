#!/usr/bin/env python3
"""Validate and emit payload-defined Stock Kermit setup commands."""

import json
import os
from pathlib import Path
import sys


def load_commands(path: Path, family: str | None = None) -> list[str]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(value, dict) and type(value.get("version")) is int and value["version"] == 2:
        if set(value) != {"version", "families"} or not isinstance(value["families"], dict):
            raise ValueError("invalid Stock setup family fields")
        for name, commands in value["families"].items():
            if name not in ("hp39", "hp48", "hp4950"):
                raise ValueError("unsupported Stock setup family")
            validate_commands(commands)
        if family not in value["families"]:
            raise ValueError("Stock setup requires an explicit supported family")
        return value["families"][family]
    if not isinstance(value, dict) or set(value) != {"version", "commands"}:
        raise ValueError("invalid Stock setup fields")
    if type(value["version"]) is not int or value["version"] != 1:
        raise ValueError("unsupported Stock setup version")
    commands = value["commands"]
    validate_commands(commands)
    return commands


def validate_commands(commands: list[str]) -> None:
    if not isinstance(commands, list) or not 1 <= len(commands) <= 32:
        raise ValueError("Stock setup requires 1..32 commands")
    for command in commands:
        if (
            not isinstance(command, str)
            or not command.strip()
            or len(command) > 1024
            or any(ord(char) < 32 or ord(char) > 126 or char == "," for char in command)
        ):
            raise ValueError("Stock setup requires printable comma-free ASCII commands")


if __name__ == "__main__":
    print("\n".join(load_commands(Path(sys.argv[1]), os.environ.get("HPCLANG_E2E_FAMILY"))))
