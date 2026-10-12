#!/usr/bin/env python3
"""Validate and emit payload-defined Stock Kermit setup commands."""

import json
from pathlib import Path
import sys


def load_commands(path: Path) -> list[str]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or set(value) != {"version", "commands"}:
        raise ValueError("invalid Stock setup fields")
    if type(value["version"]) is not int or value["version"] != 1:
        raise ValueError("unsupported Stock setup version")
    commands = value["commands"]
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
    return commands


if __name__ == "__main__":
    print("\n".join(load_commands(Path(sys.argv[1]))))
