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


def bound_record(journal, authority, claim=None):
    image = pack.bind_image(
        bytes.fromhex("1eff2fe1") + bytes(12) + b"HPCBIND1" + bytes(8) + placeholder,
        16,
        journal,
        authority,
    )
    program = pack._recovery_program(pack.package(image, len(image), model="hp50g"))
    admission = image if claim is None else image + claim.to_bytes(4, "little")
    admission_program = pack._recovery_program(
        pack.package(admission, (len(admission) + 7) & ~7, model="hp50g")
    )
    library = pack._configuration_library(
        empty, 1701, (program,) + (empty,) * 5 + (admission_program,)
    )
    layout = pack.recovery_layout(
        library,
        (("recovery", 0, image, 16, False), ("admission", 6, admission, 16, False)),
        identity_offset=32,
        **({"creation_offset": len(image)} if claim is not None else {}),
    )
    library = pack.seal_recovery_identity(library, layout["identity_object_nibble"])
    data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in library[8:])
    span = 5 + int(data[5:10][::-1], 16)
    allocated = (span + 15) & ~7
    data = data[:span] + "f" * (allocated - span - 8)
    record = b"\x53" + ((allocated - 2) | 0x100000).to_bytes(3, "little")
    record += bytes(int(data[i], 16) | int(data[i + 1], 16) << 4 for i in range(0, len(data), 2))
    return record, layout


original, layout = bound_record(0x180105, 0x1A0105)
journal = 0x160105 + len(original)
rebound, updated = bound_record(journal, journal + 4124)
assert layout == updated and len(original) == len(rebound)
with Path(sys.argv[1]).open("a") as output:
    output.write("#ifdef HPCLANG_RECOVERY_BINDING_VECTORS\n")
    for name, data in (("binding_original", original), ("binding_expected", rebound)):
        output.write(
            f"static const unsigned char {name}[] = {{" + ",".join(map(str, data)) + "};\n"
        )
    for name in ("recovery_binding_nibble", "admission_binding_nibble", "identity_object_nibble"):
        output.write(f"static const unsigned {name} = {layout[name]};\n")
    output.write("#endif\n")

original, layout = bound_record(0x180105, 0x1A0105, 0x1E0105)
journal = 0x160105 + len(original)
authority = journal + 4124
claim = authority + 32
rebound, updated = bound_record(journal, authority, claim)
assert layout == updated and len(original) == len(rebound)
with Path(sys.argv[1]).open("a") as output:
    output.write("#ifdef HPCLANG_RECOVERY_BINDING_VECTORS\n")
    for name, data in (("creation_original", original), ("creation_expected", rebound)):
        output.write(
            f"static const unsigned char {name}[] = {{" + ",".join(map(str, data)) + "};\n"
        )
    for name in (
        "recovery_binding_nibble",
        "admission_binding_nibble",
        "identity_object_nibble",
        "admission_creation_nibble",
    ):
        output.write(f"static const unsigned creation_{name} = {layout[name]};\n")
    output.write("#endif\n")
