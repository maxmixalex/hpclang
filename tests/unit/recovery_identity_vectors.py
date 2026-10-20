"""Generate independent hashlib/native-CRC reference objects for the C reader."""

import importlib.util
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "recovery_packager", root / "tools/package-recovery.py"
)
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
placeholder = b"HPCROOT1" + bytes(40)
encoded = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in placeholder)
literal = pack.immediate(0x2A2C) + pack.immediate(101) + encoded
empty = pack.immediate(0x2D9D) + pack.immediate(0x312B)
rows = []
covered = set()
for padding in range(140):
    for wrapped in (False, True):
        command = pack.immediate(0x2D9D) + literal + pack.immediate(0x312B) if wrapped else literal
        tail = pack.immediate(0x2A2C) + pack.immediate(5 + padding) + "a" * padding
        original = pack._configuration_library(empty, 1701, (command, tail))
        text = "".join(f"{b & 15:x}{b >> 4:x}" for b in original[8:])
        at = text.index(encoded)
        span = 5 + int(text[5:10][::-1], 16)
        key = ((span + 1) // 2 % 64, at % 2, span % 2)
        if key[0] not in (0, 1, 55, 56, 63) or key in covered:
            continue
        covered.add(key)
        sealed = pack.seal_recovery_identity(original, at)[8:]
        nibbles = [n for byte in sealed for n in (byte & 15, byte >> 4)]
        descriptor = bytes(nibbles[at + i] | nibbles[at + i + 1] << 4 for i in range(0, 96, 2))
        rows.append(
            "{"
            + f"{span}, {at}, "
            + "{"
            + ",".join(map(str, sealed))
            + "},{"
            + ",".join(map(str, descriptor))
            + "}}"
        )
assert len(covered) == 20
Path(sys.argv[1]).write_text(
    "static const struct { unsigned span, at; unsigned char data[512], descriptor[48]; } "
    "vectors[] = {\n" + ",\n".join(rows) + "\n};\n"
)
