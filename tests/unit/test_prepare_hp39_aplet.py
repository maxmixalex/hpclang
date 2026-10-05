import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("prepare_aplet", ROOT / "tools/prepare-hp39-aplet.py")
APLET = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(APLET)


class ApletResetTests(unittest.TestCase):
    ORIGINAL = bytes.fromhex("742ab012030c0000c0a2020500c0000000742ab01203")

    def test_views_and_backward_link(self):
        data = APLET.reset_views_epilog(self.ORIGINAL)
        n = [v for b in data for v in (b & 15, b >> 4)]

        def read(offset, width=5):
            return sum(n[offset + i] << (4 * i) for i in range(width))

        def string(offset):
            self.assertEqual(read(offset), 0x2A2C)
            end = offset + 5 + read(offset + 5)
            value = bytes(read(i, 2) for i in range(offset + 10, end, 2))
            return value.decode("ascii"), end

        self.assertEqual(read(0), 0x2A74)
        cursor = 5
        for expected in (("Start", "", 15), ("Reset", ".HPCRESET", 10)):
            self.assertEqual(read(cursor), 0x2A74)
            prompt, cursor = string(cursor + 5)
            program, cursor = string(cursor)
            self.assertEqual(read(cursor), 0x2911)
            self.assertEqual((prompt, program, read(cursor + 5)), expected)
            self.assertEqual(read(cursor + 10), 0x312B)
            cursor += 15
        self.assertEqual(read(cursor), 0x312B)
        cursor += 5
        self.assertEqual(read(cursor), cursor + 2)
        original = [v for b in self.ORIGINAL for v in (b & 15, b >> 4)]
        self.assertEqual(n[cursor + 5 :], original[15:])

    def test_reject_unrecognized_template(self):
        for invalid in (
            b"",
            self.ORIGINAL[:-1],
            bytes(len(self.ORIGINAL)),
            APLET.reset_views_epilog(self.ORIGINAL),
        ):
            with self.assertRaises(ValueError):
                APLET.reset_views_epilog(invalid)

    def test_reset_program_balances_context(self):
        self.assertEqual(
            APLET.reset_program(),
            b"HP39AscD 9 .HPCRESETSYSEVAL 38496:SYSEVAL 75526:"
            b"SYSEVAL 38414:SYSEVAL 230997:SYSEVAL 38414:",
        )


if __name__ == "__main__":
    unittest.main()
