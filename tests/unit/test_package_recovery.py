import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "package_recovery", ROOT / "tools/package-recovery.py"
)
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class RecoveryPackageTests(unittest.TestCase):
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
