import importlib.util
import hashlib
import json
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "package_recovery", ROOT / "tools/package-recovery.py"
)
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class RecoveryPackageTests(unittest.TestCase):
    def test_library_identity_seal_has_no_recursive_digest_dependency(self):
        placeholder = b"HPCROOT1" + bytes(40)
        payload = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in placeholder)
        literal = PACK.immediate(0x2A2C) + PACK.immediate(5 + len(payload)) + payload
        empty = PACK.immediate(0x2D9D) + PACK.immediate(0x312B)
        alignments, lengths = set(), set()
        for wrapped, extra in ((False, False), (True, False), (False, True)):
            command = (
                PACK.immediate(0x2D9D) + literal + PACK.immediate(0x312B) if wrapped else literal
            )
            original = PACK._configuration_library(
                empty, 1701, (command, empty) if extra else (command,)
            )
            original_nibbles = [n for byte in original[8:] for n in (byte & 15, byte >> 4)]
            # This synthetic fixture contains exactly one explicitly constructed
            # payload. Production callers must use owned ELF/command offsets.
            offset = "".join(format(n, "x") for n in original_nibbles).index(payload)
            sealed = PACK.seal_recovery_identity(original, offset)
            data = [n for byte in sealed[8:] for n in (byte & 15, byte >> 4)]
            span = 5 + sum(data[5 + i] << (4 * i) for i in range(5))
            alignments.add(offset % 2)
            lengths.add(span % 2)
            descriptor = bytes(
                data[offset + i] | data[offset + i + 1] << 4 for i in range(0, 96, 2)
            )
            self.assertEqual(descriptor[:8], b"HPCROOT1")
            self.assertEqual(int.from_bytes(descriptor[8:12], "little"), span)
            self.assertEqual(int.from_bytes(descriptor[12:16], "little"), offset)
            canonical = bytearray((span + 1) // 2)
            for index in range(span):
                value = (
                    0 if offset + 32 <= index < offset + 96 or index >= span - 4 else data[index]
                )
                canonical[index // 2] |= value << (4 * (index % 2))
                if not (offset + 16 <= index < offset + 96 or index >= span - 4):
                    self.assertEqual(data[index], original_nibbles[index])
            self.assertEqual(descriptor[16:], hashlib.sha256(canonical).digest())
            crc = 0
            for nibble in data[5 : span - 4]:
                for bit in range(4):
                    carry = (crc ^ (nibble >> bit)) & 1
                    crc >>= 1
                    if carry:
                        crc ^= 0x8408
            self.assertEqual(sum(data[span - 4 + i] << (4 * i) for i in range(4)), crc)
            self.assertEqual(sealed, PACK.seal_recovery_identity(original, offset))
            for bad_offset in (-1, True, offset + 1, span - 4):
                with self.assertRaises(ValueError):
                    PACK.seal_recovery_identity(original, bad_offset)
            for invalid in (sealed, original[:-1], original + b"\0", b"bad"):
                with self.assertRaises(ValueError):
                    PACK.seal_recovery_identity(invalid, offset)
            changed = bytearray(original)
            changed[-2] ^= 1
            with self.assertRaisesRegex(ValueError, "CRC"):
                PACK.seal_recovery_identity(bytes(changed), offset)
        self.assertEqual(alignments, {0, 1})
        self.assertEqual(lengths, {0, 1})

    def test_bound_cli_packages_both_elfs_before_publishing_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery, predicate = root / "recovery.elf", root / "admission.elf"
            output, binary, program = (
                root / name for name in ("library.hp", "image.bin", "code.hp")
            )
            layout_path = root / "layout.json"
            image = PACK.bind_image(bytes(32) + b"HPCBIND1" + bytes(24), 32, 0x160105, 0x161121)
            predicate_image = PACK.bind_image(b"HPCBIND1" + bytes(56), 0, 0x1A0105, 0x1A1121)
            valid_predicate = True
            identity_offset = None

            def listing(command, **kwargs):
                if command[0] == "readelf":
                    return "R_ARM_REL32"
                offset = 32 if command[-1] == recovery else (0 if valid_predicate else 64)
                identity_symbol = (
                    f"{identity_offset:08x} D hpclang_recovery_identity\n"
                    if command[-1] == predicate and identity_offset is not None
                    else ""
                )
                return (
                    "00000000 T entry\n00000040 B __zero_start\n00000080 B __image_end\n"
                    f"{offset:08x} D hpclang_recovery_binding\n" + identity_symbol
                )

            def extract(command, **kwargs):
                Path(command[-1]).write_bytes(image if command[-2] == recovery else predicate_image)

            argv = [
                "pack",
                "--elf",
                "--model",
                "hp50g",
                "--flash-copy",
                "--library-id",
                "1701",
                "--journal-slot",
                "0x180105",
                "--authority-slot",
                "0x181121",
                "--reservation-name",
                "RESERVE",
                "--reservation-bytes",
                "8192",
                "--reservation-prefix",
                "owned",
                "--admission-elf",
                str(predicate),
                "--binary-output",
                str(binary),
                "--program-output",
                str(program),
                "--layout-output",
                str(layout_path),
                "--nm",
                "nm",
                "--readelf",
                "readelf",
                "--objcopy",
                "objcopy",
                str(recovery),
                str(output),
            ]
            with (
                patch.object(sys, "argv", argv),
                patch.object(PACK.subprocess, "check_output", side_effect=listing),
                patch.object(PACK.subprocess, "run", side_effect=extract),
            ):
                PACK.main()
                expected = PACK.bind_image(image, 32, 0x180105, 0x181121)
                self.assertEqual(binary.read_bytes(), expected)
                recovery_program = PACK.package(expected, 128, model="hp50g", fresh_copy=True)
                self.assertEqual(program.read_bytes(), recovery_program)
                admission = PACK.package(
                    PACK.bind_image(predicate_image, 0, 0x180105, 0x181121),
                    128,
                    model="hp50g",
                    fresh_copy=True,
                )
                self.assertEqual(
                    output.read_bytes(),
                    PACK.guarded_configuration_library(
                        recovery_program,
                        1701,
                        name="RESERVE",
                        capacity=8192,
                        prefix=b"owned",
                        admission=admission,
                    ),
                )
                layout = json.loads(layout_path.read_text())
                self.assertEqual((layout["version"], layout["library_id"]), (1, 1701))
                packed = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in output.read_bytes()[8:])
                span = 5 + int(packed[5:10][::-1], 16)
                self.assertEqual(layout["record_bytes"], ((span + 15) & ~7) // 2)
                bound = b"HPCBIND1" + bytes.fromhex("0501180021111800")
                descriptor = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in bound)
                for name in ("recovery", "admission"):
                    offset = layout[name + "_binding_nibble"] - 8
                    self.assertEqual(packed[offset : offset + 32], descriptor)
                self.assertNotEqual(
                    layout["recovery_binding_nibble"], layout["admission_binding_nibble"]
                )
                argv.remove("--flash-copy")
                PACK.main()
                reusable = json.loads(layout_path.read_text())
                packed = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in output.read_bytes()[8:])
                for name in ("recovery", "admission"):
                    offset = reusable[name + "_binding_nibble"] - 8
                    self.assertEqual(packed[offset : offset + 32], descriptor)
                self.assertNotEqual(
                    reusable["recovery_binding_nibble"], layout["recovery_binding_nibble"]
                )
                identity_offset = 16
                predicate_image = predicate_image[:16] + b"HPCROOT1" + bytes(40)
                PACK.main()
                sealed = output.read_bytes()
                owned_layout = json.loads(layout_path.read_text())
                identity_at = owned_layout["identity_object_nibble"]
                values = [n for byte in sealed[8:] for n in (byte & 15, byte >> 4)]
                identity = bytes(
                    values[identity_at + i] | values[identity_at + i + 1] << 4
                    for i in range(0, 96, 2)
                )
                self.assertEqual(identity[:8], b"HPCROOT1")
                self.assertEqual(int.from_bytes(identity[12:16], "little"), identity_at)
                self.assertNotEqual(identity[16:], bytes(32))
                # Sealing is required by the image symbol even without a JSON
                # sidecar. It must not silently leave an executable placeholder.
                layout_option = argv.index("--layout-output")
                del argv[layout_option : layout_option + 2]
                PACK.main()
                self.assertEqual(output.read_bytes(), sealed)
                argv[layout_option:layout_option] = ["--layout-output", str(layout_path)]
                for bad_identity_offset in (8, 18, 64):
                    identity_offset = bad_identity_offset
                    for path in (output, binary, program, layout_path):
                        path.write_bytes(b"retained")
                    with self.assertRaisesRegex(ValueError, "owned initialized placeholder"):
                        PACK.main()
                    self.assertTrue(
                        all(
                            path.read_bytes() == b"retained"
                            for path in (output, binary, program, layout_path)
                        )
                    )
                identity_offset = 16
                for path in (output, binary, program, layout_path):
                    path.write_bytes(b"retained")
                valid_predicate = False
                with self.assertRaisesRegex(ValueError, "outside initialized"):
                    PACK.main()
                self.assertTrue(
                    all(
                        path.read_bytes() == b"retained"
                        for path in (output, binary, program, layout_path)
                    )
                )

    def test_admission_precedes_the_complete_reservation_guard(self):
        recovery = PACK.package(bytes(range(64)), 4096, model="hp50g")
        admission = PACK.package(bytes(range(8)), 64, model="hp50g")
        options = dict(name="RESERVE", capacity=8192, prefix=b"bound")
        plain = PACK.guarded_configuration_library(recovery, 1701, **options)
        guarded = PACK.guarded_configuration_library(recovery, 1701, **options, admission=admission)

        def parts(package):
            data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in package[8:])

            def word(at, width=5):
                return int(data[at : at + width][::-1], 16)

            fields = 17 + 2 * word(10, 2)
            links = fields + 10 + word(fields + 10)
            count = (word(links + 5) - 5) // 5
            config = fields + 15 + word(fields + 15)
            starts = [links + 10 + 5 * i + word(links + 10 + 5 * i) for i in range(count)]
            return [data[a:b] for a, b in zip(starts, starts[1:] + [config])], data[
                config : 5 + word(5) - 4
            ]

        original, configuration = parts(plain)
        commands, conditional = parts(guarded)
        self.assertEqual(commands[:-1], original)
        self.assertEqual(commands[-1], PACK._recovery_program(admission))
        op = PACK.immediate
        prefix = op(0x2D9D) + op(0x389B9) + op(0x37F48)
        prefix += op(0x2E92) + op(1701, 3) + op(6, 3) + op(0x37F7F)
        self.assertEqual(
            conditional, prefix + configuration + op(0x3807D) + op(0x389D4) + op(0x312B)
        )
        with self.assertRaises(ValueError):
            PACK.guarded_configuration_library(recovery, 1701, **options, admission=b"not code")

    def test_elf_validates_compiled_binding_and_preserves_unbound_images(self):
        with tempfile.TemporaryDirectory() as temporary:
            elf = Path(temporary) / "recovery.elf"
            options = dict(model="hp50g", nm="nm", readelf="readelf", objcopy="objcopy")
            symbols = "00000000 T entry\n00000040 B __zero_start\n00000080 B __image_end\n"
            descriptor = "00000020 D hpclang_recovery_binding\n"
            image = bytes(32) + b"HPCBIND1" + bytes(8) + bytes(16)

            def extract(command, **kwargs):
                Path(command[-1]).write_bytes(image)

            def package(binding_symbols, **kwargs):
                with (
                    patch.object(
                        PACK.subprocess,
                        "check_output",
                        side_effect=["R_ARM_REL32", symbols + binding_symbols],
                    ),
                    patch.object(PACK.subprocess, "run", side_effect=extract),
                ):
                    return PACK.package_elf(elf, **options, **kwargs)

            with self.assertRaisesRegex(ValueError, "user Flash"):
                package(descriptor)
            image = PACK.bind_image(image, 32, 0x160105, 0x181005)
            self.assertEqual(package(descriptor)[1], image)
            rebound = package(descriptor, binding=(0x180105, 0x181121))[1]
            self.assertEqual(int.from_bytes(rebound[40:44], "little"), 0x180105)
            self.assertEqual(package("")[1], image)
            with self.assertRaisesRegex(ValueError, "no installation binding"):
                package("", binding=(0x180105, 0x181121))
            with self.assertRaisesRegex(ValueError, "outside initialized"):
                package("00000040 D hpclang_recovery_binding\n")
            image = image[:16] + b"\xff" * 4 + image[20:]
            owned_id = descriptor + "00000010 D hpclang_recovery_library_id\n"
            self.assertEqual(package(owned_id)[1], image)
            for number in (769, 1701, 1791):
                envelope, initialized = package(owned_id, library_id=number)
                expected = PACK.bind_library_id(image, 16, number)
                self.assertEqual(initialized, expected)
                self.assertEqual(envelope, PACK.package(expected, 128, model="hp50g"))
            with self.assertRaisesRegex(ValueError, "placeholder"):
                package(descriptor + "00000014 D hpclang_recovery_library_id\n", library_id=1701)

    def test_library_number_uses_only_owned_placeholder(self):
        image = bytes(range(16)) + b"\xff" * 4 + bytes(range(16))
        self.assertEqual(PACK.bind_library_id(image, 16, None), image)
        for number in (769, 1701, 1791):
            patched = PACK.bind_library_id(image, 16, number)
            self.assertEqual(patched[:16], image[:16])
            self.assertEqual(patched[20:], image[20:])
            self.assertEqual(int.from_bytes(patched[16:20], "little"), number)
        for number in (0, 768, 1792, True, 1701.0):
            with self.subTest(number=number), self.assertRaises(ValueError):
                PACK.bind_library_id(image, 16, number)
        for offset in (-4, 1, 36, True):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                PACK.bind_library_id(image, offset, 1701)
        with self.assertRaisesRegex(ValueError, "placeholder"):
            PACK.bind_library_id(image, 0, 1701)

    def test_binding_changes_only_reserved_address_fields(self):
        image = bytes(range(32)) + b"HPCBIND1" + bytes(8) + bytes(range(64))
        patched = PACK.bind_image(image, 32, 0x160105, 0x181005)
        self.assertEqual(patched[:40], image[:40])
        self.assertEqual(patched[48:], image[48:])
        self.assertEqual(int.from_bytes(patched[40:44], "little"), 0x160105)
        self.assertEqual(int.from_bytes(patched[44:48], "little"), 0x181005)
        self.assertEqual(image[40:48], bytes(8))

    def test_binding_rejects_unowned_layout_and_invalid_geometry(self):
        image = b"HPCBIND1" + bytes(8)
        for offset in (-4, 1, 4, True, 0.0):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                PACK.bind_image(image, offset, 0x160105, 0x181005)
        with self.assertRaisesRegex(ValueError, "format"):
            PACK.bind_image(b"notowned" + bytes(8), 0, 0x160105, 0x181005)
        for journal, authority in (
            (0x140000, 0x181005),
            (0x161105, 0x161105),
            (0x17FFF1, 0x181005),
            (0x160105, 0x180101),
            (0x160105, 0x1FFFF1),
            (0x160106, 0x181005),
            (True, 0x181005),
            (0x160105, 0x200105),
        ):
            with self.subTest(journal=journal, authority=authority), self.assertRaises(ValueError):
                PACK.bind_image(image, 0, journal, authority)

    def test_binding_cli_rejects_partial_or_raw_mode_without_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            source, output = Path(temporary) / "input", Path(temporary) / "output"
            source.write_bytes(b"HPCBIND1" + bytes(8))
            output.write_bytes(b"retained")
            for options in (
                ("--elf", "--journal-slot", "0x160105"),
                ("--elf", "--authority-slot", "0x161121"),
                (
                    "--runtime-size",
                    "64",
                    "--journal-slot",
                    "0x160105",
                    "--authority-slot",
                    "0x161121",
                ),
            ):
                result = subprocess.run(
                    [
                        sys.executable,
                        str(ROOT / "tools/package-recovery.py"),
                        "--model",
                        "hp50g",
                        "--library-id",
                        "1701",
                        *options,
                        str(source),
                        str(output),
                    ],
                    capture_output=True,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(output.read_bytes(), b"retained")

    def test_guarded_library_keeps_recovery_outside_small_configuration(self):
        program = PACK.package(bytes(range(64)), 4096, model="hp50g", fresh_copy=True)
        for library_id, name, prefix in ((769, "R", b"\x00\xffroot"), (1791, "HPCRES", b"owned:1")):
            with self.subTest(library_id=library_id):
                raw = PACK.guarded_configuration_library(
                    program, library_id, name=name, capacity=8192, prefix=prefix
                )
                data = [n for byte in raw[8:] for n in (byte & 15, byte >> 4)]

                def word(at, width=5):
                    return sum(data[at + i] << (4 * i) for i in range(width))

                self.assertEqual(word(0), 0x2B40)
                end = 5 + word(5)
                title = word(10, 2)
                header = 14 + 2 * title
                self.assertEqual(word(header, 3), library_id)
                fields = header + 3
                self.assertEqual((word(fields), word(fields + 5)), (0, 0))
                links = fields + 10 + word(fields + 10)
                configuration = fields + 15 + word(fields + 15)
                self.assertEqual(word(links), 0x2A4E)
                self.assertEqual(word(links + 5), 5 + 6 * 5)
                starts = [links + 10 + 5 * i + word(links + 10 + 5 * i) for i in range(6)]
                self.assertEqual(starts, sorted(set(starts)))
                self.assertEqual(starts[0], links + 10 + 6 * 5)
                self.assertLess(starts[-1], configuration)
                self.assertLess(end - 4 - configuration, 256)
                original = [n for byte in program[8:] for n in (byte & 15, byte >> 4)]
                extent = sum(original[10 + i] << (4 * i) for i in range(5))
                self.assertEqual(data[starts[0] : starts[1]], original[: 15 + extent])
                for start, stop in zip(starts, starts[1:] + [configuration]):
                    self.assertEqual(word(start), 0x2D9D)
                    self.assertEqual(word(stop - 5), 0x312B)
                # Configuration has its own program boundary; the full image
                # occupies a distinct command instead of inflating this body.
                self.assertEqual(word(configuration), 0x2D9D)
                self.assertEqual(word(end - 9), 0x312B)
                crc = 0
                for value in data[5 : end - 4]:
                    for bit in range(4):
                        carry = (crc ^ (value >> bit)) & 1
                        crc >>= 1
                        if carry:
                            crc ^= 0x8408
                self.assertEqual(word(end - 4, 4), crc)
                self.assertIn(data[end:], ([], [0]))

    def test_guard_rejects_invalid_reservation_metadata(self):
        program = PACK.package(bytes(range(8)), 64, model="hp50g", fresh_copy=True)
        defaults = dict(name="HPCRES", capacity=4096, prefix=b"owned")
        cases = [dict(name=n) for n in ("", "has space", "\0", "R" * 128, "\u00e9")]
        cases += [dict(capacity=n) for n in (0, 1, -1, True, 4096.0, 262144)]
        cases += [dict(prefix=p) for p in (b"", "owned", b"x" * 4097)]
        for change in cases:
            with self.subTest(change=change):
                with self.assertRaises(ValueError):
                    PACK.guarded_configuration_library(program, 1701, **(defaults | change))

    def test_guard_cli_requires_complete_metadata_without_partial_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output, companion = (root / name for name in ("in.bin", "out.hp", "program.hp"))
            source.write_bytes(bytes(range(8)))
            output.write_bytes(b"old output")
            companion.write_bytes(b"old program")
            command = [
                sys.executable,
                str(ROOT / "tools/package-recovery.py"),
                "--model",
                "hp50g",
                "--runtime-size",
                "64",
                "--program-output",
                str(companion),
            ]
            options = [
                "--reservation-name",
                "HPCRES",
                "--reservation-bytes",
                "4096",
                "--reservation-prefix",
                "owned",
            ]
            too_small = options[:]
            too_small[3] = "1"
            for invalid in (
                options,
                ["--library-id", "1701", *options[:-2]],
                ["--library-id", "1701", *too_small],
            ):
                result = subprocess.run(
                    command + invalid + [str(source), str(output)], capture_output=True
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(output.read_bytes(), b"old output")
                self.assertEqual(companion.read_bytes(), b"old program")
            subprocess.run(
                command
                + ["--library-id", "1701", "--flash-copy", *options, str(source), str(output)],
                check=True,
                capture_output=True,
            )
            self.assertTrue(output.read_bytes().startswith(b"HPHP49-C"))
            self.assertGreater(len(output.read_bytes()), len(companion.read_bytes()))

    def test_fresh_flash_copy_alignment_and_lifetime(self):
        for model in ("hp49gplus", "hp50g"):
            for size in (1, 8, 9, 24, 4096, 32768):
                payload = bytes(i % 251 for i in range(size))
                initialized = (size + 7) & ~7
                runtime = initialized + 8192
                raw = PACK.package(payload, runtime, model=model, fresh_copy=True)
                ordinary = PACK.package(payload, runtime, model=model)
                self.assertEqual(len(ordinary) - len(raw), initialized)
                data = [n for byte in raw[8:] for n in (byte & 15, byte >> 4)]
                end = 10 + sum(data[10 + i] << (4 * i) for i in range(5))
                source = end - runtime * 2
                expected = [
                    n
                    for byte in payload + bytes(initialized - size)
                    for n in (byte & 15, byte >> 4)
                ]
                for phase in range(16):
                    with self.subTest(model=model, size=size, phase=phase):
                        # Each invocation starts from immutable Flash, not the
                        # modified object left by an earlier RAM invocation.
                        for _ in range(2):
                            memory = [15] * phase + data[:]
                            src = phase + source
                            dst = src & ~15
                            self.assertGreaterEqual(dst, phase + source - 15)
                            self.assertLessEqual(dst + runtime * 2, phase + end)
                            prefix, suffix = memory[:dst], memory[phase + end :]
                            for offset in range(0, initialized * 2, 16):
                                chunk = memory[src + offset : src + offset + 16]
                                memory[dst + offset : dst + offset + 16] = chunk
                            self.assertEqual(memory[dst : dst + initialized * 2], expected)
                            memory[dst : dst + runtime * 2] = [15] * (runtime * 2)
                            self.assertEqual(memory[:dst], prefix)
                            self.assertEqual(memory[phase + end :], suffix)

    def test_flash_cli_requires_library_and_separate_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output, program = (
                root / name for name in ("image.bin", "library.hp", "program.hp")
            )
            source.write_bytes(bytes(range(8)))
            command = [
                sys.executable,
                str(ROOT / "tools/package-recovery.py"),
                "--model",
                "hp50g",
                "--runtime-size",
                "32",
            ]
            for options in (
                ["--flash-copy"],
                ["--program-output", str(program)],
                ["--library-id", "1701", "--program-output", str(source)],
            ):
                result = subprocess.run(
                    command + options + [str(source), str(output)], capture_output=True
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(source.read_bytes(), bytes(range(8)))
                self.assertFalse(output.exists())
            subprocess.run(
                command
                + [
                    "--flash-copy",
                    "--library-id",
                    "1701",
                    "--program-output",
                    str(program),
                    str(source),
                    str(output),
                ],
                check=True,
                capture_output=True,
            )
            expected = PACK.package(source.read_bytes(), 32, model="hp50g", fresh_copy=True)
            self.assertEqual(program.read_bytes(), expected)
            self.assertEqual(output.read_bytes(), PACK.configuration_library(expected, 1701))

    def test_configuration_library_fields_payload_and_crc(self):
        program = PACK.package(bytes(range(8)), 32, model="hp50g")
        raw = PACK.configuration_library(program, 1701)
        data = [n for byte in raw[8:] for n in (byte & 15, byte >> 4)]

        def word(at, width=5):
            return sum(data[at + i] << (4 * i) for i in range(width))

        self.assertEqual(raw[:8], b"HPHP49-C")
        self.assertEqual(word(0), 0x2B40)
        end = 5 + word(5)
        title_length = word(10, 2)
        title = bytes(word(12 + 2 * i, 2) for i in range(title_length))
        self.assertEqual(title, b"HPCRecovery")
        suffix = 12 + title_length * 2
        self.assertEqual(word(suffix, 2), title_length)
        self.assertEqual(word(suffix + 2, 3), 1701)
        fields = suffix + 5
        self.assertEqual([word(fields + 5 * i) for i in range(3)], [0, 0, 0])
        configuration = fields + 15 + word(fields + 15)
        original = [n for byte in program[8:] for n in (byte & 15, byte >> 4)]
        extent = sum(original[10 + i] << (4 * i) for i in range(5))
        self.assertEqual(data[configuration : end - 4], original[: 15 + extent])
        # Independent bit-at-a-time reflected CRC-16, rather than the nibble formula.
        crc = 0
        for value in data[5 : end - 4]:
            for bit in range(4):
                carry = (crc ^ (value >> bit)) & 1
                crc >>= 1
                if carry:
                    crc ^= 0x8408
        self.assertEqual(word(end - 4, 4), crc)
        self.assertIn(data[end:], ([], [0]))

    def test_configuration_library_rejects_invalid_ids_and_programs(self):
        program = PACK.package(bytes(range(8)), 32, model="hp50g")
        for library_id in (0, 768, 1792, 4095, True):
            with self.assertRaises(ValueError):
                PACK.configuration_library(program, library_id)
        for malformed in (
            b"",
            program[:10],
            b"HPHP48-C" + program[8:],
            program[:-2],
            program + b"\0",
        ):
            with self.assertRaises(ValueError):
                PACK.configuration_library(malformed, 1701)
        for library_id in (769, 1791):
            self.assertTrue(PACK.configuration_library(program, library_id))

    def test_object_extent_payload_and_owned_storage(self):
        for size in (1, 8, 9, 24, 4096, 32768):
            with self.subTest(size=size):
                payload = bytes(i % 251 for i in range(size))
                initialized = (size + 7) & ~7
                runtime = initialized + 8192
                raw = PACK.package(payload, runtime, model="hp50g")
                self.assertEqual(raw[:8], b"HPHP49-C")
                nibbles = [n for byte in raw[8:] for n in (byte & 15, byte >> 4)]

                def word(offset):
                    return sum(nibbles[offset + i] << (4 * i) for i in range(5))

                self.assertEqual(word(0), 0x2D9D)
                self.assertEqual(word(5), 0x2DCC)
                end = 10 + word(10)
                self.assertEqual(word(end), 0x312B)
                self.assertLessEqual(len(nibbles) - end - 5, 1)
                # Extract from the tail independently of encoded instruction length.
                reserve = runtime * 2 + 15
                source = end - reserve - initialized * 2
                copied = bytes(
                    nibbles[i] | (nibbles[i + 1] << 4)
                    for i in range(source, source + initialized * 2, 2)
                )
                self.assertEqual(copied, payload + bytes(initialized - size))
                self.assertEqual(nibbles[end - reserve : end], [0] * reserve)
                for phase in range(16):
                    reserved_start = phase + end - reserve
                    aligned = (reserved_start + 15) & ~15
                    self.assertGreaterEqual(aligned, reserved_start)
                    self.assertLessEqual(aligned + runtime * 2, phase + end)
                    memory = [15] * phase + nibbles[:]
                    absolute_source = phase + source
                    prefix = memory[:reserved_start]
                    expected = memory[absolute_source : absolute_source + initialized * 2]
                    # Reusing a saved CODE object must reload the original image,
                    # even after its previous invocation changed all runtime data.
                    for _ in range(2):
                        for offset in range(0, initialized * 2, 16):
                            chunk = memory[absolute_source + offset : absolute_source + offset + 16]
                            memory[aligned + offset : aligned + offset + 16] = chunk
                        self.assertEqual(memory[aligned : aligned + initialized * 2], expected)
                        self.assertEqual(memory[:reserved_start], prefix)
                        memory[aligned : aligned + runtime * 2] = [15] * (runtime * 2)

    def test_invalid_inputs(self):
        for payload, runtime, model in (
            (b"", 8, "hp50g"),
            (bytes(32769), 32776, "hp50g"),
            (bytes(9), 8, "hp50g"),
            (bytes(8), 9, "hp50g"),
            (bytes(8), -8, "hp50g"),
            (bytes(8), 0x80000, "hp50g"),
            (bytes(8), 8, "hp48gii"),
        ):
            with (
                self.subTest(runtime=runtime, model=model),
                self.assertRaises(ValueError),
            ):
                PACK.package(payload, runtime, model=model)

    def test_hp4950_models_share_the_validated_envelope(self):
        payload = bytes(range(32))
        self.assertEqual(
            PACK.package(payload, 4096, model="hp49gplus"),
            PACK.package(payload, 4096, model="hp50g"),
        )

    def test_complete_program_must_fit_main_ram_window(self):
        # Measure the serialized overhead independently, including both copies
        # of initialized data and the surrounding RPL objects. The HP transfer
        # header is not calculator memory. Passing this ceiling is not proof of
        # enough free RAM: the OS and other objects also occupy this window.
        window_bytes = 256 * 1024
        for model in ("hp49gplus", "hp50g"):
            for size in (1, 8, 32768):
                with self.subTest(model=model, initialized=size):
                    payload = bytes(size)
                    runtime = (size + 7) & ~7
                    baseline = PACK.package(payload, runtime, model=model)
                    largest = runtime + ((window_bytes - (len(baseline) - 8)) // 8) * 8
                    accepted = PACK.package(payload, largest, model=model)
                    self.assertLessEqual(len(accepted) - 8, window_bytes)
                    self.assertGreater(len(accepted) - 8 + 8, window_bytes)
                    with self.assertRaisesRegex(ValueError, "main-RAM window"):
                        PACK.package(payload, largest + 8, model=model)


if __name__ == "__main__":
    unittest.main()
