import importlib.util
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "package_recovery", ROOT / "tools/package-recovery.py"
)
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class RecoveryPackageTests(unittest.TestCase):
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
